#include <set>

#include "Mir/Instruction.h"
#include "Pass/Analyses/AliasAnalysis.h"

namespace {
size_t gen_alloc_id() {
    static size_t alloc_id = 0;
    return ++alloc_id;
}

bool tbaa_distinct(const std::shared_ptr<Mir::Type::Type> &a, const std::shared_ptr<Mir::Type::Type> &b) {
    auto tbaa_include = [&](auto &&self, const std::shared_ptr<Mir::Type::Type> &x,
                            const std::shared_ptr<Mir::Type::Type> &y) {
        if (*x == *y) {
            return true;
        }
        if (x->is_integer() || x->is_float()) {
            return false;
        }
        if (x->is_array()) {
            const auto array_type = x->as<Mir::Type::Array>();
            const auto next = y->is_array() ? array_type->get_element_type() : array_type->get_atomic_type();
            return self(self, next, y);
        }
        log_error("Unexpected tbaa include: %s %s", x->to_string().c_str(), y->to_string().c_str());
    };
    return !tbaa_include(tbaa_include, a, b) && !tbaa_include(tbaa_include, b, a);
}
} // namespace

namespace Pass {
void AliasAnalysis::run_on_func(const std::shared_ptr<Mir::Function> &func) {
    const auto alias_result = std::make_shared<Result>();

    // 全局变量和栈变量存储在不同的内存区域，因此互不相交
    size_t global_id = gen_alloc_id();
    size_t stack_id = gen_alloc_id();
    alias_result->add_distinct_pair_id(global_id, stack_id);

    std::unordered_set<size_t> global_groups, stack_groups;
    std::unordered_set<InheritEdge, InheritEdge::Hasher> inherit_graph;
    std::unordered_set<std::shared_ptr<Mir::Block>> visited_blocks;
    // 全局变量：分配一个新的 id，将新分配的id与global_id与该全局变量关联
    for (const auto &gv: module->get_global_variables()) {
        auto id = gen_alloc_id();
        alias_result->set_value_attrs(gv, {global_id, id});
        global_groups.insert(id);
    }
    // 函数参数：对于指针类型参数，共享相同的arg_id，表示参数之间可能别名
    auto arg_id = gen_alloc_id();
    for (const auto &arg: func->get_arguments()) {
        if (arg->get_type()->is_pointer()) {
            alias_result->set_value_attrs(arg, {arg_id});
        }
    }

    const auto &dom_tree_layer_order = dom_graph->dom_tree_layer(func);
    for (const auto &block: dom_tree_layer_order) {
        visited_blocks.insert(block);
        for (const auto &inst: block->get_instructions()) {
            if (!inst->get_type()->is_pointer()) {
                continue;
            }
            switch (inst->get_op()) {
                case Mir::Operator::ALLOC: {
                    auto id = gen_alloc_id();
                    stack_groups.insert(id);
                    // alloc得到的内存空间与函数参数不可能重名
                    alias_result->add_distinct_pair_id(id, arg_id);
                    alias_result->set_value_attrs(inst, {stack_id, id});
                    break;
                }
                case Mir::Operator::BITCAST:
                case Mir::Operator::LOAD:
                case Mir::Operator::CALL: {
                    alias_result->set_value_attrs(inst, {});
                    break;
                }
                case Mir::Operator::PHI: {
                    alias_result->set_value_attrs(inst, {});
                    std::set<std::shared_ptr<Mir::Value>> inherit_set;
                    const auto phi = inst->as<Mir::Phi>();
                    for (const auto &[block, value]: phi->get_optional_values()) {
                        if (value->is_constant()) {
                            continue;
                        }
                        inherit_set.insert(value);
                        if (inherit_set.size() > 2) {
                            break;
                        }
                    }
                    if (inherit_set.size() == 2) {
                        inherit_graph.insert(InheritEdge{phi, *inherit_set.begin(), *inherit_set.rbegin()});
                    } else if (inherit_set.size() == 1) {
                        inherit_graph.insert(InheritEdge{phi, *inherit_set.begin()});
                    }
                    break;
                }
                case Mir::Operator::GEP: {
                    const auto gep = inst->as<Mir::GetElementPtr>();
                    std::vector<size_t> attrs;
                    auto cur = gep;
                    while (true) {
                        const auto base = gep->get_addr(), index = gep->get_index();
                        if (index->is_constant()) {
                            if (**index->as<Mir::ConstInt>() == 0) {
                                // gep的索引为0，则该 gep 的结果与 gep 的 base 指向相同的内存地址
                                inherit_graph.insert(InheritEdge{gep, base});
                                break;
                            }
                        }
                        if (cur == gep && index->is_constant()) {
                            if (**index->as<Mir::ConstInt>() != 0) {
                                // gep的索引不为0，则该 gep 的结果与 gep 的 base 不可能别名
                                const auto id1 = gen_alloc_id(), id2 = gen_alloc_id();
                                alias_result->add_distinct_pair_id(id1, id2);
                                attrs.push_back(id1);
                                alias_result->add_value_attrs(cur->get_addr(), id2);
                            }
                        }
                        if (const auto next = base->is<Mir::GetElementPtr>()) {
                            cur = next;
                        } else {
                            break;
                        }
                        alias_result->add_value_attrs(gep, attrs);
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    for (const auto &block: func->get_blocks()) {
        if (visited_blocks.count(block)) [[likely]] {
            continue;
        }
        for (const auto &inst: block->get_instructions()) {
            if (!inst->get_type()->is_pointer()) {
                continue;
            }
            alias_result->add_value_attrs(inst, {});
        }
    }

    // TBAA: 基于类型的别名分析
    std::unordered_map<std::shared_ptr<Mir::Type::Type>, size_t> types;
    for (const auto &[key, value]: alias_result->pointer_attributes) {
        if (!key->get_type()->is_pointer()) [[unlikely]] {
            log_error("Key must be a pointer type: %s", key->to_string().c_str());
        }
        const auto type = key->get_type()->as<Mir::Type::Pointer>();
        if (types.count(type->get_contain_type())) {
            continue;
        }
        const auto id = gen_alloc_id();
        types[type] = id;
    }

    for (const auto &[type1, id1]: types) {
        const auto x = type1->as<Mir::Type::Pointer>()->get_contain_type();
        if (x == Mir::Type::Integer::i8) {
            continue;
        }
        for (const auto &[type2, id2]: types) {
            const auto y = type2->as<Mir::Type::Pointer>()->get_contain_type();
            if (y == Mir::Type::Integer::i8) {
                continue;
            }
            if (tbaa_distinct(x, y)) {
                alias_result->add_distinct_pair_id(id1, id2);
            }
        }
    }

    alias_result->add_distinct_group(global_groups);
    alias_result->add_distinct_group(stack_groups);

    while (true) {
        bool changed = false;
        for (const auto &edge: inherit_graph) {
            if (edge.src2 != nullptr) {
                auto vec1 = alias_result->inherit_from(edge.src1), vec2 = alias_result->inherit_from(edge.src2);
                std::sort(vec1.begin(), vec1.end());
                std::sort(vec2.begin(), vec2.end());
                std::vector<size_t> intersect;
                std::set_intersection(vec1.begin(), vec1.end(), vec2.begin(), vec2.end(),
                                      std::back_inserter(intersect));
                changed |= alias_result->add_value_attrs(edge.dst, intersect);
            } else {
                changed |= alias_result->add_value_attrs(edge.dst, alias_result->inherit_from(edge.src1));
            }
        }
        if (!changed) {
            break;
        }
    }

    results.push_back(alias_result);
}

void AliasAnalysis::analyze(const std::shared_ptr<const Mir::Module> module) {
    this->module = nullptr;
    this->dom_graph = nullptr;
    this->results.clear();

    this->module = std::const_pointer_cast<Mir::Module>(module);
    dom_graph = get_analysis_result<DominanceGraph>(module);
    for (const auto &func: *module) {
        run_on_func(func);
    }
}
} // namespace Pass

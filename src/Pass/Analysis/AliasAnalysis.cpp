#include <set>

#include "Mir/Instruction.h"
#include "Pass/Analysis.h"

namespace {
size_t gen_alloc_id() {
    static size_t alloc_id = 0;
    return ++alloc_id;
}
}

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

    const auto &dom_tree_layer_order = cfg->dom_tree_layer(func);
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
                        const auto base = gep->get_addr(),
                                   index = gep->get_index();
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
                                const auto id1 = gen_alloc_id(),
                                           id2 = gen_alloc_id();
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
                default: break;
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


    results.push_back(alias_result);
}

void AliasAnalysis::analyze(const std::shared_ptr<const Mir::Module> module) {
    this->module = std::const_pointer_cast<Mir::Module>(module);
    cfg = get_analysis_result<ControlFlowGraph>(module);
    for (const auto &func: *module) {
        run_on_func(func);
    }
}
}

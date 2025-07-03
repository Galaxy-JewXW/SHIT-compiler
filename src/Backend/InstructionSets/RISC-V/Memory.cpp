#include "Backend/InstructionSets/RISC-V/Memory.h"

// void RISCV::Memory::Memory::load_to(std::shared_ptr<RISCV::Registers::ABI> reg, std::shared_ptr<Mir::Value> value) {
    // // 如果是常量，直接加载到目标寄存器
    // if (value->is_constant()) {
    //     function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadImmediate>(reg, value->get_name()));
    //     return;
    // }
    
    // // 检查源是否分配了寄存器
    // if (!value->is_constant() && has_register(value->get_name())) {
    //     RISCV::Registers::ABI src_reg = get_register(value->get_name());
    //     function_field->add_instruction(std::make_shared<RISCV::Instructions::Add>(
    //         reg,
    //         std::make_shared<RISCV::Registers::StaticRegister>(src_reg),
    //         RISCV::Registers::ZERO));
    //     return;
    // }
    
    // // 原始逻辑，处理没有寄存器分配的情况
    // if (vreg2offset.find(value->get_name()) != vreg2offset.end()) {
    //     int64_t offset = function_field->sp->offset - vreg2offset[value->get_name()];
    //     function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadDoubleword>(reg, function_field->sp, offset));
    // } else if (vptr2offset.find(value->get_name()) != vptr2offset.end()) {
    //     log_warn("Not allowed to store from pointer to value");
    // } else {
    //     function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadAddress>(reg, value->get_name()));
    //     function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadDoubleword>(reg, reg, 0));
    // }
// }

// void RISCV::Memory::Memory::store_to(const std::shared_ptr<Backend::MIR::Variable> &store_to, std::shared_ptr<Mir::Value> value) {
    // 如果源和目标都分配了寄存器，直接进行寄存器到寄存器的复制
    // if (!value->is_constant() && has_register(value->get_name()) && has_register(store_to)) {
    //     RISCV::Registers::ABI src_reg = get_register(value->get_name());
    //     RISCV::Registers::ABI dst_reg = get_register(store_to);
    //     function_field->add_instruction(std::make_shared<RISCV::Instructions::Add>(
    //         std::make_shared<RISCV::Registers::StaticRegister>(dst_reg),
    //         std::make_shared<RISCV::Registers::StaticRegister>(src_reg),
    //         RISCV::Registers::ZERO));
    //     return;
    // }
    
    // // 首先将源加载到A0寄存器
    // this->load_to(RISCV::Registers::A0, value);
    
    // // 如果目标分配了寄存器，直接将A0复制到目标寄存器
    // if (has_register(store_to)) {
    //     RISCV::Registers::ABI dst_reg = get_register(store_to);
    //     function_field->add_instruction(std::make_shared<RISCV::Instructions::Add>(
    //         std::make_shared<RISCV::Registers::StaticRegister>(dst_reg),
    //         RISCV::Registers::A0,
    //         RISCV::Registers::ZERO));
    //     return;
    // }
    
    // // 原始逻辑，处理没有寄存器分配的情况
    // if (this->vreg2offset.find(store_to) != this->vreg2offset.end()) {
    //     int64_t offset = function_field->sp->offset - this->vreg2offset[store_to];
    //     function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadDoubleword>(RISCV::Registers::A7, function_field->sp, offset));
    //     function_field->add_instruction(std::make_shared<RISCV::Instructions::StoreDoubleword>(RISCV::Registers::A0, RISCV::Registers::A7, 0));
    // } else {
    //     this->store_to(store_to, RISCV::Registers::A0);
    // }
// }

// void RISCV::Memory::Memory::store_to(const std::shared_ptr<Backend::MIR::Variable> &store_to, RISCV::Registers::ABI reg) {
//     this->function_field->add_instruction(std::make_shared<RISCV::Instructions::StoreDoubleword>(reg, RISCV::Registers::ABI::SP, this->function_field->sp->offset - store_to->offset));
//     RISCV::Registers::register_occupied[static_cast<int>(reg)] = store_to;
// }

// void RISCV::Memory::Memory::get_element_pointer(const std::shared_ptr<Backend::MIR::Variable> &array, std::shared_ptr<Mir::Value> offset, const std::shared_ptr<Backend::MIR::Variable> &store_to) {
    // 图着色寄存器分配对数组元素指针的处理与原始实现相同
    // if (offset->is_constant()) {
    //     function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadImmediate>(RISCV::Registers::A0, std::any_cast<int>(std::dynamic_pointer_cast<Mir::Const>(offset)->get_constant_value()) << 3));
    // } else if (vreg2offset.find(offset->get_name()) != vreg2offset.end()) {
    //     int64_t offset_ = function_field->sp->offset - vreg2offset[offset->get_name()];
    //     function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadDoubleword>(RISCV::Registers::A0, function_field->sp, offset_));
    //     function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadDoubleword>(RISCV::Registers::A0, RISCV::Registers::A0, 0));
    // } else if (vptr2offset.find(offset->get_name()) != vptr2offset.end()) {
    //     int64_t offset_ = function_field->sp->offset - vptr2offset[offset->get_name()];
    //     function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadDoubleword>(RISCV::Registers::A0, function_field->sp, offset_));
    // } else {
    //     function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadAddress>(RISCV::Registers::A0, offset->get_name()));
    //     function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadDoubleword>(RISCV::Registers::A0, RISCV::Registers::A0, 0));
    // }
    // if (vreg2offset.find(array) != vreg2offset.end()) {
    //     // size_t offset = function_field.sp - function_field.memory.vreg2offset[array];
    //     // size_t offset_ = function_field.sp - function_field.memory.vreg2offset[obejct_reference];
    //     // function_field.add_instruction(std::make_shared<RISCV::Instructions::LoadDoubleword>(RISCV::Instructions::Registers::A0, RISCV::Instructions::Registers::SP, offset));
    //     // function_field.add_instruction(std::make_shared<RISCV::Instructions::StoreDoubleword>(RISCV::Instructions::Registers::A0, RISCV::Instructions::Registers::SP, offset_));
    // } else {
    //     size_t offset_ = function_field->sp->offset - vreg2offset[store_to];
    //     function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadAddress>(RISCV::Registers::A7, array));
    //     function_field->add_instruction(std::make_shared<RISCV::Instructions::Add>(RISCV::Registers::A0, RISCV::Registers::A0, RISCV::Registers::A7));
    //     function_field->add_instruction(std::make_shared<RISCV::Instructions::StoreDoubleword>(RISCV::Registers::A0, function_field->sp, offset_));
    // }
// }

// RISCV::Registers::ABI RISCV::Memory::Memory::get_register(const std::shared_ptr<Backend::MIR::Variable> &var) const {
//     if (var->reg != RISCV::Registers::ABI::ZERO) {
//         return var->reg;
//     }
//     log_error("Variable %s has not been allocated a register", var->name.c_str());
//     return RISCV::Registers::ABI::ZERO;
// }

// void RISCV::Memory::Memory::load(const std::shared_ptr<Backend::MIR::Variable> &load_from) {
//     RISCV::Registers::ABI src = get_register(load_from);
//     if (RISCV::Registers::register_occupied[static_cast<int>(src)] && RISCV::Registers::register_occupied[static_cast<int>(src)].get() != load_from.get()) {
//         store(RISCV::Registers::register_occupied[static_cast<int>(src)]);
//     } else if (RISCV::Registers::register_occupied[static_cast<int>(src)] && RISCV::Registers::register_occupied[static_cast<int>(src)].get() == load_from.get()) {
//         return;
//     }
//     this->function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadDoubleword>(src, RISCV::Registers::ABI::SP, this->function_field->sp->offset - load_from->offset));
//     RISCV::Registers::register_occupied[static_cast<int>(src)] = load_from;
// }

// void RISCV::Memory::Memory::store(const std::shared_ptr<Backend::MIR::Variable> &store_to) {
//     RISCV::Registers::ABI src = get_register(store_to);
//     if (RISCV::Registers::register_occupied[static_cast<int>(src)] && RISCV::Registers::register_occupied[static_cast<int>(src)].get() == store_to.get()) {
//         this->function_field->add_instruction(std::make_shared<RISCV::Instructions::StoreDoubleword>(src, RISCV::Registers::ABI::SP, this->function_field->sp->offset - store_to->offset));
//         RISCV::Registers::register_occupied[static_cast<int>(src)] = nullptr;
//     }
// }
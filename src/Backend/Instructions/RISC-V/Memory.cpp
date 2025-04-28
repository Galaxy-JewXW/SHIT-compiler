#include "Backend/Instructions/RISC-V/Memory.h"
#include "Backend/Instructions/RISC-V/Registers.h"
#include "Backend/Instructions/RISC-V/Instructions.h"
#include "Backend/Instructions/RISC-V/Modules.h"
#include "Mir/Value.h"
#include "Utils/Log.h"

namespace RISCV::Memory {
    void Memory::load_to(const std::string &load_from, const std::string &load_to) {
        if (this->vptr2offset.find(load_from) != this->vptr2offset.end()) {
            int64_t offset = this->function_field->sp->offset - this->vptr2offset[load_from];
            this->function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadDoubleword>(RISCV::Registers::A0, this->function_field->sp, offset));
        } else if (this->vreg2offset.find(load_from) != this->vreg2offset.end()) {
            int64_t offset = this->function_field->sp->offset - this->vreg2offset[load_from];
            this->function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadDoubleword>(RISCV::Registers::A0, this->function_field->sp, offset));
            this->function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadDoubleword>(RISCV::Registers::A0, RISCV::Registers::A0, 0));
        } else {
            this->function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadAddress>(RISCV::Registers::A0, load_from));
            this->function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadDoubleword>(RISCV::Registers::A0, RISCV::Registers::A0, 0));
        }
        if (this->vreg2offset.find(load_to) != this->vreg2offset.end()) {
            int64_t offset = this->function_field->sp->offset - this->vreg2offset[load_to];
            this->function_field->add_instruction(std::make_shared<RISCV::Instructions::StoreDoubleword>(RISCV::Registers::A0, this->function_field->sp, offset));
        } else {
            this->function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadAddress>(RISCV::Registers::A7, load_to));
            this->function_field->add_instruction(std::make_shared<RISCV::Instructions::StoreDoubleword>(RISCV::Registers::A0, RISCV::Registers::A7, 0));
        }
    }

    void Memory::load_to(std::shared_ptr<RISCV::Registers::Register> reg, std::shared_ptr<Mir::Value> value) {
        if (value->is_constant()) {
            function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadImmediate>(reg, value->get_name()));
        } else if (vreg2offset.find(value->get_name()) != vreg2offset.end()) {
            int64_t offset = function_field->sp->offset - vreg2offset[value->get_name()];
            function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadDoubleword>(reg, function_field->sp, offset));
        } else if (vptr2offset.find(value->get_name()) != vptr2offset.end()) {
            // int64_t offset = function_field->sp->offset - vptr2offset[value->get_name()];
            // function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadDoubleword>(reg, function_field->sp, offset));
            log_warn("Not allowed to store from pointer to value");
        } else {
            function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadAddress>(reg, value->get_name()));
            function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadDoubleword>(reg, reg, 0));
        }
    }

    void Memory::store_to(const std::string &store_to, std::shared_ptr<Mir::Value> value) {
        this->load_to(RISCV::Registers::A0, value);
        if (this->vreg2offset.find(store_to) != this->vreg2offset.end()) {
            int64_t offset = function_field->sp->offset - this->vreg2offset[store_to];
            function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadDoubleword>(RISCV::Registers::A7, function_field->sp, offset));
            function_field->add_instruction(std::make_shared<RISCV::Instructions::StoreDoubleword>(RISCV::Registers::A0, RISCV::Registers::A7, 0));
        } else {
            this->store_to(store_to, RISCV::Registers::A0);
        }
    }

    void Memory::store_to(const std::string &store_to, std::shared_ptr<RISCV::Registers::Register> reg) {
        if (this->vreg2offset.find(store_to) != this->vreg2offset.end()) {
            int64_t offset = function_field->sp->offset - this->vreg2offset[store_to];
            function_field->add_instruction(std::make_shared<RISCV::Instructions::StoreDoubleword>(reg, function_field->sp, offset));
        } else if (this->vptr2offset.find(store_to) != this->vptr2offset.end()) {
            int64_t offset = function_field->sp->offset - this->vptr2offset[store_to];
            function_field->add_instruction(std::make_shared<RISCV::Instructions::StoreDoubleword>(reg, function_field->sp, offset));
        } else {
            function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadAddress>(RISCV::Registers::A7, store_to));
            function_field->add_instruction(std::make_shared<RISCV::Instructions::StoreDoubleword>(reg, RISCV::Registers::A7, 0));
        }
    }

    void Memory::get_element_pointer(const std::string &array, std::shared_ptr<Mir::Value> offset, const std::string &store_to) {
        if (offset->is_constant()) {
            function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadImmediate>(RISCV::Registers::A0, std::any_cast<int>(std::dynamic_pointer_cast<Mir::Const>(offset)->get_constant_value()) << 3));
        } else if (vreg2offset.find(offset->get_name()) != vreg2offset.end()) {
            int64_t offset_ = function_field->sp->offset - vreg2offset[offset->get_name()];
            function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadDoubleword>(RISCV::Registers::A0, function_field->sp, offset_));
            function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadDoubleword>(RISCV::Registers::A0, RISCV::Registers::A0, 0));
        } else if (vptr2offset.find(offset->get_name()) != vptr2offset.end()) {
            int64_t offset_ = function_field->sp->offset - vptr2offset[offset->get_name()];
            function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadDoubleword>(RISCV::Registers::A0, function_field->sp, offset_));
        } else {
            function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadAddress>(RISCV::Registers::A0, offset->get_name()));
            function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadDoubleword>(RISCV::Registers::A0, RISCV::Registers::A0, 0));
        }
        if (vreg2offset.find(array) != vreg2offset.end()) {
            // size_t offset = function_field.sp - function_field.memory.vreg2offset[array];
            // size_t offset_ = function_field.sp - function_field.memory.vreg2offset[obejct_reference];
            // function_field.add_instruction(std::make_shared<RISCV::Instructions::LoadDoubleword>(RISCV::Instructions::Registers::A0, RISCV::Instructions::Registers::SP, offset));
            // function_field.add_instruction(std::make_shared<RISCV::Instructions::StoreDoubleword>(RISCV::Instructions::Registers::A0, RISCV::Instructions::Registers::SP, offset_));
        } else {
            size_t offset_ = function_field->sp->offset - vreg2offset[store_to];
            function_field->add_instruction(std::make_shared<RISCV::Instructions::LoadAddress>(RISCV::Registers::A7, array));
            function_field->add_instruction(std::make_shared<RISCV::Instructions::Add>(RISCV::Registers::A0, RISCV::Registers::A0, RISCV::Registers::A7));
            function_field->add_instruction(std::make_shared<RISCV::Instructions::StoreDoubleword>(RISCV::Registers::A0, function_field->sp, offset_));
        }
    }
}
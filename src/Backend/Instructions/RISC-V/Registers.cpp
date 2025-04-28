#include "Backend/Instructions/RISC-V/Registers.h"
#include <memory>
#include "Backend/Instructions/RISC-V/Instructions.h"
#include "Backend/Instructions/RISC-V/Modules.h"
#include "Utils/Log.h"

[[nodiscard]] std::string RISCV::Registers::reg2string(const RISCV::Registers::ABI &reg) {
    switch (reg) {
        case ABI::ZERO:  return "zero";
        case ABI::RA:    return "ra";
        case ABI::SP:    return "sp";
        case ABI::GP:    return "gp";
        case ABI::TP:    return "tp";
        case ABI::T0:    return "t0";
        case ABI::T1:    return "t1";
        case ABI::T2:    return "t2";
        case ABI::FP:    return "fp";
        case ABI::S1:    return "s1";
        case ABI::A0:    return "a0";
        case ABI::A1:    return "a1";
        case ABI::A2:    return "a2";
        case ABI::A3:    return "a3";
        case ABI::A4:    return "a4";
        case ABI::A5:    return "a5";
        case ABI::A6:    return "a6";
        case ABI::A7:    return "a7";
        case ABI::S2:    return "s1";
        case ABI::S3:    return "s2";
        case ABI::S4:    return "s3";
        case ABI::S5:    return "s4";
        case ABI::S6:    return "s5";
        case ABI::S7:    return "s6";
        case ABI::S8:    return "s7";
        case ABI::S9:    return "s8";
        case ABI::S10:   return "s9";
        case ABI::S11:   return "s10";
        case ABI::T3:    return "t3";
        case ABI::T4:    return "t4";
        case ABI::T5:    return "t5";
        case ABI::T6:    return "t6";

        case ABI::FT0:   return "ft0";
        case ABI::FT1:   return "ft1";
        case ABI::FT2:   return "ft2";
        case ABI::FT3:   return "ft3";
        case ABI::FT4:   return "ft4";
        case ABI::FT5:   return "ft5";
        case ABI::FT6:   return "ft6";
        case ABI::FT7:   return "ft7";

        default: return "";
    }
}

[[nodiscard]] std::string RISCV::Registers::StaticRegister::to_string() const {
    return RISCV::Registers::reg2string(reg);
}

[[nodiscard]] std::string RISCV::Registers::StackPointer::to_string() const {
    return "sp";
}

namespace RISCV::Registers {
    void StackPointer::alloc_stack(int64_t size) {
        alloc_record.push_back(size);
        offset += size;
        log_debug("Alloc: %lld, $sp: %lld", size, offset);
        function_field->add_instruction(std::make_shared<RISCV::Instructions::AddImmediate>(function_field->sp, function_field->sp, -size));
    }

    void StackPointer::alloc_stack() {
        return alloc_stack(8);
    }

    void StackPointer::free_stack(int64_t size) {
        if (size == 0) {
            log_warn("Are you sure you want to free 0 bytes?");
        } else {
            int64_t total = 0;
            for (std::vector<int64_t>::iterator it = alloc_record.end(); it != alloc_record.begin(); it--) {
                total += *it;
                if (total == size) {
                    alloc_record.erase(it, alloc_record.end());
                    this->offset -= size;
                    log_debug("Free: %lld, $sp: %lld", size, offset);
                    function_field->add_instruction(std::make_shared<RISCV::Instructions::AddImmediate>(function_field->sp, function_field->sp, size));
                } else if (total > size) {
                    log_warn("No exactly %lld bytes to free.", size);
                    return;
                }
            }
            log_warn("No enough %lld bytes to free.", size);
        }
    }

    void StackPointer::free_stack(size_t last_k) {
        if (last_k <= 0) {
            log_warn("Are you sure you want to free 0 bytes?");
        } else if (last_k > alloc_record.size()) {
            log_warn("No enough %d alloc records to free.", last_k);
        } else {
            int64_t total = 0;
            for (std::vector<int64_t>::iterator it = alloc_record.end() - 1; it != alloc_record.end() - last_k - 1; it--) {
                total += *it;
            }
            this->offset -= total;
            alloc_record.erase(alloc_record.end() - last_k, alloc_record.end());
            log_debug("Free: %lld, $sp: %lld", total, this->offset);
            function_field->add_instruction(std::make_shared<RISCV::Instructions::AddImmediate>(function_field->sp, function_field->sp, total));
        }
    }

    void StackPointer::free_stack() {
        alloc_record.clear();
        function_field->add_instruction(std::make_shared<RISCV::Instructions::AddImmediate>(function_field->sp, function_field->sp, offset));
        log_debug("Free: %lld, $sp: 0", offset);
        this->offset = 0;
    }
}
#include "Backend/MIR/Instructions.h"

std::string Backend::MIR::InstructionType_to_string(Backend::MIR::InstructionType type) {
    switch (type) {
        case Backend::MIR::InstructionType::ADD: return "+";
        case Backend::MIR::InstructionType::FADD: return "+";
        case Backend::MIR::InstructionType::SUB: return "-";
        case Backend::MIR::InstructionType::FSUB: return "-";
        case Backend::MIR::InstructionType::MUL: return "*";
        case Backend::MIR::InstructionType::FMUL: return "*";
        case Backend::MIR::InstructionType::DIV: return "/";
        case Backend::MIR::InstructionType::FDIV: return "/";
        case Backend::MIR::InstructionType::LOAD: return "load";
        case Backend::MIR::InstructionType::STORE: return "store";
        case Backend::MIR::InstructionType::CALL: return "call";
        case Backend::MIR::InstructionType::RETURN: return "return";
        case Backend::MIR::InstructionType::JUMP: return "jump";
        case Backend::MIR::InstructionType::BRANCH_ON_ZERO: return "BRANCH_ON_ZERO";
        case Backend::MIR::InstructionType::BRANCH_ON_NON_ZERO: return "BRANCH_ON_NON_ZERO";
        case Backend::MIR::InstructionType::BRANCH_ON_EQUAL: return "==";
        case Backend::MIR::InstructionType::BRANCH_ON_NOT_EQUAL: return "!=";
        case Backend::MIR::InstructionType::BRANCH_ON_GREATER_THAN: return ">";
        case Backend::MIR::InstructionType::BRANCH_ON_LESS_THAN: return "<";
        case Backend::MIR::InstructionType::BRANCH_ON_GREATER_THAN_OR_EQUAL: return ">=";
        case Backend::MIR::InstructionType::BRANCH_ON_LESS_THAN_OR_EQUAL: return "<=";
        case Backend::MIR::InstructionType::BITWISE_AND: return "&";
        case Backend::MIR::InstructionType::BITWISE_OR: return "|";
        case Backend::MIR::InstructionType::BITWISE_XOR: return "^";
        case Backend::MIR::InstructionType::BITWISE_NOT: return "!";
        case Backend::MIR::InstructionType::SHIFT_LEFT: return "<<";
        case Backend::MIR::InstructionType::SHIFT_RIGHT: return ">>";
        case Backend::MIR::InstructionType::SHIFT_LEFT_LOGICAL: return "SHIFT_LEFT_LOGICAL";
        case Backend::MIR::InstructionType::SHIFT_RIGHT_LOGICAL: return "SHIFT_RIGHT_LOGICAL";
        case Backend::MIR::InstructionType::SHIFT_RIGHT_ARITHMETIC: return "SHIFT_RIGHT_ARITHMETIC";
        case Backend::MIR::InstructionType::PUTF: return "putf";
        case Backend::MIR::InstructionType::LOAD_ADDR: return "load_addr";
        default: log_error("Unknown instruction type: {}", static_cast<uint32_t>(type));
    }
}
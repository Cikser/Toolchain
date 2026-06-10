#include "as.hpp"
#include <stdexcept>

void as::assembler::instr_halt() {
    emit_instruction(encode_instruction(0x0, 0x0, 0x0, 0x0, 0x0, 0));
}

void as::assembler::instr_int() {
    emit_instruction(encode_instruction(0x1, 0x0, 0x0, 0x0, 0x0, 0));
}

void as::assembler::instr_iret() {
    emit_instruction(encode_instruction(0x9, 0x6, 0x0, 0xE, 0x0, 4));
    emit_instruction(encode_instruction(0x9, 0x3, 0xF, 0xE, 0x0, 8));
}

void as::assembler::instr_ret() {
    emit_instruction(encode_instruction(0x9, 0x3, 0xF, 0xE, 0x0, 4));
}

void as::assembler::instr_push(int32_t reg) {
    emit_instruction(encode_instruction(0x8, 0x1, 0xE, 0x0, (uint8_t)reg, -4));
}

void as::assembler::instr_pop(int32_t reg) {
    emit_instruction(encode_instruction(0x9, 0x3, (uint8_t)reg, 0xE, 0x0, 4));
}

void as::assembler::instr_xchg(int32_t reg_s, int32_t reg_d) {
    emit_instruction(encode_instruction(0x4, 0x0, 0x0, (uint8_t)reg_d, (uint8_t)reg_s, 0));
}


void as::assembler::instr_add(int32_t reg_s, int32_t reg_d) {
    emit_instruction(encode_instruction(0x5, 0x0, (uint8_t)reg_d, (uint8_t)reg_d, (uint8_t)reg_s, 0));
}

void as::assembler::instr_sub(int32_t reg_s, int32_t reg_d) {
    emit_instruction(encode_instruction(0x5, 0x1, (uint8_t)reg_d, (uint8_t)reg_d, (uint8_t)reg_s, 0));
}

void as::assembler::instr_mul(int32_t reg_s, int32_t reg_d) {
    emit_instruction(encode_instruction(0x5, 0x2, (uint8_t)reg_d, (uint8_t)reg_d, (uint8_t)reg_s, 0));
}

void as::assembler::instr_div(int32_t reg_s, int32_t reg_d) {
    emit_instruction(encode_instruction(0x5, 0x3, (uint8_t)reg_d, (uint8_t)reg_d, (uint8_t)reg_s, 0));
}

void as::assembler::instr_not(int32_t reg) {
    emit_instruction(encode_instruction(0x6, 0x0, (uint8_t)reg, (uint8_t)reg, 0x0, 0));
}

void as::assembler::instr_and(int32_t reg_s, int32_t reg_d) {
    emit_instruction(encode_instruction(0x6, 0x1, (uint8_t)reg_d, (uint8_t)reg_d, (uint8_t)reg_s, 0));
}

void as::assembler::instr_or(int32_t reg_s, int32_t reg_d) {
    emit_instruction(encode_instruction(0x6, 0x2, (uint8_t)reg_d, (uint8_t)reg_d, (uint8_t)reg_s, 0));
}

void as::assembler::instr_xor(int32_t reg_s, int32_t reg_d) {
    emit_instruction(encode_instruction(0x6, 0x3, (uint8_t)reg_d, (uint8_t)reg_d, (uint8_t)reg_s, 0));
}

void as::assembler::instr_shl(int32_t reg_s, int32_t reg_d) {
    emit_instruction(encode_instruction(0x7, 0x0, (uint8_t)reg_d, (uint8_t)reg_d, (uint8_t)reg_s, 0));
}

void as::assembler::instr_shr(int32_t reg_s, int32_t reg_d) {
    emit_instruction(encode_instruction(0x7, 0x1, (uint8_t)reg_d, (uint8_t)reg_d, (uint8_t)reg_s, 0));
}

void as::assembler::instr_csrrd(int32_t csr, int32_t reg) {
    emit_instruction(encode_instruction(0x9, 0x0, (uint8_t)reg, (uint8_t)csr, 0x0, 0));
}

void as::assembler::instr_csrwr(int32_t reg, int32_t csr) {
    emit_instruction(encode_instruction(0x9, 0x4, (uint8_t)csr, (uint8_t)reg, 0x0, 0));
}

void as::assembler::instr_jmp(const operand_t& op) {
   emit_jump_or_call(0x3, 0x0, 0x8, 0x0, 0x0, op);
}

void as::assembler::instr_call(const operand_t& op) {
    emit_jump_or_call(0x2, 0x0, 0x1, 0x0, 0x0, op);
}

void as::assembler::instr_beq(int32_t reg1, int32_t reg2, const operand_t& op) {
    emit_jump_or_call(0x3, 0x1, 0x9, (uint8_t)reg1, (uint8_t)reg2, op);
}

void as::assembler::instr_bne(int32_t reg1, int32_t reg2, const operand_t& op) {
    emit_jump_or_call(0x3, 0x2, 0xA, (uint8_t)reg1, (uint8_t)reg2, op);
}

void as::assembler::instr_bgt(int32_t reg1, int32_t reg2, const operand_t& op) {
    emit_jump_or_call(0x3, 0x3, 0xB, (uint8_t)reg1, (uint8_t)reg2, op);
}

void as::assembler::instr_ld(const operand_t& op, int32_t reg) {
    emit_ld(op, reg);
}

void as::assembler::instr_st(int32_t reg, const operand_t& op) {
    emit_st(reg, op);
}

void as::assembler::emit_ld(const operand_t& op, int32_t reg) {
    if (reg == 0) {
        throw std::runtime_error("Unable to load value in r0");
    }
    switch (op.type) {
        case operand_type::LITERAL_IMM: {
            if (check_bounds(op.literal)) {
                emit_instruction(encode_instruction(0x9, 0x1, (uint8_t)reg, 0x0, 0x0, op.literal));
            }
            else {
                // todo literal pool
            }
            break;
        }
        case operand_type::SYMBOL_IMM: {
            auto it = m_sym_table.find(op.symbol);
            if (it != m_sym_table.end()) {
                if (it->second.absolute && check_bounds(it->second.value)) {
                    emit_instruction(encode_instruction(0x9, 0x1, (uint8_t)reg, 0x0, 0x0, (int16_t)it->second.value));
                }
                else if (it->second.defined && it->second.section == m_current_section && check_bounds(it->second.value - (current_offset() + 4))) {
                    emit_instruction(encode_instruction(0x9, 0x1, (uint8_t)reg, 0xF, 0x0, (int16_t)(it->second.value - (current_offset() + 4))));
                }
                else {
                    // todo literal pool / reloc
                }
            }
            else {
                symbol_t sym{};
                sym.name = op.symbol;
                sym.section = SECTION_UNDEF;
                m_sym_table.insert({op.symbol, sym});
                // todo literal pool / reloc
            }
            break;
        }
        case operand_type::LITERAL_MEM: {
            if (check_bounds(op.literal)) {
                emit_instruction(encode_instruction(0x9, 0x2, (uint8_t)reg, 0, 0, (int16_t)op.literal));
            }
            else {
                // todo literal pool
            }
            break;
        }
        case operand_type::SYMBOL_MEM: {
            auto it = m_sym_table.find(op.symbol);
            if (it != m_sym_table.end()) {
                if (it->second.absolute && check_bounds(it->second.value)) {
                    emit_instruction(encode_instruction(0x9, 0x2, (uint8_t)reg, 0x0, 0x0, (int16_t)it->second.value));
                }
                else if (it->second.defined && it->second.section == m_current_section && check_bounds(it->second.value - (current_offset() + 4))) {
                    emit_instruction(encode_instruction(0x9, 0x2, (uint8_t)reg, 0xF, 0x0, (int16_t)(it->second.value - (current_offset() + 4))));
                }
                else {
                    // todo literal pool / reloc
                }
            }
            else {
                symbol_t sym{};
                sym.name = op.symbol;
                sym.section = SECTION_UNDEF;
                m_sym_table.insert({op.symbol, sym});
                // todo literal pool / reloc
            }
            break;
        }
        case operand_type::REG_DIRECT: {
            emit_instruction(encode_instruction(0x9, 0x1, (uint8_t)reg, op.reg, 0x0, 0x0));
            break;
        }
        case operand_type::REG_INDIRECT: {
            emit_instruction(encode_instruction(0x9, 0x2, (uint8_t)reg, op.reg, 0x0, 0x0));
            break;
        }
        case operand_type::REG_OFFSET_LIT: {
            if (!check_bounds(op.literal)) {
                throw std::runtime_error("Literal offset does not fit in 12-bit signed displacement");
            }
            emit_instruction(encode_instruction(0x9, 0x2, (uint8_t)reg, op.reg, 0x0, op.literal));
            break;
        }
        case operand_type::REG_OFFSET_SYM: {
            auto it = m_sym_table.find(op.symbol);
            if (it == m_sym_table.end() || !it->second.defined || !it->second.absolute) {
                throw std::runtime_error("Symbol offset must be absolute and known: " + op.symbol);
            }
            if (!check_bounds(it->second.value)) {
                throw std::runtime_error("Symbol " + it->second.name + " offset does not fit in 12-bit signed displacement");
            }
            emit_instruction(encode_instruction(0x9, 0x2, (uint8_t)reg, (uint8_t)op.reg, 0x0, (int16_t)op.literal));
            break;
        }
        default:
            throw std::runtime_error("Invalid operand type for st instruction");
    }
}

void as::assembler::emit_st(int32_t reg, const operand_t& op) {
    switch (op.type) {
        case operand_type::LITERAL_MEM: {
            if (check_bounds(op.literal)) {
                emit_instruction(encode_instruction(0x8, 0x0, 0x0, 0x0, (uint8_t)reg, op.literal));
            }
            else {
                // todo literal pool
            }
            break;
        }
        case operand_type::SYMBOL_MEM: {
            auto it = m_sym_table.find(op.symbol);
            if (it != m_sym_table.end()) {
                if (it->second.absolute && check_bounds(it->second.value)) {
                    emit_instruction(encode_instruction(0x8, 0x0, 0x0, 0x0, (uint8_t)reg, (int16_t)it->second.value));
                }
                else if (it->second.defined && it->second.section == m_current_section && check_bounds(it->second.value - (current_offset() + 4))) {
                    emit_instruction(encode_instruction(0x8, 0x0, 0x0, 0xF, (uint8_t)reg, (int16_t)(it->second.value - (current_offset() + 4))));
                }
                else {
                    // todo literal pool / reloc
                }
            }
            else {
                symbol_t sym{};
                sym.name = op.symbol;
                sym.section = SECTION_UNDEF;
                m_sym_table.insert({op.symbol, sym});
                // todo literal pool / reloc
            }
            break;
        }
        case operand_type::REG_INDIRECT: {
            emit_instruction(encode_instruction(0x8, 0x0, (uint8_t)op.reg, 0x0, (uint8_t)reg, 0x0));
            break;
        }
        case operand_type::REG_OFFSET_LIT: {
            if (!check_bounds(op.literal)) {
                throw std::runtime_error("Literal offset does not fit in 12-bit signed displacement");
            }
            emit_instruction(encode_instruction(0x8, 0x0, (uint8_t)op.reg, 0x0, (uint8_t)reg, op.literal));
            break;
        }
        case operand_type::REG_OFFSET_SYM: {
            auto it = m_sym_table.find(op.symbol);
            if (it == m_sym_table.end() || !it->second.defined || !it->second.absolute) {
                throw std::runtime_error("Symbol offset must be absolute and known: " + op.symbol);
            }
            if (!check_bounds(it->second.value)) {
                throw std::runtime_error("Symbol " + it->second.name + " offset does not fit in 12-bit signed displacement");
            }
            emit_instruction(encode_instruction(0x8, 0x0, 0x0, (uint8_t)op.reg, (uint8_t)reg, (int16_t)op.literal));
            break;
        }
        default:
            throw std::runtime_error("Invalid operand type for st instruction");
    }
}
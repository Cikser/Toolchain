#include "as.hpp"

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
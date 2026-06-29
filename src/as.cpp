#include "as.hpp"
#include <stdexcept>
#include <iostream>
#include <algorithm>

as::assembler::assembler() {
    section_t section_undef{};
    section_undef.name = SECTION_UNDEF;
    section_undef.idx = SECTION_UNDEF_IDX;
    m_section_table.insert({SECTION_UNDEF, section_undef});
    section_t section_abs{};
    section_abs.name = SECTION_ABS;
    section_abs.idx = SECTION_ABS_IDX;
    m_section_table.insert({SECTION_ABS, section_abs});
}

void as::assembler::define_label(const std::string& name) {
    auto it = m_sym_table.find(name);
    if (it != m_sym_table.end()) {
        if (it->second.is_extern || it->second.defined) {
            throw std::runtime_error("Symbol " + name + " already defined");
        }
        it->second.defined = true;
        it->second.section = current_section().name;
        it->second.value = current_offset();
    }
    else {
        symbol_t sym{};
        sym.defined = true;
        sym.name = name;
        sym.section = current_section().name;
        sym.value = current_offset();
        m_sym_table.insert({name, sym});
    }
}

void as::assembler::emit_byte(uint8_t byte) {
    if (m_current_section == SECTION_UNDEF) {
        throw std::runtime_error("No active section, use .section before emitting data");
    }
    current_section().data.push_back(byte);
}

void as::assembler::emit_word(uint32_t word) {
    emit_byte((uint8_t)(word >> 0) & 0xFF);
    emit_byte((uint8_t)(word >> 8) & 0xFF);
    emit_byte((uint8_t)(word >> 16) & 0xFF);
    emit_byte((uint8_t)(word >> 24) & 0xFF);
}

void as::assembler::emit_instruction(uint32_t instr) {
    emit_word(instr);
    check_pool();
}

void as::assembler::insert_instr_disp(std::vector<uint8_t>& data, uint32_t index, uint16_t value) {
    data[index + 2] |= (value & 0xF00) >> 8;
    data[index + 3] = value & 0xFF;
}

void as::assembler::check_pool(bool end_of_section) {
    if (!end_of_section) {
        section_t& sec = current_section();
        if (sec.pool_entries.empty()) {
            return;
        }
        if (current_offset() - sec.pool_entries[0].offset < 2040) {
            return;
        }
    }
    else {
        if (m_current_section == SECTION_UNDEF) {
            return;
        }
    }
    section_t& sec = current_section();
    uint32_t pool_size = sec.pool_entries.size() * 4;
    emit_instruction(encode_instruction(0x3, 0x0, 0xF, 0x0, 0x0, pool_size));
    for (auto& entry : sec.pool_entries) {
        if (entry.type == pool_entry_type::LITERAL) {
            emit_word(entry.literal);
            insert_instr_disp(sec.data, entry.offset, current_offset() - entry.offset - 8);
        }
        else {
            auto it = m_sym_table.find(entry.symbol_name);
            if (it != m_sym_table.end() && it->second.absolute && it->second.defined) {
                emit_word(it->second.value);
                insert_instr_disp(sec.data, entry.offset, current_offset() - entry.offset - 8);
            }
            else {
                emit_word(0x0);
                insert_instr_disp(sec.data, entry.offset, current_offset() - entry.offset - 8);
                backpatch_t bp{};
                bp.offset = current_offset() - 4;
                bp.type = backpatch_type::RELOC;
                bp.section_name = sec.name;
                bp.symbol_name = entry.symbol_name;
                m_backpatch_table.push_back(bp);
            }
        }
    }
    sec.pool_entries.clear();
}

static constexpr uint32_t INSTR_OFFSET = 4;
static constexpr uint32_t MODF_OFFSET = 0;
static constexpr uint32_t REGA_OFFSET = 12;
static constexpr uint32_t REGB_OFFSET = 8;
static constexpr uint32_t REGC_OFFSET = 20;
static constexpr uint32_t DISP_HI_OFFSET = 16;
static constexpr uint32_t DISP_LO_OFFSET = 24;

static constexpr uint32_t INSTR_MASK = 0xF;
static constexpr uint32_t MODF_MASK = 0xF;
static constexpr uint32_t REG_MASK = 0xF;
static constexpr uint32_t DISP_HI_MASK = 0xF;
static constexpr uint32_t DISP_LO_MASK = 0xFF;

uint32_t as::assembler::encode_instruction(uint8_t oc, uint8_t mod, uint8_t regA,
                                        uint8_t regB, uint8_t regC, int16_t disp) {
        uint32_t instr = 0;
        instr |= ((uint32_t)(oc & INSTR_MASK) << INSTR_OFFSET);
        instr |= ((uint32_t)(mod & MODF_MASK) << MODF_OFFSET);
        instr |= ((uint32_t)(regA & REG_MASK) << REGA_OFFSET);
        instr |= ((uint32_t)(regB & REG_MASK) << REGB_OFFSET);
        instr |= ((uint32_t)(regC & REG_MASK) << REGC_OFFSET);
        instr |= ((uint32_t)((disp >> 8) & DISP_HI_MASK) << DISP_HI_OFFSET);
        instr |= ((uint32_t)(disp & DISP_LO_MASK) << DISP_LO_OFFSET);
        return instr;
}

uint32_t as::assembler::current_offset() {
    return current_section().data.size();
}

as::section_t& as::assembler::current_section() {
    return m_section_table.at(m_current_section);
}

bool as::assembler::check_bounds(int32_t literal) {
    return literal >= -2048 && literal <= 2047;
} 

void as::assembler::resolve_symbols() {
    for (auto& [key, symbol] : m_sym_table) {
        if (symbol.section == SECTION_UNDEF) {
            symbol.is_extern = true;
            symbol.global = true;
        }
    }
}

void as::assembler::resolve_bounds_backpatch() {
    bool change = true;
    std::vector<backpatch_t> bps;
    for (const auto& bp : m_backpatch_table) {
        if (bp.type == backpatch_type::BOUNDS) {
            bps.push_back(bp);
        }
    }
    if (bps.empty()) {
        return;
    }
    for (auto iterator = m_backpatch_table.begin(); iterator != m_backpatch_table.end(); ++iterator) {
        if (iterator->type == backpatch_type::BOUNDS) {
            m_backpatch_table.erase(iterator);
            if (iterator == m_backpatch_table.end()) {
                break;
            }
        }
    }
    while (change) {
        change = false;
        for (auto iterator = bps.begin(); iterator != bps.end(); ++iterator) {
            backpatch_t& bp = *iterator;
            auto it = m_sym_table.find(bp.symbol_name);
            if (it == m_sym_table.end()) {
                continue;
            }
            symbol_t& sym = it->second;
            if (sym.absolute && check_bounds(sym.value)) {
                section_t& section = m_section_table.at(bp.section_name);
                section.data[bp.offset + 2] = (section.data[bp.offset + 2] & 0xF0) | (sym.value & (0xF << 8));
                section.data[bp.offset + 3] = sym.value & 0xFF;
            }
            else if (sym.absolute && !check_bounds(sym.value)) {
                throw std::runtime_error("Symbol " + sym.name + " offset does not fit in 12-bit signed displacement");
            }
            else {
                throw std::runtime_error("Symbol offset must be absolute and known: " + sym.name);
            }
            bps.erase(iterator);
            change = true;
            break;
        }
    }
    if (!bps.empty()) {
        throw std::runtime_error("Unable to complete backpatch");
    }
}

void as::assembler::resolve_reloc_backpatch() {
    bool change = true;
    std::vector<backpatch_t> bps;
    for (const auto& bp : m_backpatch_table) {
        if (bp.type == backpatch_type::RELOC) {
            bps.push_back(bp);
        }
    }
    if (bps.empty()) {
        return;
    }
    for (auto iterator = m_backpatch_table.begin(); iterator != m_backpatch_table.end(); ++iterator) {
        if (iterator->type == backpatch_type::RELOC) {
            m_backpatch_table.erase(iterator);
            if (iterator == m_backpatch_table.end()) {
                break;
            }
        }
    }
    while (change) {
        change = false;
        for (auto iterator = bps.begin(); iterator != bps.end(); ++iterator) {
            backpatch_t& bp = *iterator;
            auto it = m_sym_table.find(bp.symbol_name);
            if (it == m_sym_table.end()) {
                continue;
            }
            symbol_t& sym = it->second;
            if (sym.absolute) {
                section_t& section = m_section_table.at(bp.section_name);
                section.data[bp.offset] = sym.value & 0xFF;
                section.data[bp.offset + 1] = sym.value & 0xFF << 8;
                section.data[bp.offset + 2] = sym.value & 0xFF << 16;
                section.data[bp.offset + 3] = sym.value & 0xFF << 24;
            }
            else {
                relocation_t reloc{};
                reloc.section_name = bp.section_name;
                reloc.offset = bp.offset;
                reloc.type = relocation_type::R_32;
                if (sym.global) {
                    reloc.symbol_name = sym.name;
                    reloc.addend = 0;
                }
                else {
                    reloc.symbol_name = sym.section;
                    reloc.addend = sym.value;
                }
                section_t& section = m_section_table.find(bp.section_name)->second;
                section.relocations.push_back(reloc);
            }
            bps.erase(iterator);
            change = true;
            break;
        }
    }
    if (!bps.empty()) {
        throw std::runtime_error("Unable to complete backpatch");
    }
}

void as::assembler::resolve_backpatch() {
    check_pool(true);
    resolve_bounds_backpatch();
    resolve_reloc_backpatch();
    for (auto& [key, section] : m_section_table) {
        while (section.data.size() % 4 != 0) {
            section.data.push_back(0);
        }
    }
}

as::eval_result_t as::assembler::try_eval_expr(std::shared_ptr<expr_node_t>& expr, bool final) {
    eval_result_t out{};
    if (!expr->is_binary) {
        if (expr->is_literal) {
            out.value = expr->literal;
            out.absolute = true;
            out.valid = true;
            return out;
        }
        auto it = m_sym_table.find(expr->symbol);
        if (it != m_sym_table.end()) {
            symbol_t& sym = it->second;
            if (it->second.absolute && it->second.defined) {
                out.value = it->second.value;
                out.valid = true;
                out.absolute = true;
                return out;
            }
            else {
                out.value = sym.value;
                out.section = sym.section;
                out.valid = true;
                return out;
            }
        }
        symbol_t sym{};
        sym.name = expr->symbol;
        m_sym_table.insert({sym.name, sym});
        out.valid = false;
        return out;
    }
    eval_result_t left = try_eval_expr(expr->left);
    eval_result_t right = try_eval_expr(expr->right);
    switch (expr->type) {
        case expr_type::ADD: {
            out.value = left.value + right.value;
            break;
        }
        case expr_type::SUB: {
            out.value = left.value - right.value;
            break;
        }
        case expr_type::MUL: {
            out.value = left.value * right.value;
            break;
        }
        case expr_type::DIV: {
            out.value = left.value / right.value;
            break;
        }
    }
    if (left.absolute && right.absolute) {
        out.absolute = true;
        out.valid = true;
        return out;
    }
    if ((expr->type == expr_type::MUL || expr->type == expr_type::DIV) && (!left.absolute || !right.absolute)) {
        return out;
    }
    if (!left.absolute && !right.absolute && left.section != right.section) {
        return out;
    }
    if (!left.absolute && !right.absolute && left.section == right.section && final) {
        out.absolute = true;
        out.valid = true;
        return out;
    }
    out.valid = true;
    return out;
}

void as::assembler::resolve_pequs() {
    bool change = true;
    while (change) {
        change = false;
        for (auto iterator = m_pequ_table.begin(); iterator != m_pequ_table.end(); ++iterator) {
            pending_equ_t& pequ = *iterator;
            eval_result_t result = try_eval_expr(pequ.expr, true);
            if (!result.absolute) {
                continue;
            }
            symbol_t& sym = m_sym_table.at(pequ.symbol);
            sym.value = result.value;
            sym.defined = true;
            sym.absolute = true;
            sym.section = SECTION_ABS;
            change = true;
            m_pequ_table.erase(iterator);
            break;
        }
    }
    if (!m_pequ_table.empty()) {
        throw std::runtime_error("Unable to complete .equ evaluation");
    }
}
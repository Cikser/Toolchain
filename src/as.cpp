#include "as.hpp"
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <functional>

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
            section_t& section = m_section_table.at(bp.section_name);
            if (sym.absolute) {
                uint32_t value = (uint32_t)sym.value;
                section.data[bp.offset + 0] = (value >> 0) & 0xFF;
                section.data[bp.offset + 1] = (value >> 8) & 0xFF;
                section.data[bp.offset + 2] = (value >> 16) & 0xFF;
                section.data[bp.offset + 3] = (value >> 24) & 0xFF;
            }
            else if (sym.section.rfind(EXTERN_SECTION_PREFIX, 0) == 0) {
                relocation_t reloc{};
                reloc.section_name = bp.section_name;
                reloc.offset = bp.offset;
                reloc.type = relocation_type::R_32;
                reloc.symbol_name = sym.section.substr(PREFIX_LEN);
                reloc.addend = sym.value;
                section.relocations.push_back(reloc);
            }
            else if (sym.global) {
                relocation_t reloc{};
                reloc.section_name = bp.section_name;
                reloc.offset = bp.offset;
                reloc.type = relocation_type::R_32;
                reloc.symbol_name  = sym.name;
                reloc.addend = 0;
                section.relocations.push_back(reloc);
            }
            else {
                relocation_t reloc{};
                reloc.section_name = bp.section_name;
                reloc.offset = bp.offset;
                reloc.type = relocation_type::R_32;
                reloc.symbol_name = sym.section;
                reloc.addend = sym.value;
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

as::node_eval_t as::assembler::eval_node(const std::shared_ptr<expr_node_t>& node) {
    node_eval_t result;
    if (!node->is_binary) {
        if (node->is_literal) {
            result.value = node->literal;
            return result;
        }
        auto it = m_sym_table.find(node->symbol);
        if (it == m_sym_table.end()) {
            symbol_t sym{};
            sym.name = node->symbol;
            sym.section = SECTION_UNDEF;
            m_sym_table.insert({node->symbol, sym});
            result.deferred = true;
            return result;
        }
        const symbol_t& sym = it->second;
        if (sym.absolute && sym.defined) {
            result.value = sym.value;
            return result;
        }
        if (sym.defined && sym.section.rfind(EXTERN_SECTION_PREFIX, 0) == 0) {
            result.coeffs[sym.section] = 1;
            result.value = sym.value;
            return result;
        }
        if (sym.defined) {
            result.coeffs[sym.section] = 1;
            result.value = sym.value;
            return result;
        }
        if (sym.is_extern) {
            result.coeffs[EXTERN_SECTION_PREFIX + sym.name] = 1;
            result.value = 0;
            return result;
        }
        result.deferred = true;
        return result;
    }
    node_eval_t left = eval_node(node->left);
    node_eval_t right = eval_node(node->right);
    if (left.deferred || right.deferred) {
        result.deferred = true;
        return result;
    }
    switch (node->type) {
        case expr_type::ADD: {
            result.value = left.value + right.value;
            result.coeffs = left.coeffs;
            for (auto& [sec, c] : right.coeffs) {
                result.coeffs[sec] += c;
            }
            break;
        }
        case expr_type::SUB: {
            result.value = left.value - right.value;
            result.coeffs = left.coeffs;
            for (auto& [sec, c] : right.coeffs) {
                result.coeffs[sec] -= c;
            }
            break;
        }
        case expr_type::MUL:
        case expr_type::DIV: {
            if (!left.coeffs.empty() || !right.coeffs.empty()) {
                throw std::runtime_error("Relocatable or external symbol used in multiplication/division");
            }
            result.value = (node->type == expr_type::MUL)
                          ? left.value * right.value
                          : left.value / right.value;
            break;
        }
    }
    for (auto it = result.coeffs.begin(); it != result.coeffs.end(); ) {
        if (it->second == 0) {
            it = result.coeffs.erase(it);
        } else {
            ++it;
        }
    }
    return result;
}

as::expr_result_t as::assembler::try_eval_expression(const std::shared_ptr<expr_node_t>& expr) {
    expr_result_t out{};
    node_eval_t r;
    try {
        r = eval_node(expr);
    }
    catch (const std::exception&) {
        out.status = eval_status::INVALID;
        return out;
    }
    if (r.deferred) {
        out.status = eval_status::DEFERRED;
        return out;
    }
    if (r.coeffs.empty()) {
        out.status = eval_status::RESOLVED;
        out.section = SECTION_ABS;
        out.value = r.value;
        return out;
    }
    if (r.coeffs.size() == 1) {
        const auto& [sec, coeff] = *r.coeffs.begin();
        if (coeff == 1) {
            out.status = eval_status::RESOLVED;
            out.section = sec;
            out.value = r.value;
            return out;
        }
    }
    out.status = eval_status::INVALID;
    return out;
}

void as::assembler::finalize_equ_symbol(const std::string& symbol_name, const expr_result_t& result) {
    symbol_t& sym = m_sym_table[symbol_name];
    sym.name = symbol_name;
    sym.defined = true;
    sym.value = result.value;
    bool chains_to_extern = result.section.rfind(EXTERN_SECTION_PREFIX, 0) == 0;
    if (result.section == SECTION_ABS) {
        sym.absolute = true;
        sym.section = SECTION_ABS;
        sym.is_extern = false;
    }
    else if (chains_to_extern) {
        sym.absolute = false;
        sym.section = result.section;
        sym.is_extern = true;
        sym.global = true;
    }
    else {
        sym.absolute = false;
        sym.section = result.section;
        sym.is_extern = false;
    }
}

void as::assembler::collect_symbol_refs(const std::shared_ptr<expr_node_t>& node,
                                         std::vector<std::string>& out) {
    if (!node) {
        return;
    }
    if (!node->is_binary) {
        if (!node->is_literal) {
            out.push_back(node->symbol);
        }
        return;
    }
    collect_symbol_refs(node->left, out);
    collect_symbol_refs(node->right, out);
}

void as::assembler::resolve_pequs() {
    std::unordered_map<std::string, uint32_t> pequ_index;
    for (uint32_t i = 0; i < m_pequ_table.size(); i++) {
        pequ_index[m_pequ_table[i].symbol] = i;
    }
    std::function<void(uint32_t)> resolve_one = [&](uint32_t idx) {
        pending_equ_t& pequ = m_pequ_table[idx];
        if (pequ.state == equ_state::RESOLVED) { 
            return;
        }
        if (pequ.state == equ_state::VISITING) {
            throw std::runtime_error("Circular .equ dependency detected involving symbol " + pequ.symbol);
        }
        pequ.state = equ_state::VISITING;
        std::vector<std::string> deps;
        collect_symbol_refs(pequ.expr, deps);
        for (auto& dep : deps) {
            auto pit = pequ_index.find(dep);
            if (pit != pequ_index.end()) {
                resolve_one(pit->second);
                continue;
            }
            auto sit = m_sym_table.find(dep);
            if (sit == m_sym_table.end()) {
                symbol_t sym{};
                sym.name = dep;
                sym.section = SECTION_UNDEF;
                sym.is_extern = true;
                sym.global = true;
                m_sym_table.insert({dep, sym});
            }
            else if (!sit->second.defined && !sit->second.is_extern) {
                sit->second.is_extern = true;
                sit->second.global = true;
            }
        }
        expr_result_t result = try_eval_expression(pequ.expr);
        if (result.status == eval_status::INVALID) {
            throw std::runtime_error("Invalid relocation combination in .equ expression for symbol " + pequ.symbol);
        }
        if (result.status == eval_status::DEFERRED) {
            throw std::runtime_error("Unable to resolve .equ symbol " + pequ.symbol + ": unresolved dependency");
        }
        finalize_equ_symbol(pequ.symbol, result);
        pequ.state = equ_state::RESOLVED;
    };
    for (uint32_t i = 0; i < m_pequ_table.size(); i++) {
        resolve_one(i);
    }
    m_pequ_table.clear();
}

void as::assembler::normalize_extern_sections() {
    for (auto& [name, sym] : m_sym_table) {
        if (sym.section.rfind(EXTERN_SECTION_PREFIX, 0) == 0) {
            sym.section = SECTION_UNDEF;
            sym.is_extern = true;
            sym.global = true;
            sym.defined = false;
            sym.absolute = false;
        }
    }
}
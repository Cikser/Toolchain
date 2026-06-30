#include "as.hpp"
#include <stdexcept>

void as::assembler::dir_global(const std::vector<std::string>& symbols) {
    for (const auto& symbol_name : symbols) {
        auto it = m_sym_table.find(symbol_name);
        if (it != m_sym_table.end()) {
            it->second.global = true;
        }
        else {
            symbol_t sym{};
            sym.name = symbol_name;
            sym.global = true;
            sym.section = SECTION_UNDEF;
            m_sym_table.insert({symbol_name, sym});
        }
    }
}

void as::assembler::dir_extern(const std::vector<std::string>& symbols) {
    for (const auto& symbol_name : symbols) {
        auto it = m_sym_table.find(symbol_name);
        if (it != m_sym_table.end()) {
            if (it->second.defined || it->second.absolute) {
                throw std::runtime_error("Symbol " + symbol_name + " already defined");
            }
            else {
                it->second.global = true;
            }    
        }
        else {
            symbol_t sym{};
            sym.name = symbol_name;
            sym.section = SECTION_UNDEF;
            sym.global = true;
            sym.is_extern = true;
            m_sym_table.insert({symbol_name, sym});
        }
    }
}

void as::assembler::dir_section(const std::string& section_name) {
    check_pool(true);
    auto it = m_section_table.find(section_name);
    auto sym_it = m_sym_table.find(section_name);
    if (it == m_section_table.end()) {
        if (sym_it != m_sym_table.end() && (sym_it->second.defined || sym_it->second.is_extern)) {
            throw std::runtime_error("Symbol " + section_name + " already defined");
        }
        section_t section{};
        section.name = section_name;
        section.idx = m_sec_idx++;
        m_section_table.insert({section_name, section});
        m_current_section = section_name;
        symbol_t sym{};
        sym.name = section_name;
        sym.section = section_name;
        sym.defined = true;
        m_sym_table.insert({section_name, sym});
    }
    else {
        m_current_section = it->second.name;
    }
}

void as::assembler::dir_word(const std::vector<value_t>& values) {
    for (const auto& value : values) {
        if (std::holds_alternative<int32_t>(value)) {
            emit_word(std::get<int32_t>(value));
        }
        else {
            std::string symbol_name = std::get<std::string>(value);
            auto it = m_sym_table.find(symbol_name);
            if (it != m_sym_table.end() && it->second.defined && it->second.absolute) {
                emit_word(it->second.value);
            }
            else {
                if (it == m_sym_table.end()) {
                    symbol_t sym{};
                    sym.section = SECTION_UNDEF;
                    sym.name = symbol_name;
                    m_sym_table.insert({symbol_name, sym});
                }
                backpatch_t bp{};
                bp.offset = current_offset();
                bp.section_name = current_section().name;
                bp.symbol_name = symbol_name;
                bp.type = backpatch_type::RELOC;
                m_backpatch_table.push_back(bp);
                emit_word(0);
            }
        }
    }
}

void as::assembler::dir_skip(uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        emit_byte(0);
    }
}

void as::assembler::dir_ascii(const std::string& string) {
    for (const auto& ch : string) {
        emit_byte((uint8_t)ch);
    }
}

void as::assembler::dir_equ(const std::string& symbol_name, std::shared_ptr<expr_node_t> expr) {
    auto it = m_sym_table.find(symbol_name);
    if (it != m_sym_table.end() && (it->second.defined || it->second.is_extern)) {
        throw std::runtime_error("Symbol " + symbol_name + " already defined");
    }
    expr_result_t result = try_eval_expression(expr);
    if (result.status == eval_status::INVALID) {
        throw std::runtime_error("Invalid relocation combination in .equ expression for symbol " + symbol_name);
    }
    if (result.status == eval_status::RESOLVED) {
        finalize_equ_symbol(symbol_name, result);
        return;
    }
    if (it == m_sym_table.end()) {
        symbol_t sym{};
        sym.name = symbol_name;
        sym.section = SECTION_UNDEF;
        m_sym_table.insert({symbol_name, sym});
    }
    pending_equ_t pequ{};
    pequ.symbol = symbol_name;
    pequ.expr = std::move(expr);
    m_pequ_table.push_back(pequ);
}

void as::assembler::dir_end() {}
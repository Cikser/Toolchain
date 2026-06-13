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
    emit_byte((uint8_t)'\0');
}

void as::assembler::dir_equ(const std::string& symbol_name, std::shared_ptr<expr_node_t> expr) {
    auto it = m_sym_table.find(symbol_name);
    if (it != m_sym_table.end() && (it->second.defined || it->second.is_extern)) {
        throw std::runtime_error("Symbol " + symbol_name + " already defined");
    }
    eval_result_t result = try_eval_expr(expr);
    if (result.absolute) {
        if (it != m_sym_table.end()) {
            it->second.absolute = true;
            it->second.defined = true;
            it->second.value = result.value;
        }
        else {
            symbol_t sym{};
            sym.absolute = true;
            sym.name = symbol_name;
            sym.section = SECTION_ABS;
            sym.defined = true;
            sym.value = result.value;
            m_sym_table.insert({symbol_name, sym});
        }
    }
    else {
        if (it == m_sym_table.end()) {
            symbol_t sym{};
            sym.absolute = true;
            sym.name = symbol_name;
            sym.section = SECTION_ABS;
            m_sym_table.insert({symbol_name, sym});
        }
        pending_equ_t pequ{};
        pequ.expr = expr;
        pequ.symbol = symbol_name;
        m_pequ_table.push_back(pequ);
    }
}

void as::assembler::dir_end() {}
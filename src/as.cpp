#include "as.hpp"
#include <stdexcept>

as::assembler::assembler() {
    section_t section_undef{};
    section_undef.name = SECTION_UNDEF;
    m_section_table.insert({SECTION_UNDEF, section_undef});
    section_t section_abs{};
    section_abs.name = SECTION_ABS;
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
    }
    else {
        symbol_t sym{};
        sym.defined = true;
        sym.name = name;
        sym.section = current_section().name;
        sym.value = current_offset();
    }
}

void as::assembler::emit_byte(uint8_t byte) {
    current_section().data.push_back(byte);
}

void as::assembler::emit_word(uint32_t word) {
    emit_byte(word & (0xFF << 0));
    emit_byte(word & (0xFF << 8));
    emit_byte(word & (0xFF << 16));
    emit_byte(word & (0xFF << 24));
}

uint32_t as::assembler::current_offset() {
    return current_section().data.size();
}
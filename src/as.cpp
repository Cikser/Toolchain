#include "as.hpp"

void as::assembler::define_label(const std::string& name) {
    auto it = m_sym_table.find(name);
    if (it != m_sym_table.end()) {
        it->second.defined = true;
        it->second.section = m_current_section.name;
    }
}

void as::assembler::emit_byte(uint8_t byte) {
    m_current_section.data.push_back(byte);
}

void as::assembler::emit_word(uint32_t word) {
    emit_byte(word & (0xFF << 0));
    emit_byte(word & (0xFF << 8));
    emit_byte(word & (0xFF << 16));
    emit_byte(word & (0xFF << 24));
}

uint32_t as::assembler::current_offset() {
    return m_current_section.data.size();
}
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
    if (m_current_section == SECTION_UNDEF) {
        throw std::runtime_error("No active section, use .section before emitting data");
    }
    current_section().data.push_back(byte);
}

void as::assembler::emit_word(uint32_t word) {
    emit_byte(word & (0xFF << 0));
    emit_byte(word & (0xFF << 8));
    emit_byte(word & (0xFF << 16));
    emit_byte(word & (0xFF << 24));
}

void as::assembler::emit_instruction(uint32_t instr) {
    emit_word(instr);
}

static constexpr uint32_t INSTR_OFFSET = 4;
static constexpr uint32_t MODF_OFFSET = 0;
static constexpr uint32_t REGA_OFFSET = 12;
static constexpr uint32_t REGB_OFFSET = 8;
static constexpr uint32_t REGC_OFFSET = 20;

static constexpr uint32_t INSTR_MASK = 0xF;
static constexpr uint32_t MODF_MASK = 0xF;
static constexpr uint32_t REG_MASK = 0xF;
static constexpr uint32_t DISP_MASK = 0xFFF;

uint32_t as::assembler::encode_instruction(uint8_t oc, uint8_t mod, uint8_t regA,
                                        uint8_t regB, uint8_t regC, int16_t disp) {
        uint32_t instr = 0;
        instr |= ((uint32_t)(oc & INSTR_MASK) << INSTR_OFFSET);
        instr |= ((uint32_t)(mod & MODF_MASK) << MODF_OFFSET);
        instr |= ((uint32_t)(regA & REG_MASK) << REGA_OFFSET);
        instr |= ((uint32_t)(regB & REG_MASK) << REGB_OFFSET);
        instr |= ((uint32_t)(regC & REG_MASK) << REGC_OFFSET);
        instr |= ((uint32_t)(disp & DISP_MASK));
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
#include "as.hpp"
#include <stdexcept>
#include <fstream>
#include <algorithm>
#include <iomanip>

extern FILE *yyin;
extern int yyparse(as::assembler*);

void as::assembler::assemble(const std::string& input_path, const std::string& output_path) {
    FILE* f = fopen(input_path.c_str(), "r");
    if (!f)
        throw std::runtime_error("Cannot open input file: " + input_path);
    yyin = f;
    int result = yyparse(this);
    fclose(f);
    if (result != 0)
        throw std::runtime_error("Parse error in file: " + input_path);
    resolve_backpatch();
    // write_elf(output_path + ".o");
    write_dump(output_path + ".txt");
}

void as::assembler::write_dump(const std::string& path) {
    std::ofstream out(path);
    if (!out.is_open())
        throw std::runtime_error("Cannot open dump file: " + path);
    out << "=== SECTIONS ===\n";
    std::vector<section_t> sections;
    sections.reserve(m_section_table.size());
    for (auto& [key, value] : m_section_table) {
        sections.push_back(value);
    }
    auto comparator = [](const section_t& first, const section_t& second) {
        return first.idx < second.idx;
    };
    std::sort(sections.begin(), sections.end(), comparator);
    for (uint32_t i = 1; i < sections.size() - 1; i++) {
        const section_t& sec = sections[i];
        out << "\n[section: " << sec.name
        << ", index: " << sec.idx
        << ", size: " << std::dec << sec.data.size() << " bytes]\n";

        for (uint32_t off = 0; off < (uint32_t)sec.data.size(); off += 8) {
            out << std::hex << std::setw(4) << std::setfill('0') << off << ": ";
            for (uint32_t b = 0; b < 8; b++) {
                if (off + b < (uint32_t)sec.data.size()) {
                    out << std::hex << std::setw(2) << std::setfill('0')
                        << (uint32_t)sec.data[off + b] << " ";
                }
                else {
                    out << "   ";
                }
            }
            out << "\n";
        }
    }
    out << std::setfill(' ') << "\n=== SYMBOL TABLE ===\n";
    out << std::left
        << std::setw(5) << "Idx"
        << std::setw(20) << "Name"
        << std::setw(12) << "Value"
        << std::setw(16) << "Section"
        << std::setw(8) << "Global"
        << std::setw(8) << "Defined"
        << std::setw(8) << "Absolute"
        << "\n";
    uint32_t idx = 0;
    
    for (const auto& [name, sym] : m_sym_table) {
        out << std::left << std::dec
            << std::setw(5) << idx
            << std::setw(20) << sym.name;
        out << "0x" << std::right << std::hex << std::setw(8) << std::setfill('0') << sym.value;
        out << std::left << std::setfill(' ')
            << "  "
            << std::setw(16) << sym.section
            << std::setw(8) << (sym.global ? "yes" : "no")
            << std::setw(8) << (sym.defined ? "yes" : "no")
            << std::setw(8) << (sym.absolute ? "yes" : "no")
            << "\n";
        idx++;
    }
    for (uint32_t i = 1; i < (uint32_t)sections.size() - 1; i++) {
        const section_t& sec = sections[i];
        if (sec.relocations.empty())
            continue;

        out << "\n=== RELOCATIONS [" << sec.name << "] ===\n";
        out << std::left
            << std::setw(12) << "Offset"
            << std::setw(20) << "Symbol"
            << std::setw(12) << "Type"
            << "Addend"
            << "\n";

        for (const auto& rel : sec.relocations) {
            out << "0x" << std::right << std::hex << std::setw(8) << std::setfill('0') << rel.offset;
            out << std::left << std::setfill(' ') << "  "
                << std::setw(20) << rel.symbol_name
                << std::setw(12) << (rel.type == relocation_type::R_32 ? "R_32" : "R_PC_REL")
                << std::dec << rel.addend
                << "\n";
        }
    }
    out.close();
}
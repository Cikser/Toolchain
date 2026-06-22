#include "ld.hpp"
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <elf.h>
#include <iostream>
#include <memory>
#include <set>

ld::linker::linker() {
    section_t section_undef{};
    section_undef.name = SECTION_UNDEF;
    section_undef.idx = SECTION_UNDEF_IDX;
    m_section_table.push_back(section_undef);
}

void ld::linker::extract_sections_and_symbols(const std::string& input_path, 
        std::vector<section_t>& out_sections, std::vector<symbol_t>& out_symbols) {
    std::ifstream file(input_path);
    if (!file.is_open()) {
        throw std::runtime_error("File " + input_path + " does not exist");
    }
    Elf32_Ehdr elf_header;
    file.read((char*)&elf_header, sizeof(Elf32_Ehdr));
    if (!(elf_header.e_ident[EI_MAG0] == ELFMAG0 && elf_header.e_ident[EI_MAG1] == ELFMAG1
        && elf_header.e_ident[EI_MAG2] == ELFMAG2 && elf_header.e_ident[EI_MAG3] == ELFMAG3)) {
        throw std::runtime_error("File " + input_path + " in wrong format");  
    }
    // todo format checks
    if (elf_header.e_type != ET_REL) {
        throw std::runtime_error("File " + input_path + " not relocatable");
    }
    uint32_t sh_off = elf_header.e_shoff;
    uint32_t sh_size = elf_header.e_shentsize;
    uint32_t sh_num = elf_header.e_shnum;
    uint32_t shstr_idx = elf_header.e_shstrndx;
    Elf32_Shdr shstr_hdr;
    file.seekg(sh_off + shstr_idx * sh_size);
    file.read((char*)&shstr_hdr, sizeof(Elf32_Shdr));
    auto shstr_section = std::make_unique<uint8_t[]>(shstr_hdr.sh_size);
    file.seekg(shstr_hdr.sh_offset);
    file.read((char*)shstr_section.get(), shstr_hdr.sh_size);
    Elf32_Shdr symtab_hdr;
    Elf32_Shdr strtab_hdr;
    std::vector<std::pair<Elf32_Shdr, std::string>> rela_shdrs;
    for (uint32_t i = 1; i < sh_num; i++) {
        Elf32_Shdr shdr;
        file.seekg(sh_off + i * sh_size);
        file.read((char*)&shdr, sizeof(Elf32_Shdr));
        if (shdr.sh_type == SHT_SYMTAB) {
            memcpy(&symtab_hdr, &shdr, sizeof(Elf32_Shdr));
            continue;
        }
        if (shdr.sh_type == SHT_STRTAB && i != shstr_idx) {
            memcpy(&strtab_hdr, &shdr, sizeof(Elf32_Shdr));
            continue;
        }
        if (shdr.sh_type == SHT_STRTAB) {
            continue;
        }
        section_t section{};
        char buf[256];
        strcpy(buf, (char*)shstr_section.get() + shdr.sh_name);
        if (shdr.sh_type == SHT_RELA) {
            rela_shdrs.push_back({shdr, buf});
            continue;
        }
        section.name = buf;
        file.seekg(shdr.sh_offset);
        section.data.reserve(shdr.sh_size);
        file.read((char*)section.data.data(), shdr.sh_size);
        out_sections.push_back(section);
    }
    auto strtab_section = std::make_unique<uint8_t[]>(strtab_hdr.sh_size);
    file.seekg(strtab_hdr.sh_offset);
    file.read((char*)strtab_section.get(), strtab_hdr.sh_size);
    for (uint32_t i = 1; i < symtab_hdr.sh_size / symtab_hdr.sh_entsize - 1; i++) {
        Elf32_Sym elf_sym;
        file.seekg(symtab_hdr.sh_offset + i * symtab_hdr.sh_entsize);
        file.read((char*)&elf_sym, sizeof(Elf32_Sym));
        symbol_t sym{};
        char buf[256];
        strcpy(buf, (char*)strtab_section.get() + elf_sym.st_name);
        sym.name = buf;
        sym.global = ELF32_ST_BIND(elf_sym.st_info);
        sym.absolute = elf_sym.st_shndx == SHN_ABS;
        sym.defined = elf_sym.st_shndx == SHN_UNDEF;
        sym.value = (int32_t)elf_sym.st_value;
        sym.section = elf_sym.st_shndx == SHN_ABS ? SECTION_ABS
                : (elf_sym.st_shndx == SHN_UNDEF ? SECTION_UNDEF 
                : out_sections[elf_sym.st_shndx - 1].name);
        sym.is_extern = sym.global && !sym.defined;
        out_symbols.push_back(sym);
    }
    for (const auto& [rela_shdr, name] : rela_shdrs) {
        std::string section_name = name.substr(5, name.length() - 5);
        section_t* sec_ptr = nullptr;
        for (auto& sec : out_sections) {
            if (sec.name == section_name) {
                sec_ptr = &sec;
                break;
            }
        }
        section_t& section = *sec_ptr;
        for (uint32_t i = 0; i < rela_shdr.sh_size / rela_shdr.sh_entsize; i++) {
            relocation_t rel{};
            Elf32_Rela elf_rel;
            file.seekg(rela_shdr.sh_offset + i * rela_shdr.sh_entsize);
            file.read((char*)&elf_rel, sizeof(Elf32_Rela));
            rel.section_name = section_name;
            rel.type = relocation_type::R_32;
            rel.offset = elf_rel.r_offset;
            rel.symbol_name = out_symbols[ELF32_R_SYM(elf_rel.r_info) - 1].name;
            rel.addend = elf_rel.r_addend;
            section.relocations.push_back(rel);
        }
    }
}

uint32_t ld::linker::find_section(const std::string& name) {
    for (uint32_t i = 0; i < m_section_table.size(); i++) {
        if (name == m_section_table[i].name) {
            return i;
        }
    }
    return -1;
}

void ld::linker::merge_sections_and_symbols(std::vector<section_t>& sections, std::vector<symbol_t>& symbols) {
    std::unordered_map<std::string, uint32_t> offset_table;
    std::set<std::string> section_set;
    for (auto& section : sections) {
        section_set.insert(section.name);
        uint32_t idx = find_section(section.name);
        if (idx == -1) {
            continue;
        }
        uint32_t offset = m_section_table[idx].data.size();
        for (auto& sym : symbols) {
            if (sym.section == section.name) {
                sym.value += offset;
            }
        }
        for (auto& reloc : section.relocations) {
            reloc.offset += offset;
        }
        offset_table.insert({section.name, offset});
    }
    for (auto& section : sections) {
        for (auto& rel : section.relocations) {
            auto it = offset_table.find(rel.symbol_name);
            if (it == offset_table.end()) {
                continue;
            }
            rel.addend += it->second;
        }
    }
    for (auto& section : sections) {
        uint32_t idx = find_section(section.name);
        if (idx == -1) {
            section.idx = m_section_table.size();
            m_section_table.push_back(section);
            continue;
        }
        section_t& present = m_section_table[idx];
        present.data.reserve(present.data.size() + section.data.size());
        present.data.insert(present.data.end(), section.data.begin(), section.data.end());
        present.relocations.reserve(present.relocations.size() + section.relocations.size());
        present.relocations.insert(present.relocations.end(), section.relocations.begin(), present.relocations.end());
    }
    for (auto& sym : symbols) {
        if (!sym.global) {
            auto it = m_sym_table.find(sym.name);
            if (it != m_sym_table.end() && section_set.contains(sym.name)) {
                continue;
            }
            else if (it == m_sym_table.end() && section_set.contains(sym.name)) {
                sym.section_idx = find_section(sym.name);
                m_sym_table.insert({sym.name, sym});
                continue;
            }
            uint32_t idx = find_section(sym.section);
            sym.section_idx = idx;
            if (sym.section == SECTION_ABS) {
                uint32_t idx = find_section(sections[0].name);
            }
            m_section_table[idx].sym_table.insert({sym.name, sym});
        }
        else {
            auto it = m_sym_table.find(sym.name);
            if (it == m_sym_table.end()) {
                if (sym.section == SECTION_ABS) {
                    sym.section_idx = SECTION_ABS_IDX;
                }
                else {
                    sym.section_idx = find_section(sym.section);
                }
                m_sym_table.insert({sym.name, sym});
            }
            else {
                symbol_t& present = it->second;
                if (present.defined && sym.defined) {
                    throw std::runtime_error("Symbol " + sym.name + " defined multiple times");
                }
                if (!present.defined && sym.defined) {
                    present.defined = true;
                    present.value = sym.value;
                    present.absolute = sym.absolute;
                    if (sym.section == SECTION_ABS) {
                        present.section_idx = SECTION_ABS_IDX;
                    }
                    else {
                        present.section_idx = find_section(sym.section);
                    }
                    present.section = sym.section;
                }
            }
        } 
    }
}
#include "ld.hpp"
#include <stdexcept>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <elf.h>
#include <cstring>


void ld::linker::link(const std::vector<std::string>& input_paths, const std::string& output_path,
                    ld::output_type type, std::vector<place_t>& place_requests) {
    m_output_type = type;
    first_pass(input_paths, place_requests);
    second_pass();
    write_dump(output_path + ".txt");
    write_elf(output_path + (m_output_type == output_type::HEX ? ".hex" : ".o"));
}

void ld::linker::write_dump(const std::string& path) {
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("Cannot open dump file: " + path);
    }
    out << "=== SECTIONS ===\n";
    for (uint32_t i = 1; i < (uint32_t)m_section_table.size(); i++) {
        const section_t& sec = m_section_table[i];
        out << "\n[section: " << sec.name
            << ", index: " << i
            << ", address: 0x" << std::hex << std::setw(8) << std::setfill('0') << sec.address
            << ", size: " << std::dec << sec.data.size() << " bytes]\n";
        for (uint32_t off = 0; off < (uint32_t)sec.data.size(); off += 8) {
            out << std::hex << std::setw(4) << std::setfill('0') << off << ": ";
            for (uint32_t b = 0; b < 8; b++) {
                if (off + b < (uint32_t)sec.data.size())
                    out << std::hex << std::setw(2) << std::setfill('0')
                        << (uint32_t)sec.data[off + b] << " ";
                else
                    out << "   ";
            }
            out << "\n";
        }
    }
    out << std::setfill(' ') << "\n=== SYMBOL TABLE ===\n";
    out << std::left
        << std::setw(5)  << "Idx"
        << std::setw(20) << "Name"
        << std::setw(12) << "Value"
        << std::setw(16) << "Section"
        << std::setw(8)  << "Global"
        << std::setw(8)  << "Defined"
        << std::setw(8)  << "Absolute"
        << "\n";
    uint32_t idx = 0;
    for (const auto& [name, sym] : m_sym_table) {
        out << std::left << std::dec
            << std::setw(5)  << idx
            << std::setw(20) << sym.name;
        out << "0x" << std::right << std::hex << std::setw(8) << std::setfill('0') << sym.value;
        out << std::left << std::setfill(' ')
            << "  "
            << std::setw(16) << sym.section
            << std::setw(8)  << (sym.global   ? "yes" : "no")
            << std::setw(8)  << (sym.defined  ? "yes" : "no")
            << std::setw(8)  << (sym.absolute ? "yes" : "no")
            << "\n";
        idx++;
    }
    for (uint32_t i = 1; i < (uint32_t)m_section_table.size(); i++) {
        for (const auto& [name, sym] : m_section_table[i].sym_table) {
            out << std::left << std::dec
                << std::setw(5)  << idx
                << std::setw(20) << sym.name;
            out << "0x" << std::right << std::hex << std::setw(8) << std::setfill('0') << sym.value;
            out << std::left << std::setfill(' ')
                << "  "
                << std::setw(16) << sym.section
                << std::setw(8)  << "no"
                << std::setw(8)  << (sym.defined  ? "yes" : "no")
                << std::setw(8)  << (sym.absolute ? "yes" : "no")
                << "\n";
            idx++;
        }
    }
    out.close();
}

namespace ld {
    struct strtab_builder {
        std::vector<uint8_t> data;
        strtab_builder() { data.push_back('\0'); }
        uint32_t add(const std::string& s) {
            uint32_t i = (uint32_t)data.size();
            data.insert(data.end(), s.begin(), s.end());
            data.push_back('\0');
            return i;
        }
    };

    struct sym_entry_t {
        const symbol_t* sym;
        uint32_t name_idx;
    };
}

void ld::linker::write_elf(const std::string& path) {
    const bool is_rel = (m_output_type == output_type::RELOCATABLE);
    strtab_builder strtab;
    strtab_builder shstrtab;
    std::vector<sym_entry_t> locals, globals;
    for (uint32_t i = 1; i < (uint32_t)m_section_table.size(); i++) {
        for (const auto& [name, sym] : m_section_table[i].sym_table) {
            locals.push_back({ &sym, strtab.add(name) });
        }
    }
    for (const auto& [name, sym] : m_sym_table) {
        bool is_section_sym = (sym.section == sym.name) && sym.defined;
        if (is_section_sym) {
            locals.push_back({ &sym, strtab.add(name) });
        } 
        else {
            globals.push_back({ &sym, strtab.add(name) });
        }
    }
    std::unordered_map<std::string, uint32_t> sym_name_to_idx;
    uint32_t idx = 1;
    for (uint32_t i = 1; i < (uint32_t)m_section_table.size(); i++) {
        for (const auto& [name, sym] : m_section_table[i].sym_table) {
            sym_name_to_idx[name] = idx++;
        }
    }
    for (const auto& [name, sym] : m_sym_table) {
        sym_name_to_idx[name] = idx++;
    }
    std::vector<uint32_t> user_sec_indices;
    for (uint32_t i = 1; i < (uint32_t)m_section_table.size(); i++) {
        user_sec_indices.push_back(i);
    }
    std::vector<uint32_t> rela_sec_indices;
    if (is_rel) {
        for (uint32_t i = 0; i < (uint32_t)user_sec_indices.size(); i++) {
            uint32_t si = user_sec_indices[i];
            if (!m_section_table[si].relocations.empty()) {
                rela_sec_indices.push_back(i);
            }
        }
    }
    uint32_t first_user = 1;
    uint32_t num_user = (uint32_t)user_sec_indices.size();
    uint32_t num_rela = (uint32_t)rela_sec_indices.size();
    uint32_t symtab_shidx = first_user + num_user + num_rela;
    uint32_t strtab_shidx = symtab_shidx + 1;
    uint32_t shstrtab_shidx = strtab_shidx + 1;
    uint32_t total_shdrs = shstrtab_shidx + 1;
    shstrtab.add("");
    uint32_t sh_symtab_name = shstrtab.add(".symtab");
    uint32_t sh_strtab_name = shstrtab.add(".strtab");
    uint32_t sh_shstrtab_name = shstrtab.add(".shstrtab");
    std::vector<uint32_t> user_shnames;
    for (uint32_t si : user_sec_indices) {
        user_shnames.push_back(shstrtab.add(m_section_table[si].name));
    }
    std::vector<uint32_t> rela_shnames;
    if (is_rel) {
        for (uint32_t ui : rela_sec_indices) {
            uint32_t si = user_sec_indices[ui];
            rela_shnames.push_back(shstrtab.add(".rela" + m_section_table[si].name));
        }
    }
    auto shndx_for = [&](const symbol_t& sym) -> Elf32_Half {
        if (sym.section == SECTION_UNDEF) return SHN_UNDEF;
        if (sym.section == SECTION_ABS || sym.absolute) return SHN_ABS;
        uint32_t sidx = sym.section_idx;
        for (uint32_t i = 0; i < num_user; i++) {
            if (user_sec_indices[i] == sidx) {
                return (Elf32_Half)(first_user + i);
            }
        }
        return SHN_UNDEF;
    };

    std::vector<Elf32_Sym> symtab_data;
    symtab_data.push_back(Elf32_Sym{});
    auto push_sym = [&](const sym_entry_t& e, bool global) {
        Elf32_Sym es{};
        es.st_name = e.name_idx;
        es.st_value = (Elf32_Addr)e.sym->value;
        es.st_size = 0;
        es.st_info = ELF32_ST_INFO(global ? STB_GLOBAL : STB_LOCAL, STT_NOTYPE);
        es.st_other = STV_DEFAULT;
        es.st_shndx = shndx_for(*e.sym);
        symtab_data.push_back(es);
    };
    for (auto& e : locals)  { 
        push_sym(e, false); 
    }
    uint32_t first_global_sym = (uint32_t)symtab_data.size();
    for (auto& e : globals) { 
        push_sym(e, true);
    }
    std::vector<std::vector<Elf32_Rela>> rela_data;
    if (is_rel) {
        for (uint32_t ui : rela_sec_indices) {
            uint32_t si = user_sec_indices[ui];
            std::vector<Elf32_Rela> entries;
            for (const auto& rel : m_section_table[si].relocations) {
                Elf32_Rela er{};
                er.r_offset = (Elf32_Addr)rel.offset;
                uint32_t sym_idx = 0;
                auto it = sym_name_to_idx.find(rel.symbol_name);
                if (it != sym_name_to_idx.end()) {
                    sym_idx = it->second;
                }
                er.r_info = ELF32_R_INFO(sym_idx, R_X86_64_32);
                er.r_addend = (Elf32_Sword)rel.addend;
                entries.push_back(er);
            }
            rela_data.push_back(std::move(entries));
        }
    }
    uint32_t ehdr_size = sizeof(Elf32_Ehdr);
    uint32_t phdr_size = sizeof(Elf32_Phdr);
    uint32_t shdr_size = sizeof(Elf32_Shdr);
    uint32_t sym_size = sizeof(Elf32_Sym);
    uint32_t rela_size = sizeof(Elf32_Rela);
    uint32_t num_phdrs = is_rel ? 0 : (uint32_t)user_sec_indices.size();
    uint32_t offset = ehdr_size + num_phdrs * phdr_size;
    std::vector<uint32_t> sec_file_offsets(m_section_table.size(), 0);
    for (uint32_t si : user_sec_indices) {
        sec_file_offsets[si] = offset;
        offset += (uint32_t)m_section_table[si].data.size();
    }
    std::vector<uint32_t> rela_file_offsets;
    if (is_rel) {
        for (uint32_t i = 0; i < num_rela; i++) {
            rela_file_offsets.push_back(offset);
            offset += (uint32_t)rela_data[i].size() * rela_size;
        }
    }
    uint32_t symtab_offset = offset;
    uint32_t symtab_bytes = (uint32_t)symtab_data.size() * sym_size;
    offset += symtab_bytes;
    uint32_t strtab_offset = offset;
    uint32_t strtab_bytes = (uint32_t)strtab.data.size();
    offset += strtab_bytes;
    uint32_t shstrtab_offset = offset;
    uint32_t shstrtab_bytes = (uint32_t)shstrtab.data.size();
    offset += shstrtab_bytes;
    uint32_t shoff = offset;
    Elf32_Addr entry = 0;
    if (!is_rel) {
        auto it_start = m_sym_table.find("_start");
        if (it_start != m_sym_table.end() && it_start->second.defined) {
            const symbol_t& s = it_start->second;
            entry = (Elf32_Addr)s.value;
            if (!s.absolute && s.section_idx < m_section_table.size()) {
                entry += (Elf32_Addr)m_section_table[s.section_idx].address;
            }
        } 
        else if (!user_sec_indices.empty()) {
            entry = (Elf32_Addr)m_section_table[user_sec_indices[0]].address;
        }
    }
    Elf32_Ehdr ehdr{};
    ehdr.e_ident[EI_MAG0] = ELFMAG0;
    ehdr.e_ident[EI_MAG1] = ELFMAG1;
    ehdr.e_ident[EI_MAG2] = ELFMAG2;
    ehdr.e_ident[EI_MAG3] = ELFMAG3;
    ehdr.e_ident[EI_CLASS] = ELFCLASS32;
    ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
    ehdr.e_ident[EI_VERSION] = EV_CURRENT;
    ehdr.e_ident[EI_OSABI] = ELFOSABI_NONE;
    ehdr.e_type = is_rel ? ET_REL : ET_EXEC;
    ehdr.e_machine = EM_NONE;
    ehdr.e_version = EV_CURRENT;
    ehdr.e_entry = entry;
    ehdr.e_phoff = is_rel ? 0 : (Elf32_Off)ehdr_size;
    ehdr.e_shoff = (Elf32_Off)shoff;
    ehdr.e_flags = 0;
    ehdr.e_ehsize = (Elf32_Half)ehdr_size;
    ehdr.e_phentsize = is_rel ? 0 : (Elf32_Half)phdr_size;
    ehdr.e_phnum = (Elf32_Half)num_phdrs;
    ehdr.e_shentsize = (Elf32_Half)shdr_size;
    ehdr.e_shnum = (Elf32_Half)total_shdrs;
    ehdr.e_shstrndx = (Elf32_Half)shstrtab_shidx;
    std::vector<Elf32_Shdr> shdrs(total_shdrs);
    memset(shdrs.data(), 0, total_shdrs * shdr_size);
    for (uint32_t i = 0; i < num_user; i++) {
        uint32_t si = user_sec_indices[i];
        Elf32_Shdr& sh = shdrs[first_user + i];
        sh.sh_name = user_shnames[i];
        sh.sh_type = SHT_PROGBITS;
        sh.sh_flags = SHF_ALLOC;
        sh.sh_addr = is_rel ? 0 : (Elf32_Addr)m_section_table[si].address;
        sh.sh_offset = (Elf32_Off)sec_file_offsets[si];
        sh.sh_size = (Elf32_Word)m_section_table[si].data.size();
        sh.sh_addralign = 1;
    }
    if (is_rel) {
        for (uint32_t i = 0; i < num_rela; i++) {
            uint32_t ui = rela_sec_indices[i];
            Elf32_Shdr& sh = shdrs[first_user + num_user + i];
            sh.sh_name = rela_shnames[i];
            sh.sh_type = SHT_RELA;
            sh.sh_flags = 0;
            sh.sh_offset = (Elf32_Off)rela_file_offsets[i];
            sh.sh_size = (Elf32_Word)(rela_data[i].size() * rela_size);
            sh.sh_link = (Elf32_Word)symtab_shidx;
            sh.sh_info = (Elf32_Word)(first_user + ui);
            sh.sh_addralign = 4;
            sh.sh_entsize = (Elf32_Word)rela_size;
        }
    }

    {
        Elf32_Shdr& sh = shdrs[symtab_shidx];
        sh.sh_name = sh_symtab_name;
        sh.sh_type = SHT_SYMTAB;
        sh.sh_offset = (Elf32_Off)symtab_offset;
        sh.sh_size = (Elf32_Word)symtab_bytes;
        sh.sh_link = (Elf32_Word)strtab_shidx;
        sh.sh_info = (Elf32_Word)first_global_sym;
        sh.sh_addralign = 4;
        sh.sh_entsize = (Elf32_Word)sym_size;
    }
    {
        Elf32_Shdr& sh = shdrs[strtab_shidx];
        sh.sh_name = sh_strtab_name;
        sh.sh_type = SHT_STRTAB;
        sh.sh_offset = (Elf32_Off)strtab_offset;
        sh.sh_size = (Elf32_Word)strtab_bytes;
        sh.sh_addralign = 1;
    }
    {
        Elf32_Shdr& sh = shdrs[shstrtab_shidx];
        sh.sh_name = sh_shstrtab_name;
        sh.sh_type = SHT_STRTAB;
        sh.sh_offset = (Elf32_Off)shstrtab_offset;
        sh.sh_size = (Elf32_Word)shstrtab_bytes;
        sh.sh_addralign = 1;
    }
    std::ofstream fout(path, std::ios::binary | std::ios::trunc);
    if (!fout) {
        throw std::runtime_error("Cannot open output file: " + path);
    }
    fout.write((char*)&ehdr, ehdr_size);
    if (!is_rel) {
        std::vector<Elf32_Phdr> phdrs(num_phdrs);
        for (uint32_t i = 0; i < num_phdrs; i++) {
            uint32_t si = user_sec_indices[i];
            Elf32_Phdr& ph = phdrs[i];
            ph.p_type = PT_LOAD;
            ph.p_offset = (Elf32_Off)sec_file_offsets[si];
            ph.p_vaddr = (Elf32_Addr)m_section_table[si].address;
            ph.p_paddr = (Elf32_Addr)m_section_table[si].address;
            ph.p_filesz = (Elf32_Word)m_section_table[si].data.size();
            ph.p_memsz = (Elf32_Word)m_section_table[si].data.size();
            ph.p_flags = PF_R | PF_W | PF_X;
            ph.p_align = 1;
            fout.write((char*)&ph, phdr_size);
        }
    }
    for (uint32_t si : user_sec_indices) {
        const auto& data = m_section_table[si].data;
        if (!data.empty()) {
            fout.write((char*)data.data(), data.size());
        }
    }
    if (is_rel) {
        for (uint32_t i = 0; i < num_rela; i++) {
            fout.write((char*)rela_data[i].data(), rela_data[i].size() * rela_size);
        }
    }
    fout.write((char*)symtab_data.data(), symtab_bytes);
    fout.write((char*)strtab.data.data(), strtab_bytes);
    fout.write((char*)shstrtab.data.data(), shstrtab_bytes);
    fout.write((char*)shdrs.data(), total_shdrs * shdr_size);
    if (!fout) {
        throw std::runtime_error("Write error on output file: " + path);
    }
}
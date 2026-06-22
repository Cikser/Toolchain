#include "as.hpp"
#include <stdexcept>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <elf.h>
#include <cstring>

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
    resolve_symbols();
    resolve_pequs();
    resolve_backpatch();
    write_elf(output_path + ".o");
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



namespace as {
    class strtab_builder_t {
    public:
        std::vector<uint8_t> data;
 
        strtab_builder_t() {
            data.push_back('\0');
        }
 
        uint32_t add(const std::string& s) {
            uint32_t idx = (uint32_t)(data.size());
            data.insert(data.end(), s.begin(), s.end());
            data.push_back('\0');
            return idx;
        }
    };

    struct sym_entry {
        std::string name;
        const symbol_t* sym;
        uint32_t name_idx;
    };
}



void as::assembler::write_elf(const std::string& output_path) {
    std::vector<std::string> sec_names;
    for (auto& [name, sec] : m_section_table) {
        if (name == SECTION_UNDEF || name == SECTION_ABS)
            continue;
        sec_names.push_back(name);
    }
    std::sort(sec_names.begin(), sec_names.end());
    strtab_builder_t strtab;

    std::vector<sym_entry> locals, globals;
    for (auto& [sname, sym] : m_sym_table) {
        sym_entry e;
        e.name = sname;
        e.sym = &sym;
        e.name_idx = strtab.add(sname);
        if (sym.global) {
            globals.push_back(e);
        }
        else {
            locals.push_back(e);
        }
    }
    auto by_name = [](const sym_entry& a, const sym_entry& b){
        return a.name < b.name;
    };
    std::sort(locals.begin(),  locals.end(),  by_name);
    std::sort(globals.begin(), globals.end(), by_name);

    std::unordered_map<std::string, uint32_t> sec_idx_map;
    uint32_t idx = 1;
    for (auto& sn : sec_names) {
        sec_idx_map[sn] = idx++;
    }

    std::vector<std::string> rela_secs;
    for (auto& sn : sec_names) {
        if (!m_section_table.at(sn).relocations.empty()) {
            rela_secs.push_back(sn);
        }
    }
    uint32_t rela_base_idx = idx;
    std::unordered_map<std::string, uint32_t> rela_idx_map;
    for (auto& sn : rela_secs) {
        rela_idx_map[sn] = idx++;
    }

    uint32_t symtab_idx = idx++;
    uint32_t strtab_idx = idx++;
    uint32_t shstrtab_idx = idx;

    auto shndx_for = [&](const symbol_t& sym) -> Elf32_Half {
        if (sym.section == SECTION_UNDEF) return SHN_UNDEF;
        if (sym.section == SECTION_ABS) return SHN_ABS;
        auto it = sec_idx_map.find(sym.section);
        if (it == sec_idx_map.end()) return SHN_UNDEF;
        return (Elf32_Half)(it->second);
    };

    auto stt_for = [&](const symbol_t& sym) -> uint8_t {
        if (sec_idx_map.count(sym.name) && !sym.is_extern && sym.defined) {
            return STT_SECTION;
        }
        return STT_NOTYPE;
    };

    std::vector<Elf32_Sym> symtab_data;

    Elf32_Sym undef_sym{};
    symtab_data.push_back(undef_sym);

    std::unordered_map<std::string, uint32_t> sym_idx_map;

    auto push_sym = [&](const sym_entry& e) {
        Elf32_Sym es{};
        es.st_name = e.name_idx;
        es.st_value = (Elf32_Addr)(e.sym->value);
        es.st_size = 0;
        uint8_t bind = e.sym->global ? STB_GLOBAL : STB_LOCAL;
        uint8_t type = stt_for(*e.sym);
        es.st_info = ELF32_ST_INFO(bind, type);
        es.st_other = STV_DEFAULT;
        es.st_shndx = shndx_for(*e.sym);
 
        sym_idx_map[e.name] = (uint32_t)(symtab_data.size());
        symtab_data.push_back(es);
    };

    for (auto& e : locals) push_sym(e);
    uint32_t first_global = (uint32_t)(symtab_data.size());
    for (auto& e : globals) push_sym(e);

    strtab_builder_t shstrtab;
    uint32_t sh_null_name = shstrtab.add("");
    uint32_t sh_symtab_name = shstrtab.add(".symtab");
    uint32_t sh_strtab_name = shstrtab.add(".strtab");
    uint32_t sh_shstrtab_name= shstrtab.add(".shstrtab");

    std::unordered_map<std::string, uint32_t> sec_shname;
    for (auto& sn : sec_names) {
        sec_shname[sn] = shstrtab.add(sn);
    }

    std::unordered_map<std::string, uint32_t> rela_shname;
    for (auto& sn : rela_secs) {
        rela_shname[sn] = shstrtab.add(".rela" + sn);
    }

    uint32_t ehdr_size = sizeof(Elf32_Ehdr);
    uint32_t shdr_size = sizeof(Elf32_Shdr);
    uint32_t sym_size = sizeof(Elf32_Sym);
    uint32_t rela_size = sizeof(Elf32_Rela);
    uint32_t total_shdrs = shstrtab_idx + 1;
    
    uint32_t offset = ehdr_size;

    std::unordered_map<std::string, uint32_t> sec_offset;
    for (auto& sn : sec_names) {
        sec_offset[sn] = offset;
        offset += (uint32_t)(m_section_table.at(sn).data.size());
    }

    std::unordered_map<std::string, uint32_t> rela_offset;
    for (auto& sn : rela_secs) {
        rela_offset[sn] = offset;
        offset += (uint32_t)(m_section_table.at(sn).relocations.size()) * rela_size;
    }

    uint32_t symtab_offset = offset;
    uint32_t symtab_bytes = (uint32_t)(symtab_data.size()) * sym_size;
    offset += symtab_bytes;

    uint32_t strtab_offset = offset;
    uint32_t strtab_bytes = (uint32_t)(strtab.data.size());
    offset += strtab_bytes;

    uint32_t shstrtab_offset = offset;
    uint32_t shstrtab_bytes = (uint32_t)(shstrtab.data.size());
    offset += shstrtab_bytes;

    uint32_t shoff = offset;

    Elf32_Ehdr ehdr{};
    ehdr.e_ident[EI_MAG0] = ELFMAG0;
    ehdr.e_ident[EI_MAG1] = ELFMAG1;
    ehdr.e_ident[EI_MAG2] = ELFMAG2;
    ehdr.e_ident[EI_MAG3] = ELFMAG3;
    ehdr.e_ident[EI_CLASS] = ELFCLASS32;
    ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
    ehdr.e_ident[EI_VERSION] = EV_CURRENT;
    ehdr.e_ident[EI_OSABI] = ELFOSABI_NONE;
    ehdr.e_type = ET_REL;
    ehdr.e_machine = EM_X86_64;
    ehdr.e_version = EV_CURRENT;
    ehdr.e_entry = 0;
    ehdr.e_phoff = 0;
    ehdr.e_shoff = shoff;
    ehdr.e_flags = 0;
    ehdr.e_ehsize = (Elf32_Half)(ehdr_size);
    ehdr.e_phentsize = 0;
    ehdr.e_phnum = 0;
    ehdr.e_shentsize = (Elf32_Half)(shdr_size);
    ehdr.e_shnum = (Elf32_Half)(total_shdrs);
    ehdr.e_shstrndx = (Elf32_Half)(shstrtab_idx);

    std::vector<Elf32_Shdr> shdrs(total_shdrs);
    memset(shdrs.data(), 0, total_shdrs * shdr_size);

    for (auto& sn : sec_names) {
        uint32_t i = sec_idx_map.at(sn);
        Elf32_Shdr& sh = shdrs[i];
        sh.sh_name = sec_shname.at(sn);
        sh.sh_type = SHT_PROGBITS;
        sh.sh_flags = SHF_ALLOC;
        sh.sh_addr = 0;
        sh.sh_offset = sec_offset.at(sn);
        sh.sh_size = (Elf32_Word)(m_section_table.at(sn).data.size());
        sh.sh_link = 0;
        sh.sh_info = 0;
        sh.sh_addralign = 1;
        sh.sh_entsize = 0;
    }

    for (auto& sn : rela_secs) {
        uint32_t i = rela_idx_map.at(sn);
        Elf32_Shdr& sh = shdrs[i];
        sh.sh_name = rela_shname.at(sn);
        sh.sh_type = SHT_RELA;
        sh.sh_flags = 0;
        sh.sh_addr = 0;
        sh.sh_offset = rela_offset.at(sn);
        sh.sh_size = (Elf32_Word)(m_section_table.at(sn).relocations.size() * rela_size);
        sh.sh_link = symtab_idx;
        sh.sh_info = sec_idx_map.at(sn);
        sh.sh_addralign = 4;
        sh.sh_entsize   = (Elf32_Word)(rela_size);
    }

    {
        Elf32_Shdr& sh = shdrs[symtab_idx];
        sh.sh_name = sh_symtab_name;
        sh.sh_type = SHT_SYMTAB;
        sh.sh_flags = 0;
        sh.sh_addr = 0;
        sh.sh_offset = symtab_offset;
        sh.sh_size = symtab_bytes;
        sh.sh_link = strtab_idx;
        sh.sh_info = first_global;
        sh.sh_addralign = 4;
        sh.sh_entsize = (Elf32_Word)(sym_size);
    }

    {
        Elf32_Shdr& sh = shdrs[strtab_idx];
        sh.sh_name = sh_strtab_name;
        sh.sh_type = SHT_STRTAB;
        sh.sh_flags = 0;
        sh.sh_addr = 0;
        sh.sh_offset = strtab_offset;
        sh.sh_size = strtab_bytes;
        sh.sh_link = 0;
        sh.sh_info = 0;
        sh.sh_addralign = 1;
        sh.sh_entsize = 0;
    }

    {
        Elf32_Shdr& sh = shdrs[shstrtab_idx];
        sh.sh_name = sh_shstrtab_name;
        sh.sh_type = SHT_STRTAB;
        sh.sh_flags = 0;
        sh.sh_addr = 0;
        sh.sh_offset = shstrtab_offset;
        sh.sh_size = shstrtab_bytes;
        sh.sh_link = 0;
        sh.sh_info = 0;
        sh.sh_addralign = 1;
        sh.sh_entsize = 0;
    }

    std::ofstream out(output_path, std::ios::binary | std::ios::trunc);
    if (!out)
        throw std::runtime_error("Cannot open output file: " + output_path);

    out.write((char*)(&ehdr), ehdr_size);

    for (auto& sn : sec_names) {
        const auto& data = m_section_table.at(sn).data;
        if (!data.empty()) {
            out.write((char*)(data.data()), data.size());
        }
    }

    for (auto& sn : rela_secs) {
        for (const auto& rel : m_section_table.at(sn).relocations) {
            Elf32_Rela er{};
            er.r_offset = (Elf32_Addr)(rel.offset);
            er.r_addend = (Elf32_Sword)(rel.addend);

            uint32_t sidx = 0;
            auto it = sym_idx_map.find(rel.symbol_name);
            if (it != sym_idx_map.end()) {
                sidx = it->second;
            }

            uint32_t rtype = R_X86_64_32;

            er.r_info = ELF32_R_INFO(sidx, rtype);
            out.write((char*)(&er), rela_size);
        }
    }

    out.write((char*)(symtab_data.data()), symtab_bytes);

    out.write((char*)(strtab.data.data()), strtab_bytes);

    out.write((char*)(shstrtab.data.data()), shstrtab_bytes);

    out.write((char*)(shdrs.data()), total_shdrs * shdr_size);
 
    if (!out)
        throw std::runtime_error("Write error on output file: " + output_path);
}

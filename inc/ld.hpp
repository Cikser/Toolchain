#ifndef LINKER_H
#define LINKER_H

#include "ld_structs.hpp"
#include <unordered_map>

namespace ld {

    class linker {
    public:
        linker();

        void link(const std::vector<std::string>& input_paths, const std::string& output_path,
                    output_type type, std::vector<place_t>& place_requests);
        
    private:
        std::unordered_map<std::string, symbol_t> m_sym_table;
        std::vector<section_t> m_section_table;

        void first_pass(const std::vector<std::string>& input_paths);
        void second_pass();

        bool try_local(section_t& section, relocation_t& rel);
        bool try_global(section_t& section, relocation_t& rel);

        void extract_sections_and_symbols(const std::string& input_path, 
                    std::vector<section_t>& out_sections, std::vector<symbol_t>& out_symbols);
        void merge_sections_and_symbols(std::vector<section_t>& sections, std::vector<symbol_t>& symbols);
        void map_sections();

        uint32_t find_section(const std::string& name);

        void write_elf_relocatable(const std::string& path) {};
        void write_elf_executable(const std::string& path);
        void write_dump(const std::string& path);
    };
}

#endif
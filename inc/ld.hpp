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
        uint32_t m_current_offset = 0;
        
        void extract_sections_and_symbols(const std::string& input_path, 
                    std::vector<section_t>& out_sections, std::vector<symbol_t>& out_symbols);
        void merge_sections_and_symbols(std::vector<section_t>& sections, std::vector<symbol_t>& symbols);

        uint32_t find_section(const std::string& name);

        void write_elf_relocatable(const std::string& path) {};
        void write_elf_executable(const std::string& path) {};
        void write_dump(const std::string& path) {};
    };
}

#endif
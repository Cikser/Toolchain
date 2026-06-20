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
        std::unordered_map<std::string, section_t> m_section_table;
        uint32_t m_sec_idx = 1;
        uint32_t m_current_offset = 0;
        
        std::vector<section_t> extract_sections(const std::string& input_path);
        std::vector<symbol_t> extract_symbols(const std::string& input_path);

        void write_dump();
    };
}

#endif
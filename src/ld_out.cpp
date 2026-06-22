#include "ld.hpp"

void ld::linker::link(const std::vector<std::string>& input_paths, const std::string& output_path,
                    ld::output_type type, std::vector<place_t>& place_requests) {
    for (const auto& path : input_paths) {
        std::vector<symbol_t> symbols;
        std::vector<section_t> sections;
        extract_sections_and_symbols(path, sections, symbols);
        merge_sections_and_symbols(sections, symbols);
    }
}
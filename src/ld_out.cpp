#include "ld.hpp"

void ld::linker::link(const std::vector<std::string>& input_paths, const std::string& output_path,
                    ld::output_type type, std::vector<place_t>& place_requests) {
    first_pass(input_paths);
    second_pass(); 
}
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include "ld.hpp"

int main(int argc, char* argv[]) {
    std::string output_file = "out";
    ld::output_type type = ld::output_type::NULL_TYPE;
    std::vector<ld::place_t> place_requests;
    std::vector<std::string> input_paths;
    bool out = false;
    for (uint32_t i = 1; i < argc; i++) {
        if (out) {
            if (!std::isalnum(argv[i][0])) {
                goto error;
            }
            out = false;
            output_file = argv[i];
            continue;
        }
        if (strcmp(argv[i], "-o") == 0) {
            out = true;
            continue;
        }
        if (strcmp(argv[i], "-hex") == 0) {
            if (type != ld::output_type::NULL_TYPE) {
                goto error;
            }
            type = ld::output_type::HEX;
            continue;
        }
        if (strcmp(argv[i], "-relocatable") == 0) {
            if (type != ld::output_type::NULL_TYPE) {
                goto error;
            }
            type = ld::output_type::RELOCATABLE;
            continue;
        }
        char place[7];
        memcpy(place, argv[i], 6);
        place[6] = '\0';
        if (strcmp(argv[i], "-place") == 0) {
            ld::place_t place;
            char buf[256];
            int matched = sscanf(argv[i], "-place=%255[^@]@%x", buf, &place.address);
            if (matched != 2) {
                goto error;
            }
            place.section_name = buf;
            place_requests.push_back(place);
            continue;
        }
        if (std::isalnum(argv[i][0])) {
            input_paths.push_back(argv[i]);
            continue;
        }
        goto error;
    }

    if (type == ld::output_type::NULL_TYPE) {
        goto error;
    }

    try {
        ld::linker ld;
        ld.link(input_paths, output_file, type, place_requests);
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;

error:
    std::cerr << "Usage: ./linker [-hex | -relocatable]"
              << "[-place=<section_name>@<address>]... " 
              << "[-o <output_file>] <input_files>...\n";
    return 1;
}
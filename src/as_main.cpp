#include <iostream>
#include <string>
#include "as.hpp"

int main(int argc, char* argv[]) {
    std::string option;
    std::string input_file;
    std::string output_file = "out";

    if (argc != 4 && argc != 2) goto error;

    if(argc == 4) {
        option = argv[1];
        output_file = argv[2];
        input_file = argv[3];
        if (option != "-o") goto error;
    }
    
    if (input_file.empty()) goto error;

    try {
        as::assembler as;
        as.assemble(input_file, output_file);
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;

error:
    std::cerr << "Usage: ./asembler [-o <output_file>] <input_file>\n";
    return 1;
}
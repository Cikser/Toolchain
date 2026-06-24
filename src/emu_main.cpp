#include <string>
#include <iostream>
#include "emu.hpp"

int main(int argc, char* argv[]) {
    std::string path;
    if (argc != 2) {
        std::cerr << "Usage: ./emulator <input_file>\n";
        return 1;
    }
    path = argv[1];

    try {
        emu::emulator emulator;
        emulator.emulate(path);
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
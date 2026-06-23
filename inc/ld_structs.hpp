#ifndef LD_STRUCTS_HPP
#define LD_STRUCTS_HPP

#include <string>
#include <cstdint>
#include <vector>
#include <unordered_map>

namespace ld {

    enum class output_type { NULL_TYPE, HEX, RELOCATABLE };

    struct place_t {
        std::string section_name;
        uint32_t address;
    };

    #define SECTION_UNDEF "UNDEF"
    #define SECTION_ABS "ABS"

    #define SECTION_UNDEF_IDX 0
    #define SECTION_ABS_IDX ((uint32_t)-1)

    enum class relocation_type { R_PC_REL, R_32 };

    struct relocation_t {
        std::string section_name;
        std::string symbol_name;
        int32_t addend = 0;
        uint32_t offset = 0;
        relocation_type type;
    };

    struct symbol_t {
        std::string name;
        std::string section = SECTION_UNDEF;
        uint32_t section_idx = 0;
        int32_t value = 0;
        bool absolute = false;
        bool global = false;
        bool defined = false;
        bool is_extern = false;
    };

    struct section_t {
        std::string name;
        uint32_t idx = 0;
        uint32_t address = 0;
        std::vector<uint8_t> data;
        std::vector<relocation_t> relocations;
        std::unordered_map<std::string, symbol_t> sym_table;
    };

}

#endif
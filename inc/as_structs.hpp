#ifndef AS_STRUCTS_H
#define AS_STRUCTS_H

#include <variant>
#include <vector>
#include <string>
#include <cstdint>

namespace as {

    typedef std::variant<std::string, int32_t> value_t;

    struct symbol_t {
        std::string name;
        std::string section;
        int32_t value = 0;
        bool absolute = false;
        bool global = false;
        bool defined = false;
        bool is_extern = false;
    };

    struct section_t {
        std::string name;
        std::vector<uint8_t> data;
    };


    #define SECTION_UNDEF "UNDEF"
    #define SECTION_ABS "ABS"

    enum class reloaction_type { R_PC_REL, R_32 };

    struct relocation_t {
        std::string section_name;
        std::string symbol_name;
        int32_t addend = 0;
        uint32_t offset = 0;
        reloaction_type type;
    };

    struct backpatch_t {
        std::string symbol_name;
        std::string section_name;
        uint32_t offset = 0;
        relocation_t reloc{};
    };

}

#endif
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

    struct backpatch_t {
        std::string symbol_name;
        std::string section_name;
        uint32_t offset;
    };

}

#endif
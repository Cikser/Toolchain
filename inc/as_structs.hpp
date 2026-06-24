#ifndef AS_STRUCTS_H
#define AS_STRUCTS_H

#include <variant>
#include <vector>
#include <string>
#include <cstdint>
#include <memory>

namespace as {

    typedef std::variant<std::string, int32_t> value_t;

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
        int32_t value = 0;
        bool absolute = false;
        bool global = false;
        bool defined = false;
        bool is_extern = false;
    };

    struct section_t {
        std::string name;
        uint32_t idx = 0;
        std::vector<uint8_t> data;
        std::vector<relocation_t> relocations;
        std::vector<std::pair<std::string, uint32_t>> possible_bp;
    };

    enum class backpatch_type {
        DEFAULT,
        BOUNDS,
        RELOC,
    };

    struct backpatch_t {
        std::string symbol_name;
        std::string section_name;
        uint32_t offset = 0;
        backpatch_type type = backpatch_type::DEFAULT;
    };

    enum class operand_type {
        LITERAL_IMM,
        SYMBOL_IMM,
        LITERAL_MEM,
        SYMBOL_MEM,
        REG_DIRECT,
        REG_INDIRECT,
        REG_OFFSET_LIT,
        REG_OFFSET_SYM,
    };

    struct operand_t {
        int32_t literal;
        std::string symbol;
        int32_t reg;
        operand_type type;
    };

    enum class expr_type {
        ADD,
        SUB,
        MUL,
        DIV
    };

    struct expr_node_t {
        std::shared_ptr<expr_node_t> left;
        std::shared_ptr<expr_node_t> right;
        int32_t literal;
        std::string symbol;
        expr_type type;
        bool is_binary;
        bool is_literal;
    };

    struct pending_equ_t {
        std::string symbol;
        std::shared_ptr<expr_node_t> expr;
    };

    struct eval_result_t {
        std::string section = SECTION_UNDEF;
        int32_t value = 0;
        bool absolute = false;
        bool valid = false;
    };
}

#endif
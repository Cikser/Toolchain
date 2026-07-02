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

    #define EXTERN_SECTION_PREFIX "@extern:"
    #define PREFIX_LEN 8

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

    enum class pool_entry_type { SYMBOL, LITERAL };

    struct pool_entry_t {
        std::string symbol_name;
        uint32_t literal;
        uint32_t offset;
        pool_entry_type type;
    };

    struct section_t {
        std::string name;
        uint32_t idx = 0;
        std::vector<uint8_t> data;
        std::vector<relocation_t> relocations;
        std::vector<pool_entry_t> pool_entries;
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

    enum class eval_status {
        RESOLVED,
        DEFERRED,
        INVALID
    };

    struct expr_result_t {
        eval_status status = eval_status::DEFERRED;
        std::string section = SECTION_ABS;
        int32_t value = 0;
    };

    struct node_eval_t {
        bool deferred = false;
        int32_t value = 0;
        std::unordered_map<std::string, int32_t> coeffs;
    };

    enum class equ_state {
        UNVISITED,
        VISITING,
        RESOLVED
    };

    struct pending_equ_t {
        std::string symbol;
        std::shared_ptr<expr_node_t> expr;
        equ_state state = equ_state::UNVISITED;
    };
}

#endif
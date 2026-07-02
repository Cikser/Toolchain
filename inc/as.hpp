#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <string>
#include <vector>
#include <unordered_map>
#include "as_structs.hpp"

namespace as {

    class assembler {
    public:
        assembler();

        void assemble(const std::string& input_path, const std::string& output_path);

        void dir_global(const std::vector<std::string>& symbols);
        void dir_extern(const std::vector<std::string>& symbols);
        void dir_section(const std::string& section_name);
        void dir_word(const std::vector<value_t>& values);
        void dir_skip(uint32_t count);
        void dir_ascii(const std::string& string);
        void dir_equ(const std::string& symbol_name, std::shared_ptr<expr_node_t> expr);
        void dir_end();

        void define_label(const std::string& name);

        void instr_halt();
        void instr_int();
        void instr_iret();
        void instr_ret();
        void instr_call(const operand_t& op);
        void instr_jmp(const operand_t& op);
        void instr_beq(int32_t reg1, int32_t reg2, const operand_t& op);
        void instr_bne(int32_t reg1, int32_t reg2, const operand_t& op);
        void instr_bgt(int32_t reg1, int32_t reg2, const operand_t& op);
        void instr_push(int32_t reg);
        void instr_pop(int32_t reg);
        void instr_xchg(int32_t reg_s, int32_t reg_d);
        void instr_add(int32_t reg_s, int32_t reg_d);
        void instr_sub(int32_t reg_s, int32_t reg_d);
        void instr_mul(int32_t reg_s, int32_t reg_d);
        void instr_div(int32_t reg_s, int32_t reg_d);
        void instr_not(int32_t reg);
        void instr_and(int32_t reg_s, int32_t reg_d);
        void instr_or(int32_t reg_s, int32_t reg_d);
        void instr_xor(int32_t reg_s, int32_t reg_d);
        void instr_shl(int32_t reg_s, int32_t reg_d);
        void instr_shr(int32_t reg_s, int32_t reg_d);
        void instr_ld(const operand_t& op, int32_t reg);
        void instr_st(int32_t reg, const operand_t& op);
        void instr_csrrd(int32_t csr, int32_t reg);
        void instr_csrwr(int32_t reg, int32_t csr);

    private:

        std::unordered_map<std::string, symbol_t> m_sym_table;
        std::unordered_map<std::string, section_t> m_section_table;
        std::vector<backpatch_t> m_backpatch_table;
        std::vector<pending_equ_t> m_pequ_table;
        std::string m_current_section = SECTION_UNDEF;
        uint32_t m_sec_idx = 1;

        void emit_byte(uint8_t byte);
        void emit_word(uint32_t word);
        void emit_instruction(uint32_t instr);

        uint32_t current_offset();
        section_t& current_section();

        uint32_t encode_instruction(uint8_t oc, uint8_t mod, uint8_t regA, 
                    uint8_t regB, uint8_t regC, int16_t disp);

        void emit_jump_or_call(uint8_t oc, uint8_t mod_direct, uint8_t mod_mem,
                    uint8_t regB, uint8_t regC, const operand_t& op);

        void emit_ld(const operand_t& op, int32_t reg);
        void emit_st(int32_t reg, const operand_t& op);

        bool check_bounds(int32_t literal);
        void check_pool(bool end_of_section = false);
        void insert_instr_disp(std::vector<uint8_t>& data, uint32_t index, uint16_t value);

        void resolve_bounds_backpatch();
        void resolve_reloc_backpatch();
        void resolve_symbols();
        void resolve_backpatch();
        void resolve_pequs();

        void collect_symbol_refs(const std::shared_ptr<expr_node_t>& node, std::vector<std::string>& out);
        void normalize_extern_sections();
        node_eval_t eval_node(const std::shared_ptr<expr_node_t>& node);
        expr_result_t try_eval_expression(const std::shared_ptr<expr_node_t>& expr);
        void finalize_equ_symbol(const std::string& symbol_name, const expr_result_t& result);

        void write_elf(const std::string& path);
        void write_dump(const std::string& path);
    };

}

#endif
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
            void dir_equ(const std::string& symbol_name, int32_t value);
            void dir_end();

            void define_label(const std::string& name);

        private:

            std::unordered_map<std::string, symbol_t> m_sym_table;
            std::unordered_map<std::string, section_t> m_section_table;
            std::string m_current_section = SECTION_UNDEF;

            void emit_byte(uint8_t byte);
            void emit_word(uint32_t word);

            uint32_t current_offset();
            section_t& current_section();
    };

}

#endif
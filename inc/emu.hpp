#ifndef EMULATOR_H
#define EMULATOR_H

#include <array>
#include <string>
#include <cstdint>
#include <unordered_map>

namespace emu {

    static constexpr uint32_t MEM_BLOCK_MASK = (~0UL) << 8;
    static constexpr uint64_t MEM_BLOCK_SIZE = 256;
    static constexpr uint64_t REG_NUM = 16;
    static constexpr uint64_t CONTROL_REG_NUM = 3;
    static constexpr uint32_t EXEC_START_ADDR = 0x40000000;
    using mem_block_t = std::array<uint8_t, MEM_BLOCK_SIZE>;

    static constexpr uint32_t CSR_STATUS = 0;
    static constexpr uint32_t CSR_HANDLER = 1;
    static constexpr uint32_t CSR_CAUSE = 2;

    struct instruction {
        uint8_t oc;
        uint8_t mod;
        uint8_t reg_a;
        uint8_t reg_b;
        uint8_t reg_c;
        uint16_t disp;

        instruction(uint32_t instr) {
            mod = instr & 0xF;
            oc = (instr & 0xF0) >> 4;
            reg_a = (instr & 0xF000) >> 12;
            reg_b = (instr & 0xF00) >> 8;
            reg_c = (instr & 0xF00000) >> 20;
            disp = (instr & 0xFF000000) >> 24;
            disp |= ((instr & 0xF0000) >> 16) << 8;
        }
    };

    class emulator{
    public:
        emulator();

        void emulate(const std::string& path);

    private:
        std::array<uint32_t, REG_NUM> m_reg_file;
        std::array<uint32_t, CONTROL_REG_NUM> m_control_reg_file;
        std::unordered_map<uint32_t, mem_block_t> m_memory;
        bool m_running = false;

        void load_memory(const std::string& path);
        void setup();
        void run();
        void execute_instruction();
        void handle_interrupt();

        uint8_t read_byte(uint32_t addr);
        void write_byte(uint32_t addr, uint8_t value);
        uint32_t read_word(uint32_t addr);
        void write_word(uint32_t addr, uint32_t value);
        instruction read_instruction(uint32_t addr);
    };

}

#endif
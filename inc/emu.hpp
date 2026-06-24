#ifndef EMULATOR_H
#define EMULATOR_H

#include <array>
#include <string>
#include <cstdint>
#include <unordered_map>
#include <thread>
#include <mutex>

namespace emu {

    static constexpr uint32_t MEM_BLOCK_MASK = (~0u) << 8;
    static constexpr uint64_t MEM_BLOCK_SIZE = 256;
    static constexpr uint64_t REG_NUM = 16;
    static constexpr uint64_t CONTROL_REG_NUM = 3;
    static constexpr uint32_t EXEC_START_ADDR = 0x40000000;
    static constexpr uint32_t MMIO_ADDR = 0xFFFFFF00;
    static constexpr uint32_t MMIO_TERM_IN = 0xFFFFFF04;
    static constexpr uint32_t MMIO_TERM_OUT = 0xFFFFFF00;
    static constexpr uint32_t MMIO_TIMER_CFG = 0xFFFFFF10;
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
        int16_t disp;

        instruction(uint32_t instr) {
            mod = instr & 0xF;
            oc = (instr & 0xF0) >> 4;
            reg_a = (instr & 0xF000) >> 12;
            reg_b = (instr & 0xF00) >> 8;
            reg_c = (instr & 0xF00000) >> 20;
            uint32_t disp = (instr & 0xFF000000) >> 24;
            disp |= ((instr & 0xF0000) >> 16) << 8;
            if ((disp & 0x800) != 0) {
                disp |= 0xF000;
            }
            this->disp = (int16_t)disp;
        }
    };

    class emulator{
    public:
        emulator() = default;
        ~emulator();

        void emulate(const std::string& path);

    private:
        std::array<int32_t, REG_NUM> m_reg_file;
        std::array<uint32_t, CONTROL_REG_NUM> m_control_reg_file;
        std::unordered_map<uint32_t, mem_block_t> m_memory;
        std::atomic<bool> m_running = false;

        std::thread m_timer_thread;
        std::thread m_terminal_thread;

        bool m_timer_ip = false;
        bool m_term_ip = false;
        bool m_fault_ip = false;

        uint32_t m_timer_tick = (uint32_t)-1;
        std::array<uint32_t, 8> m_timer_lookup = {500, 1000, 1500, 2000, 5000, 10000, 30000, 60000};

        uint8_t m_term_buffer = 0;

        std::mutex m_timer_mutex;
        std::mutex m_term_mutex;
        std::binary_semaphore m_timer_start_semaphore = std::binary_semaphore(0);

        void load_memory(const std::string& path);
        void setup();
        void run();
        void execute_instruction();
        void handle_interrupt();

        void timer();
        void terminal();

        uint8_t read_byte(uint32_t addr);
        void write_byte(uint32_t addr, uint8_t value);
        uint32_t read_word(uint32_t addr);
        void write_word(uint32_t addr, uint32_t value);
        instruction read_instruction(uint32_t addr);

        uint32_t read_mmio(uint32_t addr);
        void write_mmio(uint32_t addr, uint32_t value);
    };

}

#endif
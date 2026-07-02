#include "emu.hpp"
#include <elf.h>
#include <fstream>
#include <stdexcept>
#include <memory>
#include <termios.h>
#include <iostream>
#include <unistd.h>
#include <csignal>
#include <cstring>
#include <format>

termios original_term;

void term_exit() {
    tcsetattr(STDIN_FILENO, TCSANOW, &original_term);
}

void emu::emulator::emulate(const std::string& path) {
    load_memory(path);
    setup();
    run();
    term_exit();
    std::cout << '\n';
    dump_registers();
}

void emu::emulator::terminal() {
    while (!m_running) {}
    while (m_running) {
        int status = read(STDIN_FILENO, &m_term_buffer, 1);
        if (status < 0) {
            break;
        }
        m_term_mutex.lock();
        m_term_ip = true;
        m_term_mutex.unlock();
    }
}

void emu::emulator::setup() {
    m_reg_file.fill(0);
    m_reg_file[0xF] = EXEC_START_ADDR;
    m_control_reg_file.fill(0);
    tcgetattr(STDIN_FILENO, &original_term);
    termios new_term = original_term;
    new_term.c_lflag &= ~(ICANON | ECHO);
    new_term.c_cc[VMIN] = 1;
    new_term.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_term);
    m_timer_thread = std::thread(&emu::emulator::timer, this);
    m_timer_thread.detach();
    m_terminal_thread = std::thread(&emu::emulator::terminal, this);
    m_terminal_thread.detach();
}

void emu::emulator::load_memory(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        term_exit();
        throw std::runtime_error("File " + path + " does not exist");
    }
    Elf32_Ehdr elf_header;
    file.read((char*)&elf_header, sizeof(Elf32_Ehdr));
    if (!(elf_header.e_ident[EI_MAG0] == ELFMAG0 && elf_header.e_ident[EI_MAG1] == ELFMAG1
        && elf_header.e_ident[EI_MAG2] == ELFMAG2 && elf_header.e_ident[EI_MAG3] == ELFMAG3)) {
        term_exit();
        throw std::runtime_error("File " + path + " in wrong format");  
    }
    if (elf_header.e_type != ET_EXEC) {
        term_exit();
        throw std::runtime_error("File " + path + " not relocatable");
    }
    uint32_t ph_offset = elf_header.e_phoff;
    uint32_t ph_num = elf_header.e_phnum;
    uint32_t ph_ent = elf_header.e_phentsize;
    for (uint32_t i = 0; i < ph_num; i++) {
        Elf32_Phdr phdr;
        file.seekg(ph_offset + i * ph_ent);
        file.read((char*)&phdr, sizeof(Elf32_Phdr));
        uint32_t offset = phdr.p_offset;
        uint32_t va = phdr.p_vaddr;
        uint32_t size = phdr.p_memsz;
        for (uint32_t cur_size = 0; cur_size < size; ) {
            uint32_t va_cur = va + cur_size;
            uint32_t block_key = va_cur & MEM_BLOCK_MASK;
            uint32_t block_offset = va_cur & ~MEM_BLOCK_MASK;
            uint32_t space_in_block = MEM_BLOCK_SIZE - block_offset;
            uint32_t to_write = std::min(space_in_block, size - cur_size);
            auto it = m_memory.find(block_key);
            if (it == m_memory.end()) {
                auto [ins_it, _] = m_memory.insert({block_key, mem_block_t{}});
                it = ins_it;
            }
            file.seekg(offset + cur_size);
            file.read((char*)(it->second.data() + block_offset), to_write);
            cur_size += to_write;
        }
    }
}

void emu::emulator::run() {
    m_running = true;
    while (m_running) {
        execute_instruction();
    }
}

uint8_t emu::emulator::read_byte(uint32_t addr) {
    auto it = m_memory.find(addr & MEM_BLOCK_MASK);
    if (it == m_memory.end()) {
        term_exit();
        throw std::runtime_error(std::format("Page fault read at address 0x{:08x}", addr));
    }
    return it->second.at(addr & ~MEM_BLOCK_MASK);
}

uint32_t emu::emulator::read_word(uint32_t addr) {
    if ((addr & MEM_BLOCK_MASK) == MMIO_ADDR) {
        return read_mmio(addr);
    }
    uint32_t ret = 0;
    ret |= read_byte(addr + 0) << 0;
    ret |= read_byte(addr + 1) << 8;
    ret |= read_byte(addr + 2) << 16;
    ret |= read_byte(addr + 3) << 24;
    return ret;
}

void emu::emulator::write_byte(uint32_t addr, uint8_t value) {
    auto it = m_memory.find(addr & MEM_BLOCK_MASK);
    if (it == m_memory.end()) {
        mem_block_t new_block;
        new_block.fill(0);
        m_memory.insert({addr & MEM_BLOCK_MASK, new_block});
    }
    it = m_memory.find(addr & MEM_BLOCK_MASK);
    it->second.at(addr & ~MEM_BLOCK_MASK) = value;
}

void emu::emulator::write_word(uint32_t addr, uint32_t value) {
    if ((addr & MEM_BLOCK_MASK) == MMIO_ADDR) {
        write_mmio(addr, value);
        return;
    }
    write_byte(addr + 0, (value >> 0) & 0xFF);
    write_byte(addr + 1, (value >> 8) & 0xFF);
    write_byte(addr + 2, (value >> 16) & 0xFF);
    write_byte(addr + 3, (value >> 24) & 0xFF);
}

emu::instruction emu::emulator::read_instruction(uint32_t addr) {
    return instruction(read_word(addr));
}

uint32_t emu::emulator::read_mmio(uint32_t addr) {
    if (addr != MMIO_TERM_IN) {
        term_exit();
        throw std::runtime_error(std::format("Page fault read at address 0x{:08x}", addr));
    }
    return m_term_buffer;
}

void emu::emulator::write_mmio(uint32_t addr, uint32_t value) {
    if (addr != MMIO_TERM_OUT && addr != MMIO_TIMER_CFG) {
        term_exit();
        throw std::runtime_error(std::format("Page fault write at address 0x{:08x}", addr));
    }
    if (addr == MMIO_TERM_OUT) {
        write(STDOUT_FILENO, &value, 1);
    }
    else {
        if (value > 0x7) {
            term_exit();
            throw std::runtime_error("Invalid timer value");
        }
        m_timer_tick = m_timer_lookup[value];
        m_timer_start_semaphore.release();
    }
} 

void emu::emulator::execute_instruction() {
    int32_t& pc = m_reg_file.at(0xF);
    int32_t& sp = m_reg_file.at(0xE);
    instruction instr = read_instruction(pc);
    pc += 4;
    switch (instr.oc) {
        case 0x0: {
            m_running = false;
            break;
        }
        case 0x1: {
            sp -= 8;
            uint32_t& cause = m_control_reg_file.at(CSR_CAUSE);
            uint32_t& status = m_control_reg_file.at(CSR_STATUS);
            uint32_t& handler = m_control_reg_file.at(CSR_HANDLER);
            write_word(sp, status);
            write_word(sp, pc);
            cause = 4;
            status = status & (~0x1);
            pc = handler;
            break;
        }
        case 0x2: {
            sp -= 4;
            write_word(sp, pc);
            switch (instr.mod) {
                case 0x0: {
                    pc = m_reg_file.at(instr.reg_a) + m_reg_file.at(instr.reg_b) + instr.disp;
                    break;
                }
                case 0x1: {
                    pc = read_word(m_reg_file.at(instr.reg_a) + m_reg_file.at(instr.reg_b) + instr.disp);
                    break;
                }
                default: {
                    m_fault_ip = true;
                    break;
                }
            }
            break;
        }
        case 0x3: {
            switch (instr.mod) {
                case 0x0: {
                    pc = m_reg_file.at(instr.reg_a) + instr.disp;
                    break;
                }
                case 0x1: {
                    if (m_reg_file.at(instr.reg_b) == m_reg_file.at(instr.reg_c)) {
                        pc = m_reg_file.at(instr.reg_a) + instr.disp;
                    } 
                    break;
                }
                case 0x2: {
                    if (m_reg_file.at(instr.reg_b) != m_reg_file.at(instr.reg_c)) {
                        pc = m_reg_file.at(instr.reg_a) + instr.disp;
                    } 
                    break;
                }
                case 0x3: {
                    if ((int32_t)m_reg_file.at(instr.reg_b) > (int32_t)m_reg_file.at(instr.reg_c)) {
                        pc = m_reg_file.at(instr.reg_a) + instr.disp;
                    } 
                    break;
                }
                case 0x8: {
                    pc = read_word(pc = m_reg_file.at(instr.reg_a) + instr.disp);
                    break;
                }
                case 0x9: {
                    if (m_reg_file.at(instr.reg_b) == m_reg_file.at(instr.reg_c)) {
                        pc = read_word(pc = m_reg_file.at(instr.reg_a) + instr.disp);
                    } 
                    break;
                }
                case 0xA: {
                    if (m_reg_file.at(instr.reg_b) != m_reg_file.at(instr.reg_c)) {
                        pc = read_word(pc = m_reg_file.at(instr.reg_a) + instr.disp);
                    } 
                    break;
                }
                case 0xB: {
                    if ((int32_t)m_reg_file.at(instr.reg_b) > (int32_t)m_reg_file.at(instr.reg_c)) {
                        pc = read_word(pc = m_reg_file.at(instr.reg_a) + instr.disp);
                    } 
                    break;
                }
                default: {
                    m_fault_ip = true;
                    break;
                }
            }
            break;
        }
        case 0x4: {
            uint32_t temp = m_reg_file.at(instr.reg_b);
            m_reg_file.at(instr.reg_b) = m_reg_file.at(instr.reg_c);
            m_reg_file.at(instr.reg_c) = temp;
            break;
        }
        case 0x5: {
            switch (instr.mod) {
                case 0x0: {
                    m_reg_file.at(instr.reg_a) = m_reg_file.at(instr.reg_b) + m_reg_file.at(instr.reg_c);
                    break;
                }
                case 0x1: {
                    m_reg_file.at(instr.reg_a) = m_reg_file.at(instr.reg_b) - m_reg_file.at(instr.reg_c);
                    break;
                }
                case 0x2: {
                    m_reg_file.at(instr.reg_a) = m_reg_file.at(instr.reg_b) * m_reg_file.at(instr.reg_c);
                    break;
                }
                case 0x3: {
                    m_reg_file.at(instr.reg_a) = m_reg_file.at(instr.reg_b) / m_reg_file.at(instr.reg_c);
                    break;
                }
                default: {
                    m_fault_ip = true;
                    break;
                }
            }
            break;
        }
        case 0x6: {
            switch (instr.mod) {
                case 0x0: {
                    m_reg_file.at(instr.reg_a) = ~m_reg_file.at(instr.reg_b);
                    break;
                }
                case 0x1: {
                    m_reg_file.at(instr.reg_a) = m_reg_file.at(instr.reg_b) & m_reg_file.at(instr.reg_c);
                    break;
                }
                case 0x2: {
                    m_reg_file.at(instr.reg_a) = m_reg_file.at(instr.reg_b) | m_reg_file.at(instr.reg_c);
                    break;
                }
                case 0x3: {
                    m_reg_file.at(instr.reg_a) = m_reg_file.at(instr.reg_b) ^ m_reg_file.at(instr.reg_c);
                    break;
                }
                default: {
                    m_fault_ip = true;
                    break;
                }
            }
            break;
        }
        case 0x7: {
            switch (instr.mod) {
                case 0x0: {
                    m_reg_file.at(instr.reg_a) = m_reg_file.at(instr.reg_b) << m_reg_file.at(instr.reg_c);
                    break;
                }
                case 0x1: {
                    m_reg_file.at(instr.reg_a) = m_reg_file.at(instr.reg_b) >> m_reg_file.at(instr.reg_c);
                    break;
                }
                default: {
                    m_fault_ip = true;
                    break;
                }
            }
            break;
        }
        case 0x8: {
            switch (instr.mod) {
                case 0x0: {
                    write_word(m_reg_file.at(instr.reg_a) + m_reg_file.at(instr.reg_b) + instr.disp, m_reg_file.at(instr.reg_c));
                    break; 
                }
                case 0x1: {
                    m_reg_file.at(instr.reg_a) = m_reg_file.at(instr.reg_a) + instr.disp;
                    write_word(m_reg_file.at(instr.reg_a), m_reg_file.at(instr.reg_c));
                    break;
                }
                case 0x2: {
                    write_word(read_word(m_reg_file.at(instr.reg_a) + m_reg_file.at(instr.reg_b) + instr.disp), m_reg_file.at(instr.reg_c));
                    break;
                }
                default: {
                    m_fault_ip = true;
                    break;
                }
            }
            break;
        }
        case 0x9: {
            switch (instr.mod) {
                case 0x0: {
                    m_reg_file.at(instr.reg_a) = m_control_reg_file.at(instr.reg_b);
                    break;
                }
                case 0x1: {
                    m_reg_file.at(instr.reg_a) = m_reg_file.at(instr.reg_b) + instr.disp;
                    break;
                }
                case 0x2: {
                    m_reg_file.at(instr.reg_a) = read_word(m_reg_file.at(instr.reg_b) + m_reg_file.at(instr.reg_c) + instr.disp);
                    break;
                }
                case 0x3: {
                    m_reg_file.at(instr.reg_a) = read_word(m_reg_file.at(instr.reg_b));
                    m_reg_file.at(instr.reg_b) = m_reg_file.at(instr.reg_b) + instr.disp;
                    break;
                }
                case 0x4: {
                    m_control_reg_file.at(instr.reg_a) = m_reg_file.at(instr.reg_b);
                    break;
                }
                case 0x5: {
                    m_control_reg_file.at(instr.reg_a) = m_control_reg_file.at(instr.reg_b) | instr.disp;
                    break;
                }
                case 0x6: {
                    m_control_reg_file.at(instr.reg_a) = read_word(m_reg_file.at(instr.reg_b) + m_reg_file.at(instr.reg_c) + instr.disp);
                    break;
                }
                case 0x7: {
                    m_control_reg_file.at(instr.reg_a) = read_word(m_reg_file.at(instr.reg_b));
                    m_reg_file.at(instr.reg_b) = m_reg_file.at(instr.reg_b) + instr.disp;
                    break;
                }
                default: {
                    m_fault_ip = true;
                    break;
                }
            }
            break;
        }
        default: {
            m_fault_ip = true;
            break;
        }
    }
    handle_interrupt();
}

void emu::emulator::handle_interrupt() {
    if (!m_fault_ip && !m_timer_ip && !m_term_ip) {
        return;
    }
    uint32_t& cause = m_control_reg_file.at(CSR_CAUSE);
    uint32_t& status = m_control_reg_file.at(CSR_STATUS);
    uint32_t& handler = m_control_reg_file.at(CSR_HANDLER);
    int32_t& pc = m_reg_file.at(0xF);
    int32_t& sp = m_reg_file.at(0xE);
    m_timer_mutex.lock();
    m_term_mutex.lock();
    bool taken = false;
    if (m_fault_ip) {
        m_fault_ip = false;
        taken = true;
        cause = 1;
    }
    else if (m_term_ip && !(status & 0x2) && !(status & 0x4)) {
        m_term_ip = false;
        taken = true;
        cause = 3;
    }
    else if (m_timer_ip & !(status & 0x1) && !(status & 0x4)) {
        m_timer_ip = false;
        taken = true;
        cause = 2;
    }
    if (!taken) {
        m_term_mutex.unlock();
        m_timer_mutex.unlock();
        return;
    }
    m_term_mutex.unlock();
    m_timer_mutex.unlock();
    sp -= 4;
    write_word(sp, status);
    sp -= 4;
    write_word(sp, pc);
    status |= 0x4;
    pc = handler;
}

void emu::emulator::timer() {
    m_timer_start_semaphore.acquire();
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(m_timer_tick));
        m_timer_mutex.lock();
        m_timer_ip = true;
        m_timer_mutex.unlock();
    }
}

void emu::emulator::dump_registers() {
    std::cout << "Emulated processor executed halt instruction\n";
    std::cout << "Emulated processor state:\n";
    for (size_t i = 0; i < m_reg_file.size(); ++i) {
        std::cout << std::format("r{}={:#010x}", i, (uint32_t)(m_reg_file[i]));
        if ((i + 1) % 4 == 0) {
            std::cout << '\n';
        } 
        else {
            std::cout << ' ';
        }
    }
}
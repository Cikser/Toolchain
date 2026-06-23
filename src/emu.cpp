#include "emu.hpp"
#include <elf.h>
#include <fstream>
#include <stdexcept>
#include <memory>

void emu::emulator::emulate(const std::string& path) {
    load_memory(path);
    setup();
    run();
}

void emu::emulator::setup() {
    m_reg_file.fill(0);
    m_reg_file[0xF] = EXEC_START_ADDR;
    m_control_reg_file.fill(0);
}

void emu::emulator::load_memory(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("File " + path + " does not exist");
    }
    Elf32_Ehdr elf_header;
    file.read((char*)&elf_header, sizeof(Elf32_Ehdr));
    if (!(elf_header.e_ident[EI_MAG0] == ELFMAG0 && elf_header.e_ident[EI_MAG1] == ELFMAG1
        && elf_header.e_ident[EI_MAG2] == ELFMAG2 && elf_header.e_ident[EI_MAG3] == ELFMAG3)) {
        throw std::runtime_error("File " + path + " in wrong format");  
    }
    // todo format checks
    if (elf_header.e_type != ET_EXEC) {
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
        for (uint32_t cur_size = 0; cur_size < size; cur_size += MEM_BLOCK_SIZE) {
            mem_block_t mem_block;
            file.seekg(offset + cur_size);
            file.read((char*)mem_block.data(), std::max<uint32_t>(size, cur_size));
            m_memory.insert({va & MEM_BLOCK_MASK, mem_block});
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
        throw std::runtime_error("Page fault");
    }
    return it->second.at(addr & ~MEM_BLOCK_MASK);
}

uint32_t emu::emulator::read_word(uint32_t addr) {
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
        throw std::runtime_error("Page fault");
    }
    it->second.at(addr & ~MEM_BLOCK_MASK) = value;
}

void emu::emulator::write_word(uint32_t addr, uint32_t value) {
    write_byte(addr + 0, (value >> 0) & 0xFF);
    write_byte(addr + 1, (value >> 8) & 0xFF);
    write_byte(addr + 2, (value >> 16) & 0xFF);
    write_byte(addr + 3, (value >> 24) & 0xFF);
}

emu::instruction emu::emulator::read_instruction(uint32_t addr) {
    return instruction(read_word(addr));
}

void emu::emulator::execute_instruction() {
    uint32_t& pc = m_reg_file.at(0xF);
    uint32_t& sp = m_reg_file.at(0xE);
    instruction instr = read_instruction(pc);
    pc += 4;
    switch (instr.oc) {
        case 0x0: {
            m_running = false;
            break;
        }
        case 0x1: {
            sp -= 4;
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
                    throw std::runtime_error("Unknown mod");
                }
            }
            break;
        }
        case 0x3: {
            switch (instr.mod) {
                case 0x0: {
                    pc = m_reg_file.at(instr.reg_a + instr.disp);
                    break;
                }
                case 0x1: {
                    if (m_reg_file.at(instr.reg_b) == m_reg_file.at(instr.reg_c)) {
                        pc = m_reg_file.at(instr.reg_a + instr.disp);
                    } 
                    break;
                }
                case 0x2: {
                    if (m_reg_file.at(instr.reg_b) != m_reg_file.at(instr.reg_c)) {
                        pc = m_reg_file.at(instr.reg_a + instr.disp);
                    } 
                    break;
                }
                case 0x3: {
                    if ((int32_t)m_reg_file.at(instr.reg_b) > (int32_t)m_reg_file.at(instr.reg_c)) {
                        pc = m_reg_file.at(instr.reg_a + instr.disp);
                    } 
                    break;
                }
                case 0x4: {
                    pc = read_word(m_reg_file.at(instr.reg_a + instr.disp));
                    break;
                }
                case 0x5: {
                    if (m_reg_file.at(instr.reg_b) == m_reg_file.at(instr.reg_c)) {
                        pc = read_word(m_reg_file.at(instr.reg_a + instr.disp));
                    } 
                    break;
                }
                case 0x6: {
                    if (m_reg_file.at(instr.reg_b) != m_reg_file.at(instr.reg_c)) {
                        pc = read_word(m_reg_file.at(instr.reg_a + instr.disp));
                    } 
                    break;
                }
                case 0x7: {
                    if ((int32_t)m_reg_file.at(instr.reg_b) > (int32_t)m_reg_file.at(instr.reg_c)) {
                        pc = read_word(m_reg_file.at(instr.reg_a + instr.disp));
                    } 
                    break;
                }
                default: {
                    throw std::runtime_error("Unknown mod");
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
                    throw std::runtime_error("Unknown mod");
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
                    throw std::runtime_error("Unknown mod");
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
                    throw std::runtime_error("Unknown mod");
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
                    write_word(read_word(m_reg_file.at(instr.reg_a) + m_reg_file.at(instr.reg_b) + instr.disp), m_reg_file.at(instr.reg_c));
                    break;
                }
                case 0x2: {
                    m_reg_file.at(instr.reg_a) = m_reg_file.at(instr.reg_a) + instr.disp;
                    write_word(m_reg_file.at(instr.reg_a), m_reg_file.at(instr.reg_c));
                    break;
                }
                default: {
                    throw std::runtime_error("Unknown mod");
                }
            }
            break;
        }
        case 0x9: {
            switch (instr.mod) {
                case 0x0: {
                    m_reg_file.at(instr.reg_a) = m_reg_file.at(instr.reg_b);
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
                    throw std::runtime_error("Unknown mod");
                }
            }
            break;
        }
        default: {
            throw std::runtime_error("Unknown oc");
        }
    }
    handle_interrupt();
}
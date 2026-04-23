
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <queue>
#include <bitset>
#include <algorithm>
#include <climits>

// RISC-V 5-stage pipeline simulator
class RISCVSimulator {
private:
private:
    // Register file (32 registers)
    std::vector<int> registers;
    
public:
    // Memory (simplified)
    std::vector<int> memory;
    
private:
    // Program counter
    int pc;
    
    // Pipeline registers
    struct IF_ID {
        int instruction;
        int pc;
        bool valid;
    } if_id;
    
    struct ID_EX {
        int rs1, rs2, rd;
        int imm;
        int pc;
        int opcode;
        int funct3;
        int funct7;
        bool valid;
        bool alu_src;  // Added missing field
    } id_ex;
    
    struct EX_MEM {
        int alu_result;
        int rs2_value;
        int rd;
        int pc;
        int opcode;
        bool valid;
    } ex_mem;
    
    struct MEM_WB {
        int result;
        int rd;
        bool valid;
    } mem_wb;
    
    // Control signals
    struct ControlSignals {
        bool reg_write;
        bool mem_read;
        bool mem_write;
        bool mem_to_reg;
        bool alu_src;
        int alu_op;
        bool branch;
    } control;
    
    // Instruction types
    enum InstructionType {
        R_TYPE, I_TYPE, S_TYPE, B_TYPE, U_TYPE, J_TYPE
    };
    
    // Opcode mappings
    std::map<int, std::string> opcode_map;
    
    // Cycle counter
    int cycles;
    
    // Forwarding detection
    bool forward_a, forward_b;
    
public:
    RISCVSimulator() : registers(32, 0), memory(1024, 0), pc(0), cycles(0) {
        // Initialize pipeline registers
        if_id.valid = false;
        id_ex.valid = false;
        ex_mem.valid = false;
        mem_wb.valid = false;
        
        // Initialize opcode map
        opcode_map[51] = "R_TYPE";   // ADD, SUB, AND, OR, etc.
        opcode_map[19] = "I_TYPE";   // ADDI, ANDI, ORI, etc.
        opcode_map[3] = "I_TYPE";    // LW
        opcode_map[35] = "S_TYPE";   // SW
        opcode_map[99] = "B_TYPE";   // BEQ, BNE, etc.
        opcode_map[111] = "J_TYPE";  // JAL
        opcode_map[23] = "U_TYPE";   // LUI
    }
    
    // Decode instruction
    void decode(int instruction) {
        int opcode = instruction & 0x7F;
        int rd = (instruction >> 7) & 0x1F;
        int funct3 = (instruction >> 12) & 0x7;
        int rs1 = (instruction >> 15) & 0x1F;
        int rs2 = (instruction >> 20) & 0x1F;
        int funct7 = (instruction >> 25) & 0x7F;
        
        // Set control signals based on opcode
        control.reg_write = false;
        control.mem_read = false;
        control.mem_write = false;
        control.mem_to_reg = false;
        control.alu_src = false;
        control.alu_op = 0;
        control.branch = false;
        
        if (opcode == 51) {  // R-type
            control.reg_write = true;
            // ALU operation determined by funct3 and funct7
        } else if (opcode == 19 || opcode == 3) {  // I-type
            control.reg_write = true;
            control.alu_src = true;
            if (opcode == 3) {  // LW
                control.mem_read = true;
                control.mem_to_reg = true;
            }
        } else if (opcode == 35) {  // S-type
            control.mem_write = true;
            control.alu_src = true;
        } else if (opcode == 99) {  // B-type
            control.branch = true;
        } else if (opcode == 111) {  // J-type
            control.reg_write = true;
            control.mem_to_reg = false;
        } else if (opcode == 23) {  // U-type
            control.reg_write = true;
            control.alu_src = true;
        }
        
        // Store in ID/EX pipeline register
        id_ex.rs1 = rs1;
        id_ex.rs2 = rs2;
        id_ex.rd = rd;
        id_ex.pc = pc;
        id_ex.opcode = opcode;
        id_ex.funct3 = funct3;
        id_ex.funct7 = funct7;
        id_ex.valid = true;
        
        // Extract immediate based on instruction type
        if (opcode_map.find(opcode) != opcode_map.end()) {
            std::string type = opcode_map[opcode];
            if (type == "I_TYPE") {
                id_ex.imm = sign_extend((instruction >> 20) & 0xFFF, 12);
            } else if (type == "S_TYPE") {
                id_ex.imm = sign_extend(((instruction >> 25) & 0x7F) | ((instruction >> 7) & 0x1F) << 7, 12);
            } else if (type == "B_TYPE") {
                id_ex.imm = sign_extend((((instruction >> 31) & 0x1) << 11) | 
                                       (((instruction >> 7) & 0x1) << 10) | 
                                       (((instruction >> 25) & 0x3F) << 4) | 
                                       (((instruction >> 8) & 0xF) << 1), 12) << 1;
            } else if (type == "U_TYPE") {
                id_ex.imm = sign_extend((instruction >> 12) & 0xFFFFF, 20) << 12;
            } else if (type == "J_TYPE") {
                id_ex.imm = sign_extend((((instruction >> 21) & 0x3FF) << 1) | 
                                       (((instruction >> 20) & 0x1) << 11) | 
                                       (((instruction >> 12) & 0xFF) << 12) | 
                                       (((instruction >> 31) & 0x1) << 20), 20) << 1;
            }
        }
    }
    
    // Sign extend a value
    int sign_extend(int value, int bits) {
        int mask = 1 << (bits - 1);
        return (value ^ mask) - mask;
    }
    
    // Execute stage
    void execute() {
        if (!id_ex.valid) {
            ex_mem.valid = false;
            return;
        }
        
        int alu_input1 = registers[id_ex.rs1];
        int alu_input2 = id_ex.alu_src ? id_ex.imm : registers[id_ex.rs2];
        
        // Forwarding logic
        if (forward_a) {
            alu_input1 = ex_mem.alu_result;
        }
        if (forward_b) {
            alu_input2 = ex_mem.alu_result;
        }
        
        int alu_result = 0;
        
        // Perform ALU operation based on opcode and funct fields
        if (id_ex.opcode == 51) {  // R-type
            if (id_ex.funct3 == 0) {
                if (id_ex.funct7 == 0) {
                    alu_result = alu_input1 + alu_input2;  // ADD
                } else if (id_ex.funct7 == 32) {
                    alu_result = alu_input1 - alu_input2;  // SUB
                }
            } else if (id_ex.funct3 == 4) {
                alu_result = alu_input1 ^ alu_input2;  // XOR
            } else if (id_ex.funct3 == 6) {
                alu_result = alu_input1 | alu_input2;  // OR
            } else if (id_ex.funct3 == 7) {
                alu_result = alu_input1 & alu_input2;  // AND
            } else if (id_ex.funct3 == 1) {
                alu_result = alu_input1 << (alu_input2 & 0x1F);  // SLL
            } else if (id_ex.funct3 == 5) {
                if (id_ex.funct7 == 0) {
                    alu_result = alu_input1 >> (alu_input2 & 0x1F);  // SRL
                } else if (id_ex.funct7 == 32) {
                    alu_result = (int)((unsigned int)alu_input1 >> (alu_input2 & 0x1F));  // SRA
                }
            }
        } else {
            alu_result = alu_input1 + alu_input2;  // Default for I, S, B, U, J types
        }
        
        // Store in EX/MEM pipeline register
        ex_mem.alu_result = alu_result;
        ex_mem.rs2_value = registers[id_ex.rs2];  // Needed for SW
        ex_mem.rd = id_ex.rd;
        ex_mem.pc = id_ex.pc;
        ex_mem.opcode = id_ex.opcode;
        ex_mem.valid = true;
    }
    
    // Memory stage
    void memory_access() {
        if (!ex_mem.valid) {
            mem_wb.valid = false;
            return;
        }
        
        int result = ex_mem.alu_result;
        
        // Memory operations
        if (control.mem_read) {
            // Load from memory
            result = memory[ex_mem.alu_result];
        } else if (control.mem_write) {
            // Store to memory
            memory[ex_mem.alu_result] = ex_mem.rs2_value;
        }
        
        // Store in MEM/WB pipeline register
        mem_wb.result = result;
        mem_wb.rd = ex_mem.rd;
        mem_wb.valid = true;
    }
    
    // Write back stage
    void write_back() {
        if (mem_wb.valid && control.reg_write) {
            if (mem_wb.rd != 0) {  // Don't write to x0 (zero register)
                registers[mem_wb.rd] = mem_wb.result;
            }
        }
    }
    
    // Fetch next instruction
    void fetch() {
        // In a real simulator, we would read from instruction memory
        // For now, we'll just increment PC
        pc += 4;
    }
    
    // Main simulation loop
    void run() {
        while (true) {
            // Check for termination condition
            if (pc >= (int)memory.size() * 4) {
                break;
            }
            
            // Pipeline stages (in reverse order to allow forwarding)
            write_back();
            memory_access();
            execute();
            decode(if_id.instruction);
            fetch();
            
            // Update pipeline registers
            if_id.instruction = memory[pc / 4];
            if_id.pc = pc;
            if_id.valid = true;
            
            cycles++;
            
            // Break after a reasonable number of cycles to avoid infinite loops
            if (cycles > 1000000) {
                break;
            }
        }
    }
    
    // Print register values
    void print_registers() {
        for (int i = 0; i < 32; i++) {
            std::cout << "x" << i << " = " << registers[i] << std::endl;
        }
    }
    
    // Print final statistics
    void print_stats() {
        std::cout << "Total cycles: " << cycles << std::endl;
    }
};

int main() {
    RISCVSimulator simulator;
    
    // Read input data (instructions and initial memory state)
    // The input format is not specified, so we'll assume a simple format
    // where each line is a 32-bit instruction in hexadecimal
    
    std::string line;
    int addr = 0;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        
        // Convert hex string to integer
        int instruction = std::stoi(line, nullptr, 16);
        simulator.memory[addr++] = instruction;
    }
    
    // Run the simulator
    simulator.run();
    
    // Output the final register values
    // According to the README, the output should be in the form of comments in a .c file
    // But since we need to output to stdout, we'll print the register values
    simulator.print_registers();
    simulator.print_stats();
    
    return 0;
}


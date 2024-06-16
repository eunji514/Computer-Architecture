#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define MEMORY_SIZE 0x4000000 // 64MB memory
uint8_t instr_memory[MEMORY_SIZE]; // Instruction memory
uint8_t data_memory[MEMORY_SIZE];  // Data memory
uint32_t reg[32]; // 32bit registers
uint32_t pc = 0; // program counter
int clock_cycle = 0;
int instruction_count = 0, memory_access_count = 0, register_ops_count = 0, branch_count = 0, jump_count = 0;
int predict_correct = 0, mis_predict = 0, total_predict = 0;

// Pipeline registers
typedef struct {
    uint32_t instruction;
    uint32_t pc;
} IF_ID;

typedef struct {
    uint32_t instruction;
    uint32_t pc;
    uint32_t rs;
    uint32_t rt;
    uint32_t rd;
    int16_t immediate;
    uint32_t address;
    uint32_t reg_rs_value;
    uint32_t reg_rt_value;
} ID_EX;

typedef struct {
    uint32_t instruction;
    uint32_t pc;
    uint32_t alu_result;
    uint32_t rt;
    uint32_t rd;
    uint32_t reg_rt_value;
} EX_MEM;

typedef struct {
    uint32_t instruction;
    uint32_t pc;
    uint32_t mem_data;
    uint32_t alu_result;
    uint32_t rd;
} MEM_WB;

IF_ID if_id = {0};
ID_EX id_ex = {0};
EX_MEM ex_mem = {0};
MEM_WB mem_wb = {0};

// int needStall = 0;

// Function declarations
void fetch();
void decode();
void execute();
void mem_access();
void write_back();
void load_binary(const char* filename, uint8_t* memory);
void forward();
void mem_write(uint32_t address, uint32_t value);
void write_back_reg(uint32_t rd, uint32_t value);
// void detect_and_insert_stall();
// void stall_pipeline();

int main() {
    // Initialize registers
    for (int i = 0; i < 29; ++i) {
        reg[i] = 0;
    }
    reg[29] = 0x1000000; // Initialize SP
    reg[31] = 0xFFFFFFF; // Initialize LR

    memset(instr_memory, 0, MEMORY_SIZE); // Initialize instruction memory
    memset(data_memory, 0, MEMORY_SIZE);  // Initialize data memory

    load_binary("simple3.bin", instr_memory); // Load binary file into instruction memory

    while (pc < MEMORY_SIZE && pc != 0xFFFFFFFF) {
        printf("Cycle %d: PC = 0x%08X\n", clock_cycle, if_id.pc);
        write_back();
        mem_access();
        execute();
        forward();
        decode();
        // detect_and_insert_stall(); // Detect and insert stalling
        fetch();
        clock_cycle++;

        
    }

    // Output
    printf("*******************************************************\n");
    printf("Cycle: %d\n", clock_cycle);
    printf("R[2]: %d\n", reg[2]);
    printf("Number of instructions: %d\n", instruction_count);
    printf("Number of memory access instructions: %d\n", memory_access_count);
    printf("Number of Register ops: %d\n", register_ops_count);
    printf("Number of branch instruction: %d\n", branch_count);
    printf("Number of jump instruction: %d\n", jump_count);
    printf("Predict correct: %d, mis predict: %d, total predict: %d\n", predict_correct, mis_predict, total_predict);
    printf("*******************************************************\n");

    return 0;
}

void fetch() {
    if (pc + 4 <= MEMORY_SIZE) {
        if_id.instruction = (instr_memory[pc] << 24) | (instr_memory[pc + 1] << 16) | (instr_memory[pc + 2] << 8) | instr_memory[pc + 3];
        if_id.pc = pc;
        pc += 4;
        instruction_count++;
        printf("Fetch: PC = 0x%08X, Instruction = 0x%08X\n\n", if_id.pc, if_id.instruction);
    }
}

void decode() {
    uint32_t instruction = if_id.instruction;
    id_ex.instruction = instruction;
    id_ex.pc = if_id.pc;
    id_ex.rs = (instruction >> 21) & 0x1F;
    id_ex.rt = (instruction >> 16) & 0x1F;
    id_ex.rd = (instruction >> 11) & 0x1F;
    id_ex.immediate = instruction & 0xFFFF;
    id_ex.address = instruction & 0x3FFFFFF;
    id_ex.reg_rs_value = reg[id_ex.rs];
    id_ex.reg_rt_value = reg[id_ex.rt];
    printf("Decode: PC = 0x%08X, Instruction = 0x%08X\n", id_ex.pc, id_ex.instruction);
}

void execute() {
    uint32_t opcode = id_ex.instruction >> 26;
    uint32_t rs = id_ex.rs;
    uint32_t rt = id_ex.rt;
    uint32_t rd = id_ex.rd;
    int16_t immediate = id_ex.immediate;
    uint32_t address = id_ex.address;
    int32_t sign_extended_immediate = (int16_t)immediate;
    uint32_t value;

    ex_mem.instruction = id_ex.instruction;
    ex_mem.pc = id_ex.pc;
    ex_mem.rt = rt;
    ex_mem.rd = rd;
    ex_mem.reg_rt_value = id_ex.reg_rt_value;

    printf("Executing instruction: 0x%08X\n", id_ex.instruction);
    printf("rs: R[%d] = 0x%08X, rt: R[%d] = 0x%08X\n", rs, id_ex.reg_rs_value, rt, id_ex.reg_rt_value);
    printf("opcode: %X\n", opcode);


    switch (opcode) {
        case 0x00: // R-type instructions
            if (id_ex.instruction != 0x00000000) { // Ensure not to count NOP as a register operation
                register_ops_count++;
            }
            switch (id_ex.instruction & 0x3F) {
                case 0x20: // add
                    value = (int32_t)id_ex.reg_rs_value + (int32_t)id_ex.reg_rt_value;
                    ex_mem.alu_result = value;
                    break;
                case 0x21: // addu
                    value = id_ex.reg_rs_value + id_ex.reg_rt_value;
                    ex_mem.alu_result = value;
                    break;
                case 0x22: // sub
                    value = (int32_t)id_ex.reg_rs_value - (int32_t)id_ex.reg_rt_value;
                    ex_mem.alu_result = value;
                    break;
                case 0x24: // and
                    value = id_ex.reg_rs_value & id_ex.reg_rt_value;
                    ex_mem.alu_result = value;
                    break;
                case 0x25: // or
                    value = id_ex.reg_rs_value | id_ex.reg_rt_value;
                    ex_mem.alu_result = value;
                    break;
                case 0x2A: // slt
                    value = (int32_t)id_ex.reg_rs_value < (int32_t)id_ex.reg_rt_value ? 1 : 0;
                    ex_mem.alu_result = value;
                    break;
                case 0x00: // sll
                    value = id_ex.reg_rt_value << id_ex.immediate;
                    ex_mem.alu_result = value;
                    break;
                case 0x02: // srl
                    value = id_ex.reg_rt_value >> id_ex.immediate;
                    ex_mem.alu_result = value;
                    break;
                case 0x08: // jr
                    jump_count++;
                    pc = id_ex.reg_rs_value;
                    printf("Execute: JR to PC = 0x%08X\n", pc);
                    break;
                default:
                    printf("Unsupported R-type funct: %X\n", id_ex.instruction & 0x3F);
            }
            break;
        case 0x02: // J
            jump_count++;
            pc = (pc & 0xF0000000) | (address << 2);
            printf("Execute: J to PC = 0x%08X\n", pc);
            break;
        case 0x03: // JAL
            jump_count++;
            reg[31] = pc;
            pc = (pc & 0xF0000000) | (address << 2);
            printf("Execute: JAL to PC = 0x%08X\n", pc);
            break;
        case 0x04: // BEQ
            branch_count++;
            total_predict++;
            if (id_ex.reg_rs_value == id_ex.reg_rt_value) {
                pc += (sign_extended_immediate << 2);
                predict_correct++;
                printf("Execute: BEQ taken to PC = 0x%08X\n", pc);
            } else {
                mis_predict++;
                printf("Execute: BEQ not taken\n");
            }
            break;
        case 0x05: // BNE
            branch_count++;
            total_predict++;
            printf("BNE Execution: id_ex.reg_rs_value = %d, id_ex.reg_rt_value = %d\n", id_ex.reg_rs_value, id_ex.reg_rt_value);
            if (id_ex.reg_rs_value != id_ex.reg_rt_value) {
                pc += sign_extended_immediate << 2;
                predict_correct++;
                printf("Execute: BNE taken to PC = 0x%08X\n", pc);
            } else {
                mis_predict++;
                printf("Execute: BNE not taken\n");
            }
            break;

        case 0x08: // ADDI
            register_ops_count++;
            value = id_ex.reg_rs_value + immediate;
            ex_mem.alu_result = value;
            ex_mem.rd = rt; 
            break;
        case 0x09: // ADDIU
            register_ops_count++;
            value = id_ex.reg_rs_value + sign_extended_immediate;
            ex_mem.alu_result = value;
            ex_mem.rd = rt; 
            break;
        case 0x0A: // SLTI
            register_ops_count++;
            value = (int32_t)id_ex.reg_rs_value < sign_extended_immediate ? 1 : 0;
            ex_mem.alu_result = value;
            ex_mem.rd = rt; 
            printf("SLTI -> rs: %d, rt: %d, value: %d\n", id_ex.reg_rs_value, ex_mem.rd, value);
            break;
        case 0x0C: // ANDI
            register_ops_count++;
            value = id_ex.reg_rs_value & (id_ex.immediate & 0xFFFF);
            ex_mem.alu_result = value;
            ex_mem.rd = rt; 
            break;
        case 0x0D: // ORI
            register_ops_count++;
            value = id_ex.reg_rs_value | (id_ex.immediate & 0xFFFF);
            ex_mem.alu_result = value;
            ex_mem.rd = rt; 
            break;
        case 0x0E: // XORI
            register_ops_count++;
            value = id_ex.reg_rs_value ^ (id_ex.immediate & 0xFFFF);
            ex_mem.alu_result = value;
            ex_mem.rd = rt; 
            break;
        case 0x0F: // LUI
            register_ops_count++;
            value = id_ex.immediate << 16;
            ex_mem.alu_result = value;
            ex_mem.rd = rt; 
            break;
        case 0x23: // LW
            memory_access_count++;
            ex_mem.alu_result = id_ex.reg_rs_value + sign_extended_immediate;
            ex_mem.rd = rs; 
            break;
        case 0x2B: // SW
            memory_access_count++;
            ex_mem.alu_result = id_ex.reg_rs_value + sign_extended_immediate;
            ex_mem.rd = rt; 
            break;
        default:
            printf("Unsupported opcode: %X\n", opcode);
    }
}

void mem_access() {
    uint32_t instruction = ex_mem.instruction;
    uint32_t opcode = instruction >> 26;
    uint32_t mem_address;
    uint32_t value;

    mem_wb.instruction = ex_mem.instruction;
    mem_wb.pc = ex_mem.pc;
    mem_wb.alu_result = ex_mem.alu_result;
    mem_wb.rd = ex_mem.rd;

    switch (opcode) {
        case 0x23: // LW
            mem_address = ex_mem.alu_result;
            if (mem_address % 4 == 0 && mem_address < MEMORY_SIZE - 3) { // Ensure we do not read out of bounds
                value = (data_memory[mem_address] << 24) | (data_memory[mem_address + 1] << 16) |
                        (data_memory[mem_address + 2] << 8) | data_memory[mem_address + 3];
                mem_wb.mem_data = value;
                printf("Memory Access: LW from address 0x%08X, Data = 0x%08X\n", mem_address, value);
            } else {
                printf("Memory access error: Invalid address %08X\n", mem_address);
            }
            break;
        case 0x2B: // SW
            mem_write(ex_mem.alu_result, ex_mem.reg_rt_value);
            printf("Memory Access: SW to address 0x%08X, Data = 0x%08X\n", ex_mem.alu_result, ex_mem.reg_rt_value);
            break;
    }
}

void write_back() {
    uint32_t instruction = mem_wb.instruction;
    uint32_t opcode = instruction >> 26;
    uint32_t rd = mem_wb.rd;

    switch (opcode) {
        case 0x00: // R-type
            write_back_reg(rd, mem_wb.alu_result);
            break;
        case 0x08: // ADDI
        case 0x09: // ADDIU
        case 0x0A: // SLTI
        case 0x0C: // ANDI
        case 0x0D: // ORI
        case 0x0E: // XORI
        case 0x0F: // LUI
            write_back_reg(rd, mem_wb.alu_result);
            break;
        case 0x23: // LW
            write_back_reg(rd, mem_wb.mem_data);
            break;
    }
    printf("Write Back: Instruction = 0x%08X, Register[%d] = 0x%08X\n", instruction, rd, reg[rd]);
}

void mem_write(uint32_t address, uint32_t value) {
    if (address < MEMORY_SIZE - 3) { // Ensure we do not write out of bounds
        if (address % 4 == 0) { // Check if the address is a multiple of 4 (word-aligned)
            data_memory[address] = (value >> 24) & 0xFF;
            data_memory[address + 1] = (value >> 16) & 0xFF;
            data_memory[address + 2] = (value >> 8) & 0xFF;
            data_memory[address + 3] = value & 0xFF;
        } else {
            printf("Memory write error: Address is not word-aligned\n");
        }
    } else {
        printf("Memory write error: Address out of bounds %08X\n", address);
    }
}

void write_back_reg(uint32_t rd, uint32_t value) {
    if (rd != 0) { // Register 0 is always 0 in MIPS
        reg[rd] = value;  // Write the value to the specified register
    }
}

void load_binary(const char* filename, uint8_t* memory) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Error opening file");
        exit(1);
    }
    fread(memory, sizeof(uint8_t), MEMORY_SIZE, file);
    fclose(file);
}

void forward() {
    // Forwarding from MEM stage to EX stage
    if (ex_mem.rd != 0) {
        if (ex_mem.rd == id_ex.rs) {
            id_ex.reg_rs_value = ex_mem.alu_result;
        }
        if (ex_mem.rd == id_ex.rt) {
            id_ex.reg_rt_value = ex_mem.alu_result;
        }
    }

    // Forwarding from WB stage to EX stage
    if (mem_wb.rd != 0) {
        if (mem_wb.rd == id_ex.rs) {
            id_ex.reg_rs_value = mem_wb.alu_result;
        }
        if (mem_wb.rd == id_ex.rt) {
            id_ex.reg_rt_value = mem_wb.alu_result;
        }
    }
}

// void detect_and_insert_stall() {
//     // Check for data dependency in ID/EX stage
//     if ((id_ex.instruction >> 26) == 0x23) { // LW instruction
//         uint32_t lw_rt = id_ex.rt;
//         if (lw_rt != 0 && ((lw_rt == ((if_id.instruction >> 21) & 0x1F)) || (lw_rt == ((if_id.instruction >> 16) & 0x1F)))) {
//             // If data dependency occurs
//             needStall = 1;
//         }
//     }

//     if (needStall) {
//         stall_pipeline();
//         needStall = 0;
//     }
// }

// void stall_pipeline() {
//     // Stall ID/EX and EX/MEM stages in the current cycle
//     ex_mem = (EX_MEM){0};
//     id_ex = (ID_EX){0};
    
//     // Restore PC and IF/ID pipeline register to previous state to re-execute the fetched instruction
//     pc -= 4;
//     if_id = prev_if_id;
    
//     // Do not increment instruction count
//     instruction_count--;
// }

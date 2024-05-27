#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>


#define MEMORY_SIZE 0x4000000 // 64MB memory
uint8_t memory[MEMORY_SIZE];
uint32_t reg[32]; // 32bit registers
uint32_t pc = 0; // program counter
uint32_t instruction; // current instruction
int instruction_count = 0, r_type_count = 0, i_type_count = 0, j_type_count = 0, memory_access_count = 0, branch_taken_count = 0;

// Function declarations
uint32_t fetch();
void decode(uint32_t instruction);
void execute(uint32_t instruction);
void loadBinary(const char* filename);


int main() {

    // Initialize registers
    for (int i = 0; i < 29; ++i) {
        reg[i] = 0;
    }
    reg[29] = 0x1000000; // Initialize SP
    reg[31] = 0xFFFFFFF; // Initialize LR

    memset(memory, 0, MEMORY_SIZE); // Initialize memory

    loadBinary("simple3.bin"); // Change file

    while (pc < MEMORY_SIZE && pc != 0xFFFFFFFF) {
        uint32_t instruction = fetch();
        printf("Cycle: %d, PC: %0X, Instruction: %08X\n", instruction_count+1, pc, instruction);
        pc += 4;
        decode(instruction);
        // printf("Value in reg[2] after cycle %d: %d\n", instruction_count+1, reg[2]); - r2 반환 확인용
        instruction_count++;
    }

    printf("\n*********** Result ************\n");
    printf("Final value in r2: %d\n", reg[2]);
    printf("Total executed instructions: %d\n", instruction_count);
    printf("R-type instructions: %d\n", r_type_count);
    printf("I-type instructions: %d\n", i_type_count);
    printf("J-type instructions: %d\n", j_type_count);
    printf("Memory access instructions: %d\n", memory_access_count);
    printf("Taken branches: %d\n", branch_taken_count);

    return 0;
}


uint32_t fetch() {
    // Read in big-endian
    instruction = (memory[pc] << 24) | (memory[pc+1] << 16) | (memory[pc+2] << 8) | memory[pc+3];
    return instruction;
}


void decode(uint32_t instruction) {
    uint32_t opcode = instruction >> 26;
    if (opcode == 0x00) { // R-type
        r_type_count++;
    } else if (opcode == 0x02 || opcode == 0x03) { // J-type
        j_type_count++;
    } else { // I-type
        i_type_count++;
    }
    execute(instruction);
}


void memWrite(uint32_t address, uint32_t value) {
    if (address < MEMORY_SIZE) {
        // Check if the address is a multiple of 4 (word-aligned)
        if (address % 4 == 0) {
            memory[address] = (value >> 24) & 0xFF;
            memory[address + 1] = (value >> 16) & 0xFF;
            memory[address + 2] = (value >> 8) & 0xFF;
            memory[address + 3] = value & 0xFF;
        } else {
            printf("Memory write error: Address is not word-aligned\n");
        }
    } else {
        printf("Memory write error: Address out of bounds\n");
    }
}


void writeBack(uint32_t rd, uint32_t value) {
    reg[rd] = value;  // Write the value to the specified register
}


void execute(uint32_t instruction) {
    uint32_t opcode = instruction >> 26;
    uint32_t rs = (instruction >> 21) & 0x1F;
    uint32_t rt = (instruction >> 16) & 0x1F;
    uint32_t rd = (instruction >> 11) & 0x1F;
    uint32_t shamt = (instruction >> 6) & 0x1F;
    uint32_t funct = instruction & 0x3F;
    int16_t immediate = instruction & 0xFFFF;
    uint32_t address = instruction & 0x3FFFFFF;
    int32_t sign_extended_immediate = (int32_t)immediate;
    uint32_t value, mem_address;


    switch (opcode) {
        case 0x00: // R-type instructions
            switch (funct) {
                case 0x08: // jr
                    pc = reg[rs];
                    break;
                case 0x21: // addu 
                    value = reg[rs] + reg[rt];  
                    writeBack(rd, value);
                    break;
            }
            break;
        case 0x02: // J
            pc = (pc & 0xF0000000) | (address << 2);
            break;
        case 0x03: // JAL
            reg[31] = pc;  
            pc = (pc & 0xF0000000) | (address << 2);
            break;
        case 0x08: // ADDI
            value = reg[rs] + immediate;
            writeBack(rt, value);
            break;
        case 0x09: // ADDIU
            value = reg[rs] + sign_extended_immediate;
            writeBack(rt, value);  
            break;
        case 0x0A: // SLTI
            reg[rt] = (int32_t)reg[rs] < sign_extended_immediate ? 1 : 0;
            break;
        case 0x05: // BNE
            if (reg[rs] != reg[rt]) {
                pc += sign_extended_immediate << 2;
                branch_taken_count++;
            }
            break;
        case 0x0F: // LI
            reg[rt] = immediate;
            break;
        case 0x23: // LW
            mem_address = reg[rs] + sign_extended_immediate;
            if (mem_address % 4 == 0 && mem_address < MEMORY_SIZE) {
                value = (memory[mem_address] << 24) | (memory[mem_address + 1] << 16) |
                        (memory[mem_address + 2] << 8) | memory[mem_address + 3];
                writeBack(rt, value);
            } else {
                printf("Memory access error: Invalid address %08X\n", mem_address);
            }
            memory_access_count++;
            break;
        case 0x2B: // SW
            memWrite(reg[rs] + sign_extended_immediate, reg[rt]);
            memory_access_count++;
            break;
        default:
            printf("Unsupported opcode: %X\n", opcode);
    }

}


void loadBinary(const char* filename) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Error opening file");
        exit(1);
    }
    fread(memory, sizeof(uint8_t), MEMORY_SIZE, file);
    fclose(file);
}

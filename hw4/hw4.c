#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define MEMORY_SIZE 0x4000000 // 64MB memory
#define CACHE_SIZE 256 // 256 bytes
#define CACHE_LINE_SIZE 64 // 64 bytes per cache line
#define CACHE_WAYS 4 // 4-way set associative cache
#define SET_COUNT (CACHE_SIZE / (CACHE_LINE_SIZE * CACHE_WAYS))
#define MEMORY_LATENCY 1000 // Memory access latency in cycles

typedef enum { RANDOM, FIFO, LRU, SCA } ReplacementPolicy;
typedef enum { WRITE_BACK, WRITE_THROUGH } WritePolicy;

typedef struct {
    uint8_t data[CACHE_LINE_SIZE];
    uint32_t tag;
    int valid;
    int dirty;
    int lru_counter;
    int second_chance;
} CacheLine;

typedef struct {
    CacheLine lines[CACHE_WAYS];
    int fifo_index;
} CacheSet;

CacheSet cache[SET_COUNT];
uint8_t memory[MEMORY_SIZE];
uint32_t reg[32]; // 32bit registers
uint32_t pc = 0; // program counter
uint32_t instruction; // current instruction
int instruction_count = 0, memory_access_count = 0, branch_taken_count = 0, branch_total_count = 0;
int cache_hit_count = 0, cache_miss_count = 0;
int total_cycles = 0;
int register_operation_count = 0;

ReplacementPolicy replacement_policy = LRU;
WritePolicy write_policy = WRITE_BACK;

// Function declarations
uint32_t fetch();
void decode(uint32_t instruction);
void execute(uint32_t instruction);
void loadBinary(const char* filename);
uint32_t memAccess(uint32_t address, uint32_t value, int write);
void memWrite(uint32_t address, uint32_t value);
void cacheInitialize();
int cacheAccess(uint32_t address, uint8_t* data, int write);
float calculateAMAT();
CacheLine* selectCacheLine(CacheSet* set);

// Main function
int main() {
    // Initialize registers
    for (int i = 0; i < 32; ++i) {
        reg[i] = 0;
    }
    reg[29] = 0x1000000; // Initialize SP
    reg[31] = 0xFFFFFFFF; // Initialize LR

    memset(memory, 0, MEMORY_SIZE); // Initialize memory
    cacheInitialize(); // Initialize cache

    loadBinary("simple3.bin"); // Load binary file

    while (pc < MEMORY_SIZE && pc != 0xFFFFFFFF) {
        uint32_t instruction = fetch();
        printf("Fetched instruction at PC: %08X, Instruction: %08X\n", pc, instruction); // Debug output
        decode(instruction);
        instruction_count++;
    }

    printf("\n******************* Result ********************\n");
    printf("Total number of cycles of execution: %d\n", total_cycles);
    printf("Number of memory (load/store) operations: %d\n", memory_access_count);
    printf("Number of register operations: %d\n", register_operation_count);
    printf("Number of branches (total/taken): %d/%d\n", branch_total_count, branch_taken_count);
    printf("Cache hit/miss: %d/%d\n", cache_hit_count, cache_miss_count);
    printf("Average Memory Access Time (AMAT): %.2f cycles\n", calculateAMAT());
    printf("*************************************************");

    return 0;
}

// Initialize cache
void cacheInitialize() {
    for (int i = 0; i < SET_COUNT; i++) {
        for (int j = 0; j < CACHE_WAYS; j++) {
            cache[i].lines[j].valid = 0;
            cache[i].lines[j].dirty = 0;
            cache[i].lines[j].lru_counter = 0;
            cache[i].lines[j].second_chance = 0;
            memset(cache[i].lines[j].data, 0, CACHE_LINE_SIZE);
        }
        cache[i].fifo_index = 0;
    }
}

// Select cache line based on replacement policy
CacheLine* selectCacheLine(CacheSet* set) {
    switch (replacement_policy) {
        case RANDOM:
            return &set->lines[rand() % CACHE_WAYS];
        case FIFO:
            return &set->lines[set->fifo_index++ % CACHE_WAYS];
        case LRU: {
            CacheLine* lru_line = &set->lines[0];
            for (int i = 1; i < CACHE_WAYS; ++i) {
                if (set->lines[i].lru_counter < lru_line->lru_counter) {
                    lru_line = &set->lines[i];
                }
            }
            return lru_line;
        }
        case SCA: {
            CacheLine* sca_line = NULL;
            for (int i = 0; i < CACHE_WAYS; ++i) {
                if (set->lines[i].second_chance == 0) {
                    sca_line = &set->lines[i];
                    break;
                }
                set->lines[i].second_chance = 0;
            }
            if (sca_line == NULL) {
                sca_line = &set->lines[0];
            }
            return sca_line;
        }
        default:
            return &set->lines[0]; // 기본 값
    }
}

// Cache access function
int cacheAccess(uint32_t address, uint8_t* data, int write) {
    uint32_t tag = address / CACHE_SIZE;
    uint32_t set_index = (address / CACHE_LINE_SIZE) % SET_COUNT;
    uint32_t offset = address % CACHE_LINE_SIZE;
    CacheSet* set = &cache[set_index];

    for (int i = 0; i < CACHE_WAYS; i++) {
        CacheLine* line = &set->lines[i];
        if (line->valid && line->tag == tag) { // Cache hit
            if (write) {
                memcpy(line->data + offset, data, 4); // Writing 4 bytes
                if (write_policy == WRITE_BACK) {
                    line->dirty = 1;
                } else {
                    uint32_t mem_address = (line->tag * SET_COUNT + set_index) * CACHE_LINE_SIZE + offset;
                    memWrite(mem_address, *((uint32_t*)data));
                }
            } else {
                memcpy(data, line->data + offset, 4); // Reading 4 bytes
            }
            line->lru_counter = instruction_count;
            line->second_chance = 1;
            cache_hit_count++;
            total_cycles += 1; // Cache hit latency
            return 1; // Cache hit
        }
    }

    // Cache miss
    CacheLine* line = selectCacheLine(set);
    if (line->dirty) {
        uint32_t mem_address = (line->tag * SET_COUNT + set_index) * CACHE_LINE_SIZE;
        for (int i = 0; i < CACHE_LINE_SIZE; i += 4) {
            uint32_t value = *((uint32_t*)(line->data + i));
            memWrite(mem_address + i, value);
        }
    }

    line->valid = 1;
    line->tag = tag;
    line->lru_counter = instruction_count;
    line->second_chance = 1;
    line->dirty = write_policy == WRITE_BACK ? write : 0;

    uint32_t mem_address = (tag * SET_COUNT + set_index) * CACHE_LINE_SIZE;
    for (int i = 0; i < CACHE_LINE_SIZE; i += 4) {
        uint32_t value = memAccess(mem_address + i, 0, 0);
        *((uint32_t*)(line->data + i)) = value;
    }

    if (write) {
        memcpy(line->data + offset, data, 4);
    } else {
        memcpy(data, line->data + offset, 4);
    }

    printf("Cache miss: address=%08X, set_index=%d, tag=%d\n", address, set_index, tag); // Cache miss debug output

    cache_miss_count++;
    total_cycles += MEMORY_LATENCY; // Cache miss latency
    return 0; // Cache miss
}

uint32_t fetch() {
    uint8_t data[4];
    cacheAccess(pc, data, 0);
    instruction = (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
    printf("Fetched instruction at PC: %08X, Instruction: %08X\n", pc, instruction); // Debug output
    total_cycles++;
    return instruction;
}

void decode(uint32_t instruction) {
    uint32_t opcode = instruction >> 26;
    printf("Decoding instruction at PC: %08X, Instruction: %08X, opcode: %02X\n", pc, instruction, opcode); // Debug output
    // if (opcode == 0x00) { // R-type
    //     register_operation_count++; // R-type instruction is a register operation
    // } else if (opcode == 0x02 || opcode == 0x03) { // J-type
    // } else { // I-type
    //     // Add register operations for I-type instructions
    //     if (opcode == 0x08 || opcode == 0x09 || opcode == 0x0A || opcode == 0x0B || 
    //         opcode == 0x0C || opcode == 0x0D || opcode == 0x0E || opcode == 0x0F) {
    //         register_operation_count++;
    //     }
    // }
    execute(instruction);
}

void writeBack(uint32_t rd, uint32_t value) {
    reg[rd] = value;  // Write the value to the specified register
}

uint32_t memAccess(uint32_t address, uint32_t value, int write) {
    if (address < MEMORY_SIZE) {
        if (address % 4 == 0) { // Ensure word alignment
            if (write) {
                memWrite(address, value);
            } else {
                return (memory[address] << 24) | (memory[address + 1] << 16) |
                       (memory[address + 2] << 8) | memory[address + 3];
            }
        } else {
            printf("Memory access error: Address is not word-aligned\n");
        }
    } else {
        printf("Memory access error: Address out of bounds\n");
    }
    return 0;
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

void execute(uint32_t instruction) {
    uint32_t opcode = instruction >> 26;
    uint32_t rs = (instruction >> 21) & 0x1F;
    uint32_t rt = (instruction >> 16) & 0x1F;
    uint32_t rd = (instruction >> 11) & 0x1F;
    uint32_t shamt = (instruction >> 6) & 0x1F;
    uint32_t funct = instruction & 0x3F;
    int16_t immediate = instruction & 0xFFFF;
    uint32_t address = instruction & 0x3FFFFFF;
    int32_t sign_extended_immediate = (int32_t)(int16_t)immediate; // sign-extend immediate
    uint32_t value, mem_address;

    printf("Executing instruction at PC: %08X, Instruction: %08X\n", pc, instruction); // Debug output

    switch (opcode) {
        case 0x00: // R-type instructions
            register_operation_count++; // R-type instruction is a register operation
            switch (funct) {
                case 0x00: // sll
                    value = reg[rt] << shamt;
                    writeBack(rd, value);
                    break;
                case 0x02: // srl
                    value = reg[rt] >> shamt;
                    writeBack(rd, value);
                    break;
                case 0x08: // jr
                    printf("Executing JR, PC before: %08X, JR to: %08X\n", pc, reg[rs]); // Debug output
                    pc = reg[rs];
                    break;
                case 0x20: // add
                    value = (int32_t)reg[rs] + (int32_t)reg[rt];
                    writeBack(rd, value);
                    break;
                case 0x21: // addu
                    value = reg[rs] + reg[rt];
                    writeBack(rd, value);
                    break;
                case 0x22: // sub
                    value = (int32_t)reg[rs] - (int32_t)reg[rt];
                    writeBack(rd, value);
                    break;
                case 0x23: // subu
                    value = reg[rs] - reg[rt];
                    writeBack(rd, value);
                    break;
                case 0x24: // and
                    value = reg[rs] & reg[rt];
                    writeBack(rd, value);
                    break;
                case 0x25: // or
                    value = reg[rs] | reg[rt];
                    writeBack(rd, value);
                    break;
                case 0x26: // xor
                    value = reg[rs] ^ reg[rt];
                    writeBack(rd, value);
                    break;
                case 0x27: // nor
                    value = ~(reg[rs] | reg[rt]);
                    writeBack(rd, value);
                    break;
                case 0x2A: // slt
                    reg[rd] = (int32_t)reg[rs] < (int32_t)reg[rt] ? 1 : 0;
                    break;
                case 0x2B: // sltu
                    reg[rd] = reg[rs] < reg[rt] ? 1 : 0;
                    break;
                default:
                    printf("Unsupported R-type funct: %X\n", funct);
            }
            break;
        case 0x02: // J
            printf("Executing J, PC before: %08X, J to: %08X\n", pc, (pc & 0xF0000000) | (address << 2)); // Debug output
            pc = (pc & 0xF0000000) | (address << 2);
            break;
        case 0x03: // JAL
            printf("Executing JAL, PC before: %08X, JAL to: %08X\n", pc, (pc & 0xF0000000) | (address << 2)); // Debug output
            reg[31] = pc + 4;
            pc = (pc & 0xF0000000) | (address << 2);
            break;
        case 0x04: // BEQ
            branch_total_count++; // 전체 분기 수 증가
            if (reg[rs] == reg[rt]) {
                printf("Executing BEQ, PC before: %08X, BEQ to: %08X\n", pc, pc + (sign_extended_immediate << 2)); // Debug output
                pc = pc + 4 + (sign_extended_immediate << 2);
                branch_taken_count++;
            }
            break;
        case 0x05: // BNE
            branch_total_count++; // 전체 분기 수 증가
            if (reg[rs] != reg[rt]) {
                printf("Executing BNE, PC before: %08X, BNE to: %08X\n", pc, pc + (sign_extended_immediate << 2)); // Debug output
                pc = pc + 4 + (sign_extended_immediate << 2);
                branch_taken_count++;
            }
            break;
        case 0x08: // ADDI
            register_operation_count++; // I-type instruction ADDI is a register operation
            value = reg[rs] + immediate;
            writeBack(rt, value);
            break;
        case 0x09: // ADDIU
            register_operation_count++; // I-type instruction ADDIU is a register operation
            value = reg[rs] + sign_extended_immediate;
            writeBack(rt, value);
            break;
        case 0x0A: // SLTI
            register_operation_count++; // I-type instruction SLTI is a register operation
            reg[rt] = (int32_t)reg[rs] < sign_extended_immediate ? 1 : 0;
            break;
        case 0x0B: // SLTIU
            register_operation_count++; // I-type instruction SLTIU is a register operation
            reg[rt] = reg[rs] < (uint32_t)sign_extended_immediate ? 1 : 0;
            break;
        case 0x0C: // ANDI
            register_operation_count++; // I-type instruction ANDI is a register operation
            reg[rt] = reg[rs] & (uint32_t)immediate;
            break;
        case 0x0D: // ORI
            register_operation_count++; // I-type instruction ORI is a register operation
            reg[rt] = reg[rs] | (uint32_t)immediate;
            break;
        case 0x0E: // XORI
            register_operation_count++; // I-type instruction XORI is a register operation
            reg[rt] = reg[rs] ^ (uint32_t)immediate;
            break;
        case 0x0F: // LUI
            register_operation_count++; // I-type instruction LUI is a register operation
            reg[rt] = immediate << 16;
            break;
        case 0x23: // LW
            mem_address = reg[rs] + sign_extended_immediate;
            if (mem_address % 4 == 0 && mem_address < MEMORY_SIZE) {
                uint8_t data[4];
                cacheAccess(mem_address, data, 0);
                value = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
                writeBack(rt, value);
                printf("Loaded value to v0: %d\n", reg[rt]); // Debugging output
            } else {
                printf("Memory access error: Address is not word-aligned or out of bounds\n");
            }
            memory_access_count++;
            break;
        case 0x2B: // SW
            mem_address = reg[rs] + sign_extended_immediate;
            if (mem_address % 4 == 0 && mem_address < MEMORY_SIZE) {
                uint8_t data_sw[4] = {
                    (reg[rt] >> 24) & 0xFF,
                    (reg[rt] >> 16) & 0xFF,
                    (reg[rt] >> 8) & 0xFF,
                    reg[rt] & 0xFF
                };
                cacheAccess(mem_address, data_sw, 1);
                printf("Stored value from v0: %d\n", reg[rt]); // Debugging output
            } else {
                printf("Memory access error: Address is not word-aligned or out of bounds\n");
            }
            memory_access_count++;
            break;
        default:
            printf("Unsupported opcode: %X\n", opcode);
    }
    if (!(opcode == 0x02 || opcode == 0x03 || opcode == 0x04 || opcode == 0x05 || (opcode == 0x00 && funct == 0x08))) {
        pc += 4;
    }
}


void loadBinary(const char* filename) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Error opening file");
        exit(1);
    }
    size_t bytesRead = fread(memory, sizeof(uint8_t), MEMORY_SIZE, file);
    fclose(file);
    printf("Loaded %zu bytes from %s\n", bytesRead, filename);
}

float calculateAMAT() {
    float hit_time = 1.0f; // Cache hit time in cycles
    float miss_penalty = MEMORY_LATENCY; // Cache miss penalty in cycles
    float miss_rate = (float)cache_miss_count / (cache_hit_count + cache_miss_count);
    return hit_time + miss_rate * miss_penalty;
}

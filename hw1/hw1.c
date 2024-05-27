#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INSTRUCTION_LENGTH 100

// 레지스터
int reg[10] = {0}; 
char inst_reg[MAX_INSTRUCTION_LENGTH]; // 명령어 문자열 저장
int inst_ptr = 0; // 명령 포인터

void calculator() {
    char op;
    char arg1[20], arg2[20];
    int val1, val2, res;

    // 연산 타입 확인
    sscanf(inst_reg, "%c %s %s", &op, arg1, arg2);

    // 피연산자 식별
    if (arg1[0] == '0' && arg1[1] == 'x') // 16진수 확인
        sscanf(arg1, "%x", &val1);
    else // 레지스터로 가정
        sscanf(arg1, "R%d", &val1);

    if (arg2[0] == '0' && arg2[1] == 'x') // 16진수 확인
        sscanf(arg2, "%x", &val2);
    else // 레지스터로 가정
        sscanf(arg2, "R%d", &val2);

    switch (op) { // 연산
        case '+':
            res = (arg1[0] == 'R' ? reg[val1] : val1) + (arg2[0] == 'R' ? reg[val2] : val2);
            reg[0] = res;
            printf("R0: %d = R%d + R%d\n", res, val1, val2);
            break;
        case '-':
            res = (arg1[0] == 'R' ? reg[val1] : val1) - (arg2[0] == 'R' ? reg[val2] : val2);
            reg[0] = res;
            printf("R0: %d = R%d - R%d\n", res, val1, val2);
            break;
        case '*':
            res = (arg1[0] == 'R' ? reg[val1] : val1) * (arg2[0] == 'R' ? reg[val2] : val2);
            reg[0] = res;
            printf("R0: %d = R%d * R%d\n", res, val1, val2);
            break;
        case '/':
            if ((arg2[0] == 'R' ? reg[val2] : val2) == 0) {
                printf("Error: Division by zero.\n");
                return;
            }
            res = (arg1[0] == 'R' ? reg[val1] : val1) / (arg2[0] == 'R' ? reg[val2] : val2);
            reg[0] = res;
            printf("R0: %d = R%d / R%d\n", res, val1, val2);
            break;
        case 'M': // 이동 연산
            if (arg1[0] == '0' && arg1[1] == 'x') { 
                sscanf(arg1, "%x", &val1);
                reg[val2] = val1;
                printf("R%d: %d\n", val2, val1);
            } else { 
                reg[val2] = reg[val1];
                printf("R%d: %d\n", val2, reg[val1]);
            }
            break;
        case 'C': // 비교 연산
            {
                int operand1 = (arg1[0] == 'R' ? reg[val1] : val1);
                int operand2 = (arg2[0] == 'R' ? reg[val2] : val2);
                
                if (operand1 > operand2) {
                    reg[0] = 1;
                } else if (operand1 < operand2) {
                    reg[0] = -1;
                } else {
                    reg[0] = 0;
                }

                printf("Comparison Result in R0: %d\n", reg[0]);
            }
            break;
        case 'H': // 중지 연산
            printf("Halt.\n");
            exit(0); // 프로그램 중지
            break;
        default:
            printf("Unsupported operation: %c\n", op);
    }
}

int main() {
    FILE *file = fopen("input.txt", "r");

    if (!file) {
        printf("Error: Could not open input.txt\n");
        return 1;
    }

    while (fgets(inst_reg, sizeof(inst_reg), file)) {
        calculator();
        inst_ptr++; // 명령 실행 후 명령 포인터 증가 (실질적으로 사용되지 않음)
    }

    fclose(file);
    return 0;
}

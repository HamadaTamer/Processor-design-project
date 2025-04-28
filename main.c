#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define N 2048

uint32_t memory[N];  // word‐addressable main memory


// Registers
int pc =0;
const uint32_t zeroReg = 0;
uint32_t Regs[32]; // 32 registers

void print_bin32(uint32_t x) {
    for (int i = 31; i >= 0; i--) {
        putchar( (x >> i & 1) ? '1' : '0' );
        if (i % 8 == 0 && i!=0) putchar(' ');  // optional byte‐spacing
    }
    putchar('\n');
}

#define MAX_LINE 128
#define MAX_INST  1024

int load_instructions(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error opening \"%s\": %s\n", filename, strerror(1));
        return -1;
    }

    char buffer[MAX_LINE];
    int count = 0;
    while (fgets(buffer, sizeof buffer, fp)) {
        // strip newline
        buffer[strcspn(buffer, "\r\n")] = '\0';

        // tokenize
        char *op = strtok(buffer, " \t");
        if (!op) continue;  // blank line

        if (strcmp(op, "J") == 0) {
            char *addr_tok = strtok(NULL, " \t");
            if (!addr_tok) { fprintf(stderr, "J missing addr at line %d\n", count+1); continue; }
            int address = atoi(addr_tok);
            if (count < MAX_INST)
                memory[count++] = (7 << 28) | address;
        }
        else if (strcmp(op, "ADD") == 0 || strcmp(op, "SUB") == 0  || strcmp(op, "SLL") == 0 || strcmp(op, "SRL") == 0) {
            char *d_tok = strtok(NULL, " ");
            char *t_tok = strtok(NULL, " ");
            char *s_tok = strtok(NULL, " ");
            int rd, rs, rt= 0;

            if (strcmp(op, "SLL") == 0 || strcmp(op, "SRL") == 0) {
                rd = atoi(d_tok+1), rt = atoi(t_tok+1),rs = atoi(s_tok);
            }else{
                rd = atoi(d_tok+1), rt = atoi(t_tok+1),rs = atoi(s_tok+1);
            }
            
            int opcode = (strcmp(op,"ADD")==0) ? 0
                       : (strcmp(op,"SUB")==0) ? 1
                       : (strcmp(op,"SLL")==0) ? 8 : 9;
            if (strcmp(op, "SLL") == 0 || strcmp(op, "SRL") == 0) {
                memory[count++] = (opcode << 28)    | (rd << 23) | (rt << 18)    | (rs );

                printf("SLL or SRL opcode = %d rs = %d rd = %d rt = %d\n", opcode, rs, rd, rt);
            } else {
                printf("opcode = %d rs = %d rd = %d rt = %d\n", opcode, rs, rd, rt);
                memory[count++] = (opcode << 28)    | (rd << 23) | (rt << 18)    | (rs << 13);
            }
        }
        else if (strcmp(op, "ADDI")==0 || strcmp(op, "ANDI")==0
              || strcmp(op, "MULI")==0 || strcmp(op, "ORI")==0
              || strcmp(op, "LW")==0   || strcmp(op, "SW")==0
              || strcmp(op, "BNE")==0) {
            char *d_tok = strtok(NULL, " ");
            char *s_tok = strtok(NULL, " ");
            char *imm_tok= strtok(NULL, " ");
            if (!d_tok||!s_tok||!imm_tok) {
                fprintf(stderr, "Bad I-type at line %d\n", count+1);
                continue;
            }
            int rd = atoi(d_tok+1), rs = atoi(s_tok+1),
                imm = atoi(imm_tok);
            int opcode;
            if      (strcmp(op,"MULI")==0) opcode = 2;
            else if (strcmp(op,"ADDI")==0) opcode = 3;
            else if (strcmp(op,"BNE")==0) opcode = 4;
            else if (strcmp(op,"ANDI")==0) opcode = 5;
            else if (strcmp(op,"ORI")==0)   opcode = 6;
            else if (strcmp(op,"LW")==0)   opcode = 10;
            else if (strcmp(op,"SW")==0)   opcode = 11;
            printf("opcode = %d rs = %d rd = %d imm = %d\n", opcode, rs, rd, imm);
            
            if (count < MAX_INST)
                memory[count++] = (opcode << 28)
                                | (rs << 23) | (rd << 18)
                                | imm;
        }
        else {
            fprintf(stderr, "Unknown opcode ‘%s’ at line %d\n", op, count+1);
        }
    }

    fclose(fp);
    return count;
}
void printMemory(){
    int i =0;
    while (memory[i] != 0) {
        print_bin32(memory[i]);
        //printf("0x%08x\n", memory[i]);
        i++;
    }
}

int main(){
    load_instructions("program2.txt");
    printMemory();

    return 0;
}
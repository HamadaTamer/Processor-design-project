#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>


#define N 2048

uint32_t memory[N];  // word‐addressable main memory
int clockCycles = 0; // clock cycle counter 


// Registers
int PC =0;
const uint32_t zeroReg = 0;
uint32_t Regs[31]; // 31 registers



void print_bin32(uint32_t x) {
    for (int i = 31; i >= 0; i--) {
        putchar( (x >> i & 1) ? '1' : '0' );
        if (i % 8 == 0 && i!=0) putchar(' ');  // optional byte‐spacing
    }
    putchar('\n');
}

#define MAX_LINE 128
#define MAX_INST  1024

/*
    so the idea is that the PipeReg will be what we will use to track the pipeline stages and allow them to feed information to each other
    the pipeline will be 5 stages: IF, ID, EX, MEM, WB
    the pipe array will be used in the following sense:
    pipe [0] is where the instruction is fetched onto 
    ID will take from pip[0], decode, and send to pipe[1]
    EX will take from pipe[1], execute and send to pipe[2]
    MEM will take from pipe[2], access memory and send to pipe[3]
    WB will take from pipe[3], write back to the register file and voila
*/
struct PipeReg{
    uint32_t instr;     // original word
    uint32_t aluOut;    // result or address
    uint32_t srcA, srcB;// operand values
    uint32_t rd;        // destination register #
    bool     valid;     // is this slot occupied?
}PipeReg;

struct PipeReg pipe[4]; // oldest instruction at pipe[3]

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
            } else {
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

// takes 2 clock cycles
void fetch(){
    pipe[0].instr = memory[PC];
    pipe[0].valid = true;
    PC++;
}

void decode(){
    if (!pipe[0].valid) 
        return; // nothing to decode

    uint32_t instr = pipe[0].instr;
    int opcode = (instr >> 28) & 0xF;


    int R_rd = (instr >> 23) & 0x1F;
    int R_rt = (instr >> 18) & 0x1F;
    int R_rs = (instr >> 23) & 0x1F;
    int R_shamt = instr & 0x1FFF;

    int I_rd = (instr >> 23) & 0x1F;
    int I_rs = (instr >> 18) & 0x1F;
    int I_imm = (instr) & 0x2FFFF;
    

    int J_addr = (instr) & 0xFFFFFFF;

    if(clockCycles % 2 == 1 && pipe[0].valid){
        if( opcode == 2 || opcode == 3 || opcode == 4 || opcode == 5 || opcode == 6 || opcode == 10 || opcode == 11){
            // I-type
            pipe[1].srcA = Regs[I_rs];
            pipe[1].srcB = I_imm;
            pipe[1].rd = I_rd;
        }else if(opcode == 7){
            // J-type
            pipe[1].srcA = PC;
            pipe[1].srcB = J_addr;
            pipe[1].rd = 0; // no destination register for J-type
        }else if(opcode == 0 || opcode == 1 || opcode == 8 || opcode == 9){
            // R-type
            pipe[1].srcA = Regs[R_rs];
            pipe[1].srcB = Regs[R_rt];
            pipe[1].rd = R_rd;
        }else{
            // invalid opcode
            fprintf(stderr, "Invalid opcode %d at PC %d\n", opcode, PC);
            return;
        }
        pipe[1].valid = true;
    }


    // decode the instruction and set the appropriate values in the pipeline register
        

    
}



int main(){
    load_instructions("program2.txt");
    printMemory();

    return 0;
}



/*
tracking the pipeline stages

*/
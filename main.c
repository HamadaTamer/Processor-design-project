#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>


#define N 2048

uint32_t memory[N];  // word‐addressable main memory
int clockCycles = 1; // clock cycle counter 
int instCount   = 0;


// Registers
int PC =0;
// const uint32_t zeroReg = 0;
uint32_t Regs[32]; // 31 registers

typedef struct PipeReg{
    uint32_t instr;     // original word
    uint32_t opcode;
    uint32_t aluOut;    // result or address
    int32_t srcA, srcB;
    uint32_t shamt;// operand values
    uint32_t  pc;  
    uint32_t* rd;        // destination register 
    bool     valid;     // is this slot occupied/valid?
} PipeReg;

 PipeReg pipe[4] = {0}; // oldest instruction at pipe[3]


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
            int rd, rs, rt;

            if (strcmp(op, "SLL") == 0 || strcmp(op, "SRL") == 0) {
                rd = atoi(d_tok+1), rt = atoi(t_tok+1),rs = atoi(s_tok);
            }else{
                rd = atoi(d_tok+1),rs = atoi(s_tok+1),rt = atoi(t_tok+1);
            }
            
            int opcode = (strcmp(op,"ADD")==0) ? 0
                       : (strcmp(op,"SUB")==0) ? 1
                       : (strcmp(op,"SLL")==0) ? 8 : 9;
            if (strcmp(op, "SLL") == 0 || strcmp(op, "SRL") == 0) {
                memory[count++] = (opcode << 28)    | (rd << 23) | (rt << 18)    | (rs );
            } else {
                memory[count++] = (opcode << 28)    | (rd << 23) | (rs << 18)    | (rt << 13);
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
            int rd = atoi(d_tok+1), rs = atoi(s_tok+1),imm = atoi(imm_tok);
            int opcode;
            if      (strcmp(op,"MULI")==0) opcode = 2;
            else if (strcmp(op,"ADDI")==0) opcode = 3;
            else if (strcmp(op,"BNE")==0) opcode = 4;
            else if (strcmp(op,"ANDI")==0) opcode = 5;
            else if (strcmp(op,"ORI")==0)   opcode = 6;
            else if (strcmp(op,"LW")==0)   opcode = 10;
            else if (strcmp(op,"SW")==0)   opcode = 11;
            
            if (count < MAX_INST)
                memory[count++] = (opcode << 28)| (rd << 23) | (rs << 18)| imm;
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
    while (memory[i] != 0 ) {
        print_bin32(memory[i]);
        //printf("0x%08x\n", memory[i]);
        i++;
    }
}

// takes 2 clock cycles
void fetch(){
    if (clockCycles % 2 == 1 && PC < instCount){
        printf("fetching instruction %d\n", PC);
        pipe[0].instr = memory[PC];
        pipe[0].pc    = PC;        
        pipe[0].valid = true;
        PC++;
    }
}

void decode(){
    if (pipe[0].valid && clockCycles % 2 != 1){
        printf("decoding instruction (stall) 0x%08x\n", pipe[0].instr);
    }
    if(clockCycles % 2 == 1 && pipe[0].valid){

        printf("decoding instruction 0x%08x\n", pipe[0].instr);
        uint32_t instr = pipe[0].instr;
        int opcode = (instr >> 28) & 0xF;


        int rd = (instr >> 23) & 0x1F;          /* bits 27-23 */
        int rs = (instr >> 18) & 0x1F;          /* bits 22-18 – I-type dest */

       int R_rs2   = (instr >> 13) & 0x1F;      /* bits 17-13 – real rs for R-type */
        int R_shamt=  instr         & 0x1FFF;

        // int I_rd = (instr >> 23) & 0x1F;
        // int I_rs = (instr >> 18) & 0x1F;
        int imm = (instr) & 0x3FFFF;
        

        int J_addr = (instr) & 0xFFFFFFF;

        if (opcode == 4) {                   /* BNE r1,r2,offset              */
            pipe[1].srcA = Regs[rd];     /* r1 value                       */
            pipe[1].srcB = Regs[rs];     /* r2 value                       */
            pipe[1].shamt = (int16_t)imm;/* keep the signed offset         */
        }else if( opcode == 2 || opcode == 3 || opcode == 4 || opcode == 5 || opcode == 6 || opcode == 10 || opcode == 11){
            // I-type
            pipe[1].srcA = Regs[rs];
            pipe[1].srcB = imm;
            pipe[1].rd = &Regs[rd];
        }else if(opcode == 7){
            // J-type
            pipe[1].srcA = PC;
            pipe[1].srcB = J_addr;
            pipe[1].rd = 0; // no destination register for J-type
        }else if(opcode == 0 || opcode == 1 || opcode == 8 || opcode == 9){
            // R-type
            pipe[1].srcA = Regs[R_rs2];
            pipe[1].srcB = Regs[rs];
            pipe[1].rd = &Regs[rd];
            pipe[1].shamt = R_shamt;
        }else{
            // invalid opcode
            fprintf(stderr, "Invalid opcode %d at PC %d\n", opcode, PC);
            return;
        }
        pipe[1].opcode = opcode;
        pipe[1].pc = PC;
        pipe[1].instr = pipe[0].instr;
        pipe[1].valid = true;  
        pipe[0].instr = 0; // clear the instruction in the fetch stage
        pipe[0].valid  = false; 
    }
}


void execute(){
    if(pipe[1].valid ){
        printf("executing instruction 0x%08x\n", pipe[1].instr);
        switch (pipe[1].opcode)
        {
        case 0:
        case 3:
            pipe[2].aluOut = pipe[1].srcA + pipe[1].srcB;
            break;
        case 1:
            pipe[2].aluOut = pipe[1].srcA * pipe[1].srcB;
            break;
        case 2:
            pipe[2].aluOut = pipe[1].srcA - pipe[1].srcB;
            break;
        case 4:

        /*
            pipe[1].srcA = Regs[rs];
            pipe[1].srcB = imm;
            pipe[1].rd = &Regs[rd];
        */
            if (Regs[*pipe[1].rd != pipe[1].srcA]){
                PC = pipe[1].pc + pipe[1].shamt;
                pipe[0].valid = false;   /* flush the instruction that was     */
     
            }
            break;
        case 5:
            pipe[2].aluOut = pipe[1].srcA & pipe[1].srcB;
            break;
        case 6:
            pipe[2].aluOut = pipe[1].srcA | pipe[1].srcB;
            break;
        case 7:
            PC = ((PC & 0x70000000) | pipe[1].srcB) -1;
            pipe[0].valid = false;    /* flush IF  */

            break;
            case 8:
            pipe[2].aluOut = pipe[1].srcB << pipe[1].shamt;
            break;
        case 9:
            pipe[2].aluOut = pipe[1].srcB >> pipe[1].shamt; 
            break;
        default:
            break;
        }
        if (clockCycles %2 == 1){
            pipe[2].valid = true;
            if (pipe[1].opcode != 7)
                pipe[2].rd = pipe[1].rd;
            pipe[2].instr = pipe[1].instr;
            pipe[1].valid = false;
            //pipe[1].instr = 0; // clear the instruction in the decode stage
        }
    }
}


void memory_rw(){
    // if(clockCycles %2 == 0 && pipe[2].valid){
    //     //pipe[3].aluOut = &(memory[1024 + pipe[2].srcA + pipe[2].srcB]);
        
    //     if((pipe[2].instr << 28) & 0xa){
    //         pipe[3].aluOut =  memory[1024 + pipe[2].srcA + pipe[2].srcB];
    //     }else if ((pipe[2].instr << 28) & 0xb){
    //        memory[1024 + pipe[2].srcA + pipe[2].srcB] =  *(pipe[2].rd);
    //     }
    //     pipe[3].valid = true;

    // }


    if (clockCycles % 2 == 0 && pipe[2].valid) {
        printf("memory access instruction %d\n", PC);
        uint32_t op = (pipe[2].instr >> 28) & 0xF;

        switch (op) {
        case 10:  /* LW */
            pipe[3].aluOut = memory[1024 + pipe[2].srcA + pipe[2].srcB];
            pipe[3].rd     = pipe[2].rd;   // forward pointer
            pipe[3].valid  = true;
            break;

        case 11:  /* SW */
            memory[1024 + pipe[2].srcA + pipe[2].srcB] = *(pipe[2].rd);
            pipe[3].valid = false;         // nothing to write back
            break;

        default:  /* ALU instruction – just forward result/address */
            pipe[3] = pipe[2];
            break;
        }

        // pipe[3].instr = pipe[2].instr;
        pipe[2].valid = false;
        pipe[2].instr = 0; // clear the instruction in the execute stage
    }
}

void write_back(){
    if (clockCycles % 2 == 1 && pipe[3].valid && pipe[3].rd && pipe[3].rd != &Regs[0] ){
        *(pipe[3].rd) = pipe[3].aluOut;
        pipe[3].valid = false;
        pipe[3].instr = pipe[2].instr;  
        printf("writing back instruction %d\n", PC);
        pipe[3].valid = false;
    }  

}

/* -----------------------------------------------------------
 *  Debug dump:  registers, PC, and every pipeline latch
 * ----------------------------------------------------------- */
static void print_state(void)
{
    printf("\n================  CYCLE %d  ================\n", clockCycles);

    /*--- PC ---*/
    printf("PC  : %d (0x%08x)\n\n", PC, PC);

    /*--- GENERAL-PURPOSE REGISTERS ---*/
    for (int i = 0; i < 32; ++i) {
        printf("R%02d:%5u  ", i, Regs[i]);
        if ((i & 7) == 7) putchar('\n');           /* break every 8 registers */
    }
    putchar('\n');

    /*--- PIPELINE REGISTERS ---*/
    const char *stage[4] = { "IF ", "ID ", "EX ", "MEM" };
    for (int i = 0; i < 4; ++i) {
        printf("%s | valid:%d  instr:0x%08x  op:%2u  A:%d  B:%d  sh:%u  alu:0x%08x  rd:",
               stage[i],
               pipe[i].valid,
               pipe[i].instr,
               pipe[i].opcode,
               pipe[i].srcA,
               pipe[i].srcB,
               pipe[i].shamt,
               pipe[i].aluOut);

        /* pretty-print destination register pointer */
        if (!pipe[i].rd)                 printf("--");
        else if (pipe[i].rd == &PC)      printf("PC");
        else                             printf("R%ld", pipe[i].rd - Regs);

        putchar('\n');
    }
    puts("============================================\n");
}

int main(){
    printf("Hello World!\n");
    instCount = load_instructions("program2.txt");
    printMemory();
    int i=0;        // infinite loop gaurd 
    while ((PC < instCount   || pipe[0].valid || pipe[1].valid ||pipe[2].valid || pipe[3].valid) && i++ < 40 ){
        write_back();
        memory_rw();
        execute();
        decode();
        fetch();
        print_state();
        clockCycles++;
        printf("clock cycle %d\n", clockCycles);
    }

    return 0;
}

/*

fetch happens first then flush
flush happens after second execute
*/

/***************************************************************/
/*                                                             */
/*   MIPS-32 Instruction Level Simulator                       */
/*                                                             */
/*   SCE212 Ajou University                                    */
/*   run.c                                                     */
/*   Adapted from Computer Architecture@KAIST                  */
/*                                                             */
/***************************************************************/

#include <stdio.h>

#include "util.h"
#include "run.h"
#include "parse.h"

instruction inst;
instruction* instr;
uint32_t ex_pc;
uint32_t id_pc;
uint32_t mem_pc;
uint32_t wb_pc;
int isjump;
int isbranch;
unsigned char rs,rt,rd,imm,shmat;

int count = 0;

    /* for control signal */
	char MemRead;
	char MemtoReg;
	char ALUOp;
	char MemWrite;
	char ALUSrc;
	char RegWrite;
	char ALUNot;

	uint32_t read_reg1;
	uint32_t read_reg2;
	
	unsigned char dest;
	
/***************************************************************/
/*                                                             */
/* Procedure: get_inst_info                                    */
/*                                                             */
/* Purpose: Read insturction information                       */
/*                                                             */
/***************************************************************/
instruction* get_inst_info(uint32_t pc) {
    return &INST_INFO[(pc - MEM_TEXT_START) >> 2];
}

/***************************************************************/
/*                                                             */
/* Procedure: process_instruction                              */
/*                                                             */
/* Purpose: Process one instrction                             */
/*                                                             */
/***************************************************************/

void process_instruction() 
{
   
   
    for (int i = PIPE_STAGE - 1; i > 0; i--)
        CURRENT_STATE.PIPE[i] = CURRENT_STATE.PIPE[i-1];
    CURRENT_STATE.PIPE[IF_STAGE] = CURRENT_STATE.PC;
    
	instr = get_inst_info(CURRENT_STATE.PC);

	CURRENT_STATE.PC += 4;

    WB_Stage();

	MEM_Stage();
	
    if (!CURRENT_STATE.load_use) {
        CURRENT_STATE.load_use = 0;
        EX_Stage();

		ID_Stage();

        IF_Stage();
    }

	INSTRUCTION_COUNT++;
}

void IF_Stage()
{
	if(CURRENT_STATE.PC > MEM_TEXT_START + 4 * (NUM_INST))
	{
		//FLUSH_EX();
		//FLUSH_ID();
		FETCH_BIT=FALSE;
		++count;
		CURRENT_STATE.PC -=4;
		FLUSH_IF();
		
		CURRENT_STATE.REGS[16]=0;
		if(count==4) RUN_BIT = FALSE;
	} 
    if(FETCH_BIT){

    // inst=INST_INFO[(CURRENT_STATE.PC - MEM_TEXT_START)>>2]; // FETCH
    // MOVE FOR NEXT STAGE !!

    inst=INST_INFO[(CURRENT_STATE.PC - MEM_TEXT_START)>>2]; // FETCH

	CURRENT_STATE.IF_ID_INST=inst; // store fetched instruction

	CURRENT_STATE.IF_ID_NPC=CURRENT_STATE.PC+4; // store NPC
    }
}

void FLUSH_IF()
{
	CURRENT_STATE.PIPE[IF_STAGE] = 0;
	memset(&(CURRENT_STATE.IF_ID_INST),0,sizeof(instruction));
    CURRENT_STATE.IF_ID_NPC=0;
}

void ID_Stage()
{
		
	/* for decode */
	short opcode;
	short func;

	id_pc=CURRENT_STATE.IF_ID_NPC;

	//instr = &(CURRENT_STATE.IF_ID_INST); // Load inst data


	opcode = OPCODE(instr);
	func = FUNC(instr);

    /* opcode information

        case 0x0: 
	    case 0x9:		// ADDIU 
		case 0xc:		// ANDI 
		case 0xf:		// LUI	
		case 0xd:		// ORI
		case 0xb:		// SLTIU
		case 0x23:		// LW
		case 0x2b:		// SW
		case 0x4:		// BEQ
		case 0x5:		// BNE
		case 0x2:		// J
		case 0x3:		// JAL
	
	*/

// read reg value
read_reg1=CURRENT_STATE.REGS[RS(instr)];
read_reg2=CURRENT_STATE.REGS[RT(instr)];

// for store id_ex register
rs=RS(instr);
rt=RT(instr);
rd=RD(instr);
imm=IMM(instr);
shmat = SHAMT(instr);

// mux for dest reg number
if(opcode==0x0 || opcode == 0x3) dest = RD(instr);
else dest = RT(instr);

// generate control signal

// ALU INPUT MUX IMM vs RT value
switch(opcode)
{
	case 0x9:		// ADDIU 
	case 0xc:		// ANDI 
	case 0xf:		// LUI	
	case 0xd:		// ORI
	case 0xb:		// SLTIU
	case 0x23:		// LW
	case 0x2b:		// SW
	case 0x4:		// BEQ
	case 0x5:		// BNE
	ALUSrc=1; // i - type
	break;
	case 0x0:
	ALUSrc=0; // r-type
	break;
	default:
	break;
}
// Reg Write - j, sw, beq, bne, jr - don't write reg insts
if(opcode == 0x2 || opcode == 0x2b || opcode == 0x4 || opcode==0x5 || (opcode == 0x0 && func == 0x8)) RegWrite=0;
else RegWrite=1;
// Branch
if(opcode == 0x4) CURRENT_STATE.id_Branch_beq = 1;
if(opcode == 0x5) CURRENT_STATE.id_Branch_bne = 1;
// Mem Read and Mem to reg - lw
if(opcode == 0x23) {
	MemRead = 1;
	MemtoReg = 1;
}
// Mem Write - sw
if(opcode == 0x2b) MemWrite=1;
// ALUOp and handle j, jr, jal
switch(opcode) // lecture note mips processor - control ALU reference
{
	case 0x0:
	    if(func==0x21)  //ADDU
            ALUOp = 1;
        else if(func==0x23)  //SUBU
            ALUOp = 2;
        else if(func== 0x24)  //AND
            ALUOp = 3;
        else if(func==0x27){  //NOR
            ALUOp = 4;
			ALUNot = 1;
		}
		else if(func==0x25) // OR
		    ALUOp = 4;
        else if(func==0x0)   //SLL
            ALUOp = 5;
        else if(func==0x2)   //SRL
            ALUOp = 6;
		else if(func==0x2B)  //SLTU
            ALUOp = 7;
        else if(func==0x8){   //JR
            ALUOp = 0;
			CURRENT_STATE.isj=1;
            CURRENT_STATE.JUMP_PC = CURRENT_STATE.REGS[rs];
		}
    break;
	case 0x9: //(0x001001)ADDIU
            ALUOp = 1;
            break;
    case 0xc: //(0x001100)ANDI
            ALUOp = 3;
            break;
    case 0xf:               //(0x001111)LUI
            ALUOp = 8;
            break;
    case 0xd:               //(0x001101)ORI
            ALUOp = 4;
            break;
    case 0xb:               //(0x001011)SLTIU
            ALUOp = 7;
            break;
    case 0x23:              //(0x100011)LW
            ALUOp = 1;
            break;
    case 0x4:               //(0x000100)BEQ
	        ALUOp = 2;
			break;
    case 0x5:               //(0x000101)BNE
            ALUOp = 2;
			ALUNot = 1;
			break;
    case 0x2:               //(0x000010)J
            ALUOp = 0;
			CURRENT_STATE.isj=1;
            CURRENT_STATE.JUMP_PC = (TARGET(instr) << 2);
            break;
    case 0x3:               //(0x000011)JAL
            ALUOp = 100; // Write reg consider 
            CURRENT_STATE.REGS[31] = id_pc + 4;
            CURRENT_STATE.JUMP_PC = (TARGET(instr) << 2);
			CURRENT_STATE.isj=1;
            break;
	default:
	break;
}
    // MOVE FOR NEXT STAGE !!
	CURRENT_STATE.ID_EX_NPC=id_pc; 
	CURRENT_STATE.ID_EX_REG1=read_reg1; 
	CURRENT_STATE.ID_EX_REG2=read_reg2; 
	CURRENT_STATE.ID_EX_IMM=imm; 
	CURRENT_STATE.ID_EX_DEST=dest; 
	CURRENT_STATE.ID_EX_RS=rs; 
	CURRENT_STATE.ID_EX_RT=rt;
	CURRENT_STATE.ID_EX_SHMAT = shmat;

    CURRENT_STATE.id_ALUNot=ALUNot;
	CURRENT_STATE.id_MemRead=MemRead;
	CURRENT_STATE.id_MemtoReg=MemtoReg;
	CURRENT_STATE.id_ALUOp=ALUOp;
	CURRENT_STATE.id_MemWrite=MemWrite;
	CURRENT_STATE.id_ALUSrc=ALUSrc;
	CURRENT_STATE.id_RegWrite=RegWrite;
}

void FLUSH_ID()
{
	CURRENT_STATE.PIPE[ID_STAGE] = 0;

	CURRENT_STATE.ID_EX_NPC=0; 
	CURRENT_STATE.ID_EX_REG1=0; 
	CURRENT_STATE.ID_EX_REG2=0; 
	CURRENT_STATE.ID_EX_IMM=0; 
	CURRENT_STATE.ID_EX_DEST=0; 
	CURRENT_STATE.ID_EX_RS=0; 
	CURRENT_STATE.ID_EX_RT=0;
	CURRENT_STATE.ID_EX_SHMAT = 0;

    CURRENT_STATE.id_ALUNot=0;
	CURRENT_STATE.id_MemRead=0;
	CURRENT_STATE.id_MemtoReg=0;
	CURRENT_STATE.id_ALUOp=0;
	CURRENT_STATE.id_MemWrite=0;
	CURRENT_STATE.id_ALUSrc=0;
	CURRENT_STATE.id_RegWrite=0;
}

void EX_Stage()
{

	if(isjump){ // jump handling
		FLUSH_ID();
		isjump=0;
		}


	if(CURRENT_STATE.isj){ // j type handling
		CURRENT_STATE.isj=0;
		CURRENT_STATE.PC = CURRENT_STATE.JUMP_PC;
		isjump=1;
	}
	
	if(CURRENT_STATE.ex_MemRead) // OCCUR LOAD USE !!
	{
		if(CURRENT_STATE.ID_EX_DEST==CURRENT_STATE.ID_EX_RT) // I - TYPE LOAD USE
		{
			if(CURRENT_STATE.ID_EX_RS==CURRENT_STATE.EX_MEM_DEST){
			CURRENT_STATE.load_use=1;
			}
		}
	else // R-TYPE LOAD USE
	{
		if((CURRENT_STATE.ID_EX_RS==CURRENT_STATE.EX_MEM_DEST)||(CURRENT_STATE.ID_EX_RT==CURRENT_STATE.EX_MEM_DEST)){
		CURRENT_STATE.load_use=1;
		}
	}
	}

	// MOVE FOR NEXT STAGE !!

	CURRENT_STATE.EX_MEM_NPC=CURRENT_STATE.ID_EX_NPC;

	CURRENT_STATE.EX_MEM_DEST = CURRENT_STATE.ID_EX_DEST;

	CURRENT_STATE.ex_MemRead = CURRENT_STATE.id_MemRead;
	CURRENT_STATE.ex_MemWrite = CURRENT_STATE.id_MemWrite;
	CURRENT_STATE.ex_RegWrite = CURRENT_STATE.id_RegWrite;
	CURRENT_STATE.ex_MemtoReg = CURRENT_STATE.id_MemtoReg;

    // EX - UMM YOU NEED TO IMPLEMENT FORWARDING ..
	switch(CURRENT_STATE.id_ALUOp)
	{
		case 1: // +
		if(CURRENT_STATE.id_ALUSrc){
			CURRENT_STATE.EX_MEM_ALU_OUT= CURRENT_STATE.ID_EX_REG1 + CURRENT_STATE.ID_EX_IMM;
		}
		else{
			CURRENT_STATE.EX_MEM_ALU_OUT = CURRENT_STATE.ID_EX_REG1 + CURRENT_STATE.ID_EX_REG2;
		}
		break;

		case 2: // -
		CURRENT_STATE.EX_MEM_ALU_OUT = CURRENT_STATE.ID_EX_REG1 - CURRENT_STATE.ID_EX_REG2;
		if(ALUNot){
			 ~CURRENT_STATE.EX_MEM_ALU_OUT;
		}
		break;

		case 3: // &
		if(CURRENT_STATE.id_ALUSrc){
			CURRENT_STATE.EX_MEM_ALU_OUT= CURRENT_STATE.ID_EX_REG1 & CURRENT_STATE.ID_EX_IMM;
		}
		else{
			CURRENT_STATE.EX_MEM_ALU_OUT = CURRENT_STATE.ID_EX_REG1 & CURRENT_STATE.ID_EX_REG2;
		}
		break;

        case 4: // |
		if(CURRENT_STATE.id_ALUSrc){
			CURRENT_STATE.EX_MEM_ALU_OUT= CURRENT_STATE.ID_EX_REG1 | CURRENT_STATE.ID_EX_IMM;
		}
		else{
			CURRENT_STATE.EX_MEM_ALU_OUT = CURRENT_STATE.ID_EX_REG1 | CURRENT_STATE.ID_EX_REG2;
		}
		if(ALUNot){
           ~CURRENT_STATE.EX_MEM_ALU_OUT;
		}
		break;

		case 5: // <<
		CURRENT_STATE.EX_MEM_ALU_OUT=CURRENT_STATE.ID_EX_REG2 << CURRENT_STATE.ID_EX_SHMAT;
		break;

		case 6: // >>
		CURRENT_STATE.EX_MEM_ALU_OUT=CURRENT_STATE.ID_EX_REG2 >> CURRENT_STATE.ID_EX_SHMAT;
		break;

		case 7: // <
        if(CURRENT_STATE.id_ALUSrc){
			if(CURRENT_STATE.ID_EX_REG1 < CURRENT_STATE.ID_EX_IMM){
				CURRENT_STATE.EX_MEM_ALU_OUT = 1;
			}
			else{
				CURRENT_STATE.EX_MEM_ALU_OUT=0;
			}
		}
		else{
			if(CURRENT_STATE.ID_EX_REG1 < CURRENT_STATE.ID_EX_REG2){
				CURRENT_STATE.EX_MEM_ALU_OUT = 1;
			}
			else{
				CURRENT_STATE.EX_MEM_ALU_OUT=0;
			}
		}
		break;

		case 8: // lui
		CURRENT_STATE.EX_MEM_ALU_OUT=CURRENT_STATE.ID_EX_IMM << 16;
		break;

		case 100:
		break;

		default:
		break;
	}
	if(CURRENT_STATE.id_MemWrite){ 
		CURRENT_STATE.EX_MEM_W_VALUE = CURRENT_STATE.ID_EX_REG2;
	} // SW HANDLE

	    // branch - control hazard !
	     // beq
		if(CURRENT_STATE.id_Branch_beq){
			if(!CURRENT_STATE.EX_MEM_ALU_OUT){
			CURRENT_STATE.EX_MEM_BR_TARGET = CURRENT_STATE.ID_EX_NPC+((CURRENT_STATE.ID_EX_IMM) << 2);
			CURRENT_STATE.EX_MEM_BR_TAKE = 1;
			CURRENT_STATE.id_Branch_beq=0;
			}
			else{
				CURRENT_STATE.EX_MEM_BR_TAKE = 0;
				CURRENT_STATE.id_Branch_beq=0;
			}
		} 
		// bne
		if(CURRENT_STATE.id_Branch_bne){
			if(CURRENT_STATE.EX_MEM_ALU_OUT){
			CURRENT_STATE.EX_MEM_BR_TARGET = CURRENT_STATE.ID_EX_NPC+((CURRENT_STATE.ID_EX_IMM) << 2);
			CURRENT_STATE.EX_MEM_BR_TAKE = 1;
			CURRENT_STATE.id_Branch_bne=0;
			}
			else{
				CURRENT_STATE.EX_MEM_BR_TAKE=0;
				CURRENT_STATE.id_Branch_bne=0;
			}
		}
}

void FLUSH_EX()
{
	CURRENT_STATE.PIPE[EX_STAGE]=0;

	CURRENT_STATE.EX_MEM_NPC=0;
	CURRENT_STATE.EX_MEM_ALU_OUT=0;
	CURRENT_STATE.EX_MEM_W_VALUE=0;
	CURRENT_STATE.EX_MEM_BR_TARGET=0;
	CURRENT_STATE.EX_MEM_BR_TAKE=0;
	CURRENT_STATE.EX_MEM_DEST=0;

	CURRENT_STATE.ex_MemRead=0;
	CURRENT_STATE.ex_MemWrite=0;
	CURRENT_STATE.ex_RegWrite=0;
	CURRENT_STATE.ex_MemtoReg=0;
}

void MEM_Stage()
{
	/*if(isbranch){
		FLUSH_EX();
		FLUSH_ID();
		FLUSH_IF();
		isbranch=0;
	}*/
	// branch occur ! - occur control hazard !!
	if(CURRENT_STATE.EX_MEM_BR_TAKE){
		CURRENT_STATE.EX_MEM_BR_TAKE = 0;
		CURRENT_STATE.PC = CURRENT_STATE.EX_MEM_BR_TARGET;
		FLUSH_EX();
		FLUSH_ID();
		FLUSH_IF();
	}

	// mem implement
	if(CURRENT_STATE.ex_MemWrite){
		mem_write_32(CURRENT_STATE.EX_MEM_ALU_OUT,CURRENT_STATE.EX_MEM_W_VALUE);
	}
	else if(CURRENT_STATE.ex_MemRead){
		CURRENT_STATE.MEM_WB_MEM_OUT=mem_read_32(CURRENT_STATE.EX_MEM_ALU_OUT);
	}

	// MOVE FOR NEXT STAGE !!

	CURRENT_STATE.MEM_WB_DEST=CURRENT_STATE.EX_MEM_DEST;
	CURRENT_STATE.mem_MemtoReg=CURRENT_STATE.ex_MemtoReg;
	CURRENT_STATE.mem_RegWrite=CURRENT_STATE.ex_RegWrite;
	CURRENT_STATE.MEM_WB_ALU_OUT=CURRENT_STATE.EX_MEM_ALU_OUT;
}
/*
void FLUSH_MEM()
{
	CURRENT_STATE.MEM_WB_NPC=0; 
	CURRENT_STATE.MEM_WB_ALU_OUT=0; 
	CURRENT_STATE.MEM_WB_MEM_OUT=0; 
	CURRENT_STATE.MEM_WB_BR_TAKE=0; 
	CURRENT_STATE.MEM_WB_DEST=0;

	CURRENT_STATE.mem_RegWrite=0;
	CURRENT_STATE.mem_MemtoReg=0;
}
*/
void WB_Stage()
{
	if(CURRENT_STATE.mem_RegWrite){
		CURRENT_STATE.REGS[CURRENT_STATE.MEM_WB_DEST] = CURRENT_STATE.MEM_WB_ALU_OUT;
	}
	else if(CURRENT_STATE.mem_MemtoReg){
		CURRENT_STATE.REGS[CURRENT_STATE.MEM_WB_DEST] = CURRENT_STATE.MEM_WB_MEM_OUT;
	}
}


/***************************************************************/
/*                                                             */
/*   MIPS-32 Instruction Level Simulator                       */
/*                                                             */
/*   SCE212 Ajou University                                    */
/*   run.c                                                     */
/*   Adapted from Computer Architecture@KAIST                  */
/*                                                             */
/***************************************************************/

#include <stdio.h>

#include "util.h"
#include "run.h"
#include "parse.h"

instruction inst;
instruction* instr;
uint32_t ex_pc;
uint32_t id_pc;
uint32_t mem_pc;
uint32_t wb_pc;
int isjump;
int isbranch;
unsigned char rs,rt,rd,imm,shmat;

int count = 0;

    /* for control signal */
	char MemRead;
	char MemtoReg;
	char ALUOp;
	char MemWrite;
	char ALUSrc;
	char RegWrite;
	char ALUNot;

	uint32_t read_reg1;
	uint32_t read_reg2;
	
	unsigned char dest;
	
/***************************************************************/
/*                                                             */
/* Procedure: get_inst_info                                    */
/*                                                             */
/* Purpose: Read insturction information                       */
/*                                                             */
/***************************************************************/
instruction* get_inst_info(uint32_t pc) {
    return &INST_INFO[(pc - MEM_TEXT_START) >> 2];
}

/***************************************************************/
/*                                                             */
/* Procedure: process_instruction                              */
/*                                                             */
/* Purpose: Process one instrction                             */
/*                                                             */
/***************************************************************/

void process_instruction() 
{
   
   
    for (int i = PIPE_STAGE - 1; i > 0; i--)
        CURRENT_STATE.PIPE[i] = CURRENT_STATE.PIPE[i-1];
    CURRENT_STATE.PIPE[IF_STAGE] = CURRENT_STATE.PC;
    
	instr = get_inst_info(CURRENT_STATE.PC);

	CURRENT_STATE.PC += 4;

    WB_Stage();

	MEM_Stage();
	
    if (!CURRENT_STATE.load_use) {
        CURRENT_STATE.load_use = 0;
        EX_Stage();

		ID_Stage();

        IF_Stage();
    }

	INSTRUCTION_COUNT++;
}

void IF_Stage()
{
	if(CURRENT_STATE.PC > MEM_TEXT_START + 4 * (NUM_INST))
	{
		//FLUSH_EX();
		//FLUSH_ID();
		FETCH_BIT=FALSE;
		++count;
		CURRENT_STATE.PC -=4;
		FLUSH_IF();
		
		CURRENT_STATE.REGS[16]=0;
		if(count==4) RUN_BIT = FALSE;
	} 
    if(FETCH_BIT){

    // inst=INST_INFO[(CURRENT_STATE.PC - MEM_TEXT_START)>>2]; // FETCH
    // MOVE FOR NEXT STAGE !!

    inst=INST_INFO[(CURRENT_STATE.PC - MEM_TEXT_START)>>2]; // FETCH

	CURRENT_STATE.IF_ID_INST=inst; // store fetched instruction

	CURRENT_STATE.IF_ID_NPC=CURRENT_STATE.PC+4; // store NPC
    }
}

void FLUSH_IF()
{
	CURRENT_STATE.PIPE[IF_STAGE] = 0;
	memset(&(CURRENT_STATE.IF_ID_INST),0,sizeof(instruction));
    CURRENT_STATE.IF_ID_NPC=0;
}

void ID_Stage()
{
		
	/* for decode */
	short opcode;
	short func;

	id_pc=CURRENT_STATE.IF_ID_NPC;

	//instr = &(CURRENT_STATE.IF_ID_INST); // Load inst data


	opcode = OPCODE(instr);
	func = FUNC(instr);

    /* opcode information

        case 0x0: 
	    case 0x9:		// ADDIU 
		case 0xc:		// ANDI 
		case 0xf:		// LUI	
		case 0xd:		// ORI
		case 0xb:		// SLTIU
		case 0x23:		// LW
		case 0x2b:		// SW
		case 0x4:		// BEQ
		case 0x5:		// BNE
		case 0x2:		// J
		case 0x3:		// JAL
	
	*/

// read reg value
read_reg1=CURRENT_STATE.REGS[RS(instr)];
read_reg2=CURRENT_STATE.REGS[RT(instr)];

// for store id_ex register
rs=RS(instr);
rt=RT(instr);
rd=RD(instr);
imm=IMM(instr);
shmat = SHAMT(instr);

// mux for dest reg number
if(opcode==0x0 || opcode == 0x3) dest = RD(instr);
else dest = RT(instr);

// generate control signal

// ALU INPUT MUX IMM vs RT value
switch(opcode)
{
	case 0x9:		// ADDIU 
	case 0xc:		// ANDI 
	case 0xf:		// LUI	
	case 0xd:		// ORI
	case 0xb:		// SLTIU
	case 0x23:		// LW
	case 0x2b:		// SW
	case 0x4:		// BEQ
	case 0x5:		// BNE
	ALUSrc=1; // i - type
	break;
	case 0x0:
	ALUSrc=0; // r-type
	break;
	default:
	break;
}
// Reg Write - j, sw, beq, bne, jr - don't write reg insts
if(opcode == 0x2 || opcode == 0x2b || opcode == 0x4 || opcode==0x5 || (opcode == 0x0 && func == 0x8)) RegWrite=0;
else RegWrite=1;
// Branch
if(opcode == 0x4) CURRENT_STATE.id_Branch_beq = 1;
if(opcode == 0x5) CURRENT_STATE.id_Branch_bne = 1;
// Mem Read and Mem to reg - lw
if(opcode == 0x23) {
	MemRead = 1;
	MemtoReg = 1;
}
// Mem Write - sw
if(opcode == 0x2b) MemWrite=1;
// ALUOp and handle j, jr, jal
switch(opcode) // lecture note mips processor - control ALU reference
{
	case 0x0:
	    if(func==0x21)  //ADDU
            ALUOp = 1;
        else if(func==0x23)  //SUBU
            ALUOp = 2;
        else if(func== 0x24)  //AND
            ALUOp = 3;
        else if(func==0x27){  //NOR
            ALUOp = 4;
			ALUNot = 1;
		}
		else if(func==0x25) // OR
		    ALUOp = 4;
        else if(func==0x0)   //SLL
            ALUOp = 5;
        else if(func==0x2)   //SRL
            ALUOp = 6;
		else if(func==0x2B)  //SLTU
            ALUOp = 7;
        else if(func==0x8){   //JR
            ALUOp = 0;
			CURRENT_STATE.isj=1;
            CURRENT_STATE.JUMP_PC = CURRENT_STATE.REGS[rs];
		}
    break;
	case 0x9: //(0x001001)ADDIU
            ALUOp = 1;
            break;
    case 0xc: //(0x001100)ANDI
            ALUOp = 3;
            break;
    case 0xf:               //(0x001111)LUI
            ALUOp = 8;
            break;
    case 0xd:               //(0x001101)ORI
            ALUOp = 4;
            break;
    case 0xb:               //(0x001011)SLTIU
            ALUOp = 7;
            break;
    case 0x23:              //(0x100011)LW
            ALUOp = 1;
            break;
    case 0x4:               //(0x000100)BEQ
	        ALUOp = 2;
			break;
    case 0x5:               //(0x000101)BNE
            ALUOp = 2;
			ALUNot = 1;
			break;
    case 0x2:               //(0x000010)J
            ALUOp = 0;
			CURRENT_STATE.isj=1;
            CURRENT_STATE.JUMP_PC = (TARGET(instr) << 2);
            break;
    case 0x3:               //(0x000011)JAL
            ALUOp = 100; // Write reg consider 
            CURRENT_STATE.REGS[31] = id_pc + 4;
            CURRENT_STATE.JUMP_PC = (TARGET(instr) << 2);
			CURRENT_STATE.isj=1;
            break;
	default:
	break;
}
    // MOVE FOR NEXT STAGE !!
	CURRENT_STATE.ID_EX_NPC=id_pc; 
	CURRENT_STATE.ID_EX_REG1=read_reg1; 
	CURRENT_STATE.ID_EX_REG2=read_reg2; 
	CURRENT_STATE.ID_EX_IMM=imm; 
	CURRENT_STATE.ID_EX_DEST=dest; 
	CURRENT_STATE.ID_EX_RS=rs; 
	CURRENT_STATE.ID_EX_RT=rt;
	CURRENT_STATE.ID_EX_SHMAT = shmat;

    CURRENT_STATE.id_ALUNot=ALUNot;
	CURRENT_STATE.id_MemRead=MemRead;
	CURRENT_STATE.id_MemtoReg=MemtoReg;
	CURRENT_STATE.id_ALUOp=ALUOp;
	CURRENT_STATE.id_MemWrite=MemWrite;
	CURRENT_STATE.id_ALUSrc=ALUSrc;
	CURRENT_STATE.id_RegWrite=RegWrite;
}

void FLUSH_ID()
{
	CURRENT_STATE.PIPE[ID_STAGE] = 0;

	CURRENT_STATE.ID_EX_NPC=0; 
	CURRENT_STATE.ID_EX_REG1=0; 
	CURRENT_STATE.ID_EX_REG2=0; 
	CURRENT_STATE.ID_EX_IMM=0; 
	CURRENT_STATE.ID_EX_DEST=0; 
	CURRENT_STATE.ID_EX_RS=0; 
	CURRENT_STATE.ID_EX_RT=0;
	CURRENT_STATE.ID_EX_SHMAT = 0;

    CURRENT_STATE.id_ALUNot=0;
	CURRENT_STATE.id_MemRead=0;
	CURRENT_STATE.id_MemtoReg=0;
	CURRENT_STATE.id_ALUOp=0;
	CURRENT_STATE.id_MemWrite=0;
	CURRENT_STATE.id_ALUSrc=0;
	CURRENT_STATE.id_RegWrite=0;
}

void EX_Stage()
{
	int forward_rs=00;
	int forward_rt=00;

	uint32_t input1;
	uint32_t input2;
	uint32_t output;

	if(isjump){ // jump handling
		FLUSH_ID();
		isjump=0;
		}


	if(CURRENT_STATE.isj){ // j type handling
		CURRENT_STATE.isj=0;
		CURRENT_STATE.PC = CURRENT_STATE.JUMP_PC;
		isjump=1;
	}
	
	if(CURRENT_STATE.ex_MemRead) // OCCUR LOAD USE !!
	{
		if(CURRENT_STATE.ID_EX_DEST==CURRENT_STATE.ID_EX_RT) // I - TYPE LOAD USE
		{
			if(CURRENT_STATE.ID_EX_RS==CURRENT_STATE.EX_MEM_DEST){
			CURRENT_STATE.load_use=1;
			}
		}
	else // R-TYPE LOAD USE
	{
		if((CURRENT_STATE.ID_EX_RS==CURRENT_STATE.EX_MEM_DEST)||(CURRENT_STATE.ID_EX_RT==CURRENT_STATE.EX_MEM_DEST)){
		CURRENT_STATE.load_use=1;
		}
	}
	}

	// MOVE FOR NEXT STAGE !!

	CURRENT_STATE.EX_MEM_NPC=CURRENT_STATE.ID_EX_NPC;

	CURRENT_STATE.EX_MEM_DEST = CURRENT_STATE.ID_EX_DEST;

	CURRENT_STATE.ex_MemRead = CURRENT_STATE.id_MemRead;
	CURRENT_STATE.ex_MemWrite = CURRENT_STATE.id_MemWrite;
	CURRENT_STATE.ex_RegWrite = CURRENT_STATE.id_RegWrite;
	CURRENT_STATE.ex_MemtoReg = CURRENT_STATE.id_MemtoReg;

	/* Dependency detect unit */

	// Bypassing path 1 input->CURRENT_STATE.EX_MEM_ALU_OUT
	// rs num same, rt num same, rs && rt num same -> you consider three case
	if(CURRENT_STATE.ex_RegWrite){
		if(CURRENT_STATE.EX_MEM_DEST != 0){
			if(CURRENT_STATE.EX_MEM_DEST == CURRENT_STATE.ID_EX_RS){
				forward_rs = 01;
			}
			if(CURRENT_STATE.EX_MEM_DEST == CURRENT_STATE.ID_EX_RT){
				forward_rt = 01;
			}
		}
	}

	// Bypassig path 2 input ->CURRNET_STATE.MEM_WB_MEM_OUT
	// LW - INST 1 - INST 2(LW OUTPUT NEED) && INST 1 and INST 2 may be have dependency
	// Load use must not occur forwarding condition !! you must consider that
	if(CURRENT_STATE.mem_MemtoReg){ // lw - wb stage
		if(CURRENT_STATE.MEM_WB_DEST !=0){ // INST 1 - mem - stage
			if((CURRENT_STATE.EX_MEM_DEST !=0) && (CURRENT_STATE.EX_MEM_DEST == CURRENT_STATE.ID_EX_RS) && !(CURRENT_STATE.ex_RegWrite))
			{
				if(CURRENT_STATE.MEM_WB_DEST == CURRENT_STATE.ID_EX_RS){ // lw - INST 2 dependency with rs
					forward_rs = 10;
				}
			}
			//
			if((CURRENT_STATE.EX_MEM_DEST !=0) && (CURRENT_STATE.EX_MEM_DEST == CURRENT_STATE.ID_EX_RT) && !(CURRENT_STATE.ex_RegWrite))
			{
				if(CURRENT_STATE.MEM_WB_DEST == CURRENT_STATE.ID_EX_RS){ // lw - INST 2 dependency with rt
					forward_rt = 10;
				}
			}
		}

	}

	/* end of Denpendency detext unit */
	// ALU input change //
    // input1
	if(forward_rs == 01){ // ALU_OUT -> INPUT2
		input1 = CURRENT_STATE.EX_MEM_ALU_OUT;
	}
	else if(forward_rs == 10){ // MEM_OUT -> INPUT2
		input1 = CURRENT_STATE.MEM_WB_MEM_OUT;
	}
	else{ // IMM -> INPUT2
		input1 = CURRENT_STATE.ID_EX_REG1;
	}
    // input2
	if(forward_rt == 01){
		input2 = CURRENT_STATE.EX_MEM_ALU_OUT;
	}
	else if(forward_rt == 10){
		input2 = CURRENT_STATE.MEM_WB_MEM_OUT;
	}
	else{
        input2 = CURRENT_STATE.ID_EX_REG2;
	}
   
    // for i -type instruction
	if(CURRENT_STATE.id_ALUSrc) input2 = CURRENT_STATE.ID_EX_IMM;

	// end of ALU input change // 

    /* start ALU operation */
	switch(CURRENT_STATE.id_ALUOp)
	{
		case 1: // +
		output = input1 + input2;
		break;

		case 2: // -
		output = input1 - input2;
		if(ALUNot){ // not
			 ~output;
		}
		break;

		case 3: // &
		output = input1 & input2;
		break;

        case 4: // |
		output = input1 | input2;
		if(ALUNot){
           ~output;
		}
		break;

		case 5: // <<
		output = input2 << CURRENT_STATE.ID_EX_SHMAT;
		break;

		case 6: // >>
		output = input2 >> CURRENT_STATE.ID_EX_SHMAT;
		break;

		case 7: // <
		if(input1 < input2){
			output = 1;
		}
		else{
			output=0;
		}
		break;

		case 8: // lui
		CURRENT_STATE.EX_MEM_ALU_OUT=CURRENT_STATE.ID_EX_IMM << 16;
		break;

		case 100:
		break;

		default:
		break;
	}
	/* end of the ALU operation */

	if(CURRENT_STATE.id_MemWrite){ 
		CURRENT_STATE.EX_MEM_W_VALUE = CURRENT_STATE.ID_EX_REG2;
	} // SW HANDLE

	// branch - control hazard !
	// beq
	if(CURRENT_STATE.id_Branch_beq){
		if(!CURRENT_STATE.EX_MEM_ALU_OUT){
			CURRENT_STATE.EX_MEM_BR_TARGET = CURRENT_STATE.ID_EX_NPC+((CURRENT_STATE.ID_EX_IMM) << 2);
			CURRENT_STATE.EX_MEM_BR_TAKE = 1;
			CURRENT_STATE.id_Branch_beq=0;
			}
		else{
			CURRENT_STATE.EX_MEM_BR_TAKE = 0;
				CURRENT_STATE.id_Branch_beq=0;
			}
	} 
	// bne
	if(CURRENT_STATE.id_Branch_bne){
		if(CURRENT_STATE.EX_MEM_ALU_OUT){
			CURRENT_STATE.EX_MEM_BR_TARGET = CURRENT_STATE.ID_EX_NPC+((CURRENT_STATE.ID_EX_IMM) << 2);
			CURRENT_STATE.EX_MEM_BR_TAKE = 1;
			CURRENT_STATE.id_Branch_bne=0;
			}
		else{
			CURRENT_STATE.EX_MEM_BR_TAKE=0;
			CURRENT_STATE.id_Branch_bne=0;
			}
	}
}

void FLUSH_EX()
{
	CURRENT_STATE.PIPE[EX_STAGE]=0;

	CURRENT_STATE.EX_MEM_NPC=0;
	CURRENT_STATE.EX_MEM_ALU_OUT=0;
	CURRENT_STATE.EX_MEM_W_VALUE=0;
	CURRENT_STATE.EX_MEM_BR_TARGET=0;
	CURRENT_STATE.EX_MEM_BR_TAKE=0;
	CURRENT_STATE.EX_MEM_DEST=0;

	CURRENT_STATE.ex_MemRead=0;
	CURRENT_STATE.ex_MemWrite=0;
	CURRENT_STATE.ex_RegWrite=0;
	CURRENT_STATE.ex_MemtoReg=0;
}

void MEM_Stage()
{
	/*if(isbranch){
		FLUSH_EX();
		FLUSH_ID();
		FLUSH_IF();
		isbranch=0;
	}*/
	// branch occur ! - occur control hazard !!
	if(CURRENT_STATE.EX_MEM_BR_TAKE){
		CURRENT_STATE.EX_MEM_BR_TAKE = 0;
		CURRENT_STATE.PC = CURRENT_STATE.EX_MEM_BR_TARGET;
		FLUSH_EX();
		FLUSH_ID();
		FLUSH_IF();
	}

	// mem implement
	if(CURRENT_STATE.ex_MemWrite){
		mem_write_32(CURRENT_STATE.EX_MEM_ALU_OUT,CURRENT_STATE.EX_MEM_W_VALUE);
	}
	else if(CURRENT_STATE.ex_MemRead){
		CURRENT_STATE.MEM_WB_MEM_OUT=mem_read_32(CURRENT_STATE.EX_MEM_ALU_OUT);
	}

	// MOVE FOR NEXT STAGE !!

	CURRENT_STATE.MEM_WB_DEST=CURRENT_STATE.EX_MEM_DEST;
	CURRENT_STATE.mem_MemtoReg=CURRENT_STATE.ex_MemtoReg;
	CURRENT_STATE.mem_RegWrite=CURRENT_STATE.ex_RegWrite;
	CURRENT_STATE.MEM_WB_ALU_OUT=CURRENT_STATE.EX_MEM_ALU_OUT;
}
/*
void FLUSH_MEM()
{
	CURRENT_STATE.MEM_WB_NPC=0; 
	CURRENT_STATE.MEM_WB_ALU_OUT=0; 
	CURRENT_STATE.MEM_WB_MEM_OUT=0; 
	CURRENT_STATE.MEM_WB_BR_TAKE=0; 
	CURRENT_STATE.MEM_WB_DEST=0;

	CURRENT_STATE.mem_RegWrite=0;
	CURRENT_STATE.mem_MemtoReg=0;
}
*/
void WB_Stage()
{
	if(CURRENT_STATE.mem_RegWrite){
		CURRENT_STATE.REGS[CURRENT_STATE.MEM_WB_DEST] = CURRENT_STATE.MEM_WB_ALU_OUT;
	}
	else if(CURRENT_STATE.mem_MemtoReg){
		CURRENT_STATE.REGS[CURRENT_STATE.MEM_WB_DEST] = CURRENT_STATE.MEM_WB_MEM_OUT;
	}
}
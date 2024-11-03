#include <cstdint>
#include <iostream>
#include <bitset>
#include <fstream>

// https://www.nesdev.org/wiki/CPU
// https://www.nesdev.org/obelisk-6502-guide/

using Byte = uint8_t; // 0 - 255, 8 bits
using Word = uint16_t; // 0 - 65535, 16 bits

static constexpr Word RESET_VECTOR = 0xFFFC;
static constexpr Word STACK_BASE = 0x0100;

struct MEM {

	/*
	Some parts of the 2 KiB of internal RAM at $0000–$07FF have predefined purposes dictated by the 6502 architecture:

    $0000-$00FF: The zero page, which can be accessed with fewer bytes and cycles than other addresses
    $0100–$01FF: The page containing the stack, which can be located anywhere here, but typically starts at $01FF
	*/

	MEM() {
		for (unsigned int i = 0; i < MAX_MEMORY; i++)
		{
			Data[i] = 0;
		}
	}


	static constexpr unsigned int  MAX_MEMORY = 0x10000; // 65,536 bytes

	Byte Data[MAX_MEMORY]; // Our ram

};

struct CPU {

	MEM* mem;

	// Registers
	Byte A = 0x0; // Accumulator
	Byte X = 0x0; // Index
	Byte Y = 0x0; // Index

	Word Pc = 0x0; // Program counter
	Byte Sp = 0x0; // stack pointer

	bool halted = false;
	int cyclesRemaining;
	int currentCycle;
	Byte opCode;
	Byte prevOpCode; // for debug
	int loops = 0; // for debug
	Byte operand = 0;
	Byte lowByte = 0;
	Byte HighByte = 0;
	Word effectiveAdress = 0;
	Byte offset = 0x0;
	Byte oldPc = 0x0;
	enum State {FETCH, EXECUTE};
	State state = FETCH;

	// Status flags
	struct {

		Byte C : 1; // Carry
		Byte Z : 1; // Zero
		Byte I : 1; // Interrupt Disable
		Byte D : 1; // Decimal Mode
		Byte B : 1; // Break Command
		Byte U : 1; // Unused (always set to 1)
		Byte V : 1; // Overflow
		Byte N : 1; // Negative
	} status;


	void reset() {

		Word lowByte = MR(RESET_VECTOR);
		Word highByte = MR(RESET_VECTOR + 1);
		Pc = (highByte << 8) | lowByte;
		Sp = 0xFD;
	}

	void start_up(MEM* memory) {

		mem = memory;

		Sp = 0xFF; // stack start 0x0100, stack end 0x01FF
		// Set the Registers to 0
		A = 0;
		X = 0;
		Y = 0;

		
		MW(RESET_VECTOR, 0x00); // set reset vector value to 0x00
		MW(RESET_VECTOR + 1, 0x06);

	}

	Byte fetch(Word& pc) {
		Byte opCode = MR(pc);
		Pc += 1;
		return opCode;
	}


	void tick() {
		switch (state) {
		case FETCH:
			opCode = MR(Pc);

			// for debugging
			if (prevOpCode == opCode) {
				loops++;
			}
			prevOpCode = opCode;
			// for debugging
			if (loops > 200) {
				opCode = 0x00; // break instruction
			}
			cyclesRemaining = getInstructionCycles(opCode);
			if (cyclesRemaining != 0) {
				std::cout << "\033[32m0x" << std::hex << opCode + 0x0 << " IMPLEMENTED" << std::endl;
			}
			else {
				std::cout << "\033[31m0x" << std::hex << opCode + 0x0 << " NOT IMPLEMENTED" << std::endl;
			}
			currentCycle = 1;
			Pc++;
			state = EXECUTE;
			break;

		case EXECUTE:
			executeInstructionCycle();
			cyclesRemaining--;
			if (cyclesRemaining <= 0) {
				state = FETCH;
			}
			break;
		}
	}

	void push(Byte value) {
		MW(0x0100 + Sp, value);
		Sp--; // Decrement Sp after storing
		Sp &= 0xFF; // Ensure Sp stays within 0x00 - 0xFF
	}

	Byte pull() {
		Sp++; // Increment Sp before reading
		Sp &= 0xFF; // Ensure Sp stays within 0x00 - 0xFF
		return MR(0x0100 + Sp);
	}

	bool isHalted() const {
		return halted;
	}





	void setCarry(int result) {

		if (0b10000000 & result) { // C flag
			status.C = 1;
		}
		else {
			status.C = 0;
		}

	}

	void setZero(int result) {
		status.Z = (result == 0) ? 1 : 0;
	}

	void setInterrupt() {

	}

	void setDecimal() {

	}

	void setBreak() {

	}

	void calcEffectiveAdress() {
		effectiveAdress = lowByte;
		effectiveAdress |= (HighByte << 8);
	}

	void setOverflow(int result) {

		if ((0b10000000 & result) ^ (0b01000000) & result) { // overflow flag
			status.V = 1;
		}
		else {
			status.V = 0;
		}

	}
	
	void setNegative(int result) {
		status.N = (result & 0x80) ? 1 : 0;
	}
	static constexpr Byte
		// ADC
		INS_ADC_IMMEDIATE = 0x69,
		// AND
		INS_AND_IMMEDIATE = 0x29,
		// BNE
		INS_BNE_RELATIVE = 0xD0,
		// BRK
		INS_BRK_IMPLIED = 0x00,
		// CLD
		INS_CLD_IMPLIED = 0xD8,
		// CPY
		INS_CPY_ABSOLUTE = 0xCC,
		// DEX
		INS_DEX_IMPLIED = 0xCA,
		// DEY
		INS_DEY_IMPLIED = 0x88,
		// JMP
		INS_JMP_ABSOLUTE = 0x4C,
		// LDA
		INS_LDA_IMMEDIATE = 0xA9,
		// LDX
		INS_LDX_IMMEDIATE = 0xA2,
		// LDY
		INS_LDY_IMMEDIATE = 0xA0,
		// NOP
		INS_NOP_IMPLIED = 0xEA,
		// STA
		INS_STA_ABSOLUTE = 0x8D,
		// TXS
		INS_TXS_IMPLIED = 0x9A;


	int getInstructionCycles(Byte opcode) {
		switch (opcode) {
		case INS_ADC_IMMEDIATE:
			return 2;
		case INS_AND_IMMEDIATE:
			return 2;
		case INS_BNE_RELATIVE:
			return 2;
		case INS_BRK_IMPLIED:
			return 7;
		case INS_CLD_IMPLIED:
			return 2;
		case INS_CPY_ABSOLUTE:
			return 4;
		case INS_DEX_IMPLIED:
			return 2;
		case INS_DEY_IMPLIED:
			return 2;
		case INS_JMP_ABSOLUTE:
			return 3;
		case INS_LDA_IMMEDIATE:
			return 2;
		case INS_LDX_IMMEDIATE:
			return 2;
		case INS_LDY_IMMEDIATE:
			return 2;
		case INS_NOP_IMPLIED:
			return 2;
		case INS_STA_ABSOLUTE:
			return 4;
		case INS_TXS_IMPLIED:
			return 2;
		default:
			return 0; // Unknown opcode
		}
	}



	void executeInstructionCycle() {


		switch (opCode)
		{
			// ADC
			case INS_ADC_IMMEDIATE:{
				switch (currentCycle) {
				case 1:
					operand = MR(Pc);
					Pc++;
					break;
				case 2:
					A += operand;
					setCarry(A);
					setZero(A);
					// TODO overflow flag
					setNegative(A);
					break;

				}
				break;
			}
			// AND
			case INS_AND_IMMEDIATE: {
				switch (currentCycle) {
				case 1:
					operand = MR(Pc);
					Pc++;
					break;
				case 2:
					A = A & operand;
					setZero(A);
					// TODO overflow flag
					setNegative(A);
					break;

				}
				break;
			}
			// BNE
			case INS_BNE_RELATIVE: {
				switch (currentCycle) {
				case 1:
					offset = MR(Pc);
					Pc++;
					break;
				case 2:
					if (status.Z == 0) {
						oldPc = Pc;
						Pc = Pc + offset;
						cyclesRemaining++;
					}
					break;
				case 3:
					if ((oldPc & 0xFF00) != (Pc & 0xFF00)) { // check if we cross a page boundary by isolating the high bytes and comparing
						cyclesRemaining++;
					}
					break;
				case 4:
					break;

				}

				break;
			}
			// BRK
			case INS_BRK_IMPLIED: {
				switch (currentCycle) {
				case 1:
					Pc++;
					break;
				case 2:
					push((Pc >> 8) & 0xFF);
					break;
				case 3:
					push(Pc & 0xFF);
					break;
				case 4:
					push(status.B);
					break;
				case 5:
					status.B = 1;
					break;
				case 6:
					Pc = MR(0xFFFE); // Low byte
					break;
				case 7:
					Pc |= (MR(0xFFFF) << 8); // High byte
					break;
				}
				break;
			}
			// CLD
			case INS_CLD_IMPLIED:{
				switch (currentCycle) {
				case 1:
					break;
				case 2:
					status.D = 0;
					break;
				}
				break;
		}
			// CPY
			case INS_CPY_ABSOLUTE:{
				switch (currentCycle) {
				case 1:
					lowByte = MR(Pc);
					Pc++;
					break;
				case 2:
					HighByte = MR(Pc);
					Pc++;
					break;
				case 3:
					effectiveAdress = lowByte;
					effectiveAdress |= (HighByte << 8);
					break;
				case 4:
					operand = MR(effectiveAdress);
					status.C = (Y >= operand) ? 1 : 0;

					setZero(Y - operand);
					setNegative(Y - operand);
					break;
				}
				break;
			}
			// DEX
			case INS_DEX_IMPLIED: {
				switch (currentCycle) {
				case 1:
					break;
				case 2:
					X--;
					setZero(X);
					setNegative(X);
					break;

				}
				break;
			}
			// DEY
			case INS_DEY_IMPLIED: {
				switch (currentCycle) {
				case 1:
					break;
				case 2:
					Y--;
					setZero(Y);
					setNegative(Y);
					break;

				}
				break;
		
			}
			// JMP
			case INS_JMP_ABSOLUTE: {
				switch (currentCycle) {
				case 1:
					lowByte = MR(Pc);
					Pc++;
					break;
				case 2:
					HighByte = MR(Pc);
					Pc++;
					break;
				case 3:
					calcEffectiveAdress();
					Pc = effectiveAdress;
					break;

				}
				break;
			}
			// LDA
			case INS_LDA_IMMEDIATE:{
				switch (currentCycle) {
				case 1:
					operand = MR(Pc);
					Pc++;
					break;
				case 2:
					A = operand;
					setZero(X);
					setNegative(X);
					break;

				}
				break;
			}
			// LDX
			case INS_LDX_IMMEDIATE:{
				switch (currentCycle) {
				case 1:
					operand = MR(Pc);
					Pc++;
					break;
				case 2:
					X = operand;
					setZero(X);
					setNegative(X);
					break;
				}
				break;
			}
			// LDY
			case INS_LDY_IMMEDIATE: {
				switch (currentCycle) {
				case 1:
					operand = MR(Pc);
					Pc++;
					break;
				case 2:
					Y = operand;
					setZero(Y);
					setNegative(Y);
					break;
				}
				break;
			}
			// NOP
			case INS_NOP_IMPLIED: {
				switch (currentCycle) {
				case 1:
					break;
				case 2:
					break;
				}
				break;
			}
			// STA
			case INS_STA_ABSOLUTE: {
				switch (currentCycle) {
				case 1:
					lowByte = MR(Pc);
					Pc++;
					break;
				case 2:
					HighByte = MR(Pc);
					Pc++;
					break;
				case 3:
					calcEffectiveAdress();
					break;
				case 4:
					MW(effectiveAdress, A);
					break;
				}
				break;
			}
			// TXS
			case INS_TXS_IMPLIED: {
				switch (currentCycle) {
				case 1:
					break;
				case 2:
					Sp = X;
					break;
				}
				break;
			}



		}

		currentCycle++;

	}

	Byte MR(Word adr) { // reads memory
		return mem->Data[adr];
	}

	void  MW(Word adr, Byte value) { // memory write
		mem->Data[adr] = value;
	}
};



static struct DEBUGLOG {

	MEM* mem;
	CPU* cpu;

	void initDebug(MEM* mem, CPU* cpu) {

		this->mem = mem;
		this->cpu = cpu;
	}

	void printA() {
		std::cout << "Register A: " << std::bitset<sizeof(Byte) * 8>(cpu->A) << std::endl;
		std::cout << "Register A: " << "0x" << std::hex << cpu->A + 0x0 << std::endl;
		std::cout << "Register A: " << std::dec << static_cast<unsigned>(cpu->A) << std::endl;
		std::cout << std::endl;
	}

	void printX() {
		std::cout << "Register X: " << std::bitset<sizeof(Byte) * 8>(cpu->X) << std::endl;
		std::cout << "Register X: " << "0x" << std::hex << cpu->X + 0x0 << std::endl;
		std::cout << "Register X: " << std::dec << static_cast<unsigned>(cpu->X) << std::endl;
		std::cout << std::endl;
	}

	void printY() {
		std::cout << "Register Y: " << std::bitset<sizeof(Byte) * 8>(cpu->Y) << std::endl;
		std::cout << "Register Y: " << "0x" << std::hex << cpu->Y + 0x0 << std::endl;
		std::cout << "Register Y: " << std::dec << static_cast<unsigned>(cpu->Y) << std::endl;
		std::cout << std::endl;
	}

	void printMem(Byte adress) {
		Byte content = cpu->MR(adress);

		std::cout << "Memory at: " << std::bitset<sizeof(Byte) * 8>(content) << std::endl;
		std::cout << "Memory at: " << "0x" << std::hex << content + 0x0 << std::endl;
		std::cout << "Memory at: " << std::dec << static_cast<unsigned>(content) << std::endl;
		std::cout << std::endl;
	}

};

int main() {

	MEM* mem = new MEM();
	CPU cpu;
	DEBUGLOG log;

	// Example code to load the program into memory
	Byte program[] = {
		0xA2, 0x04, // LDX 0x04
		0xA9, 0x06, // LDA 0x06
		0xA0, 0x05, // LDY 0x05
		0x69, 0x14, // ADD 0x14 to A
		0xCA,		// DEX Implied
		0x88,		// DEY Implied
		0x00
	};


	Word loadAddress = 0x0000;
	const char* file = "ROMS/6502_functional_test.bin";

	std::ifstream rom(file, std::ios::binary);
	if (rom.is_open()) {

		rom.seekg(0, std::ios::end);
		size_t romSize = rom.tellg();
		rom.seekg(0, std::ios::beg);
		if (romSize == 0 || romSize > (0xFFFF - loadAddress + 1)){
			std::cerr << "INVALID ROM SIZE.\n";
			delete mem;
			return 1;
		}
		rom.read(reinterpret_cast<char*>(&mem->Data[loadAddress]), romSize);
		rom.close();
	}
	else {
		std::cerr << "ERROR LOADING ROM FILE.\n";
		delete mem;
		return 1;
	}

	

	/*
	for (size_t i = 0; i < sizeof(program); ++i) {
		mem.Data[loadAddress + i] = program[i];
	}
	*/

	cpu.Pc = 0x400;
	cpu.start_up(mem);
	log.initDebug(mem, &cpu);

	std::cout << "RUNNING: "  << file << std::endl;
	/*
	log.printA();
	log.printX();
	log.printY();
	log.printMem(0x0002);

	std::cout << "--------------------------------------------" << std::endl;
	*/
	while (true) {
		cpu.tick();
		if (cpu.status.B == 1) {
			break;
		}
	}
	/*
	std::cout << "After operations: " << std::endl;
	log.printA();
	log.printX();
	log.printY();
	log.printMem(0x0200);
	*/
	while (true);
}


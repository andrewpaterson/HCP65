#ifndef __ASM65816_DEBUG_DECODER_H__
#define __ASM65816_DEBUG_DECODER_H__
#include "EmulatorDebugLib/DebugTarget.h"


//A W65C816's instructions for a debugger, from what its assembler knows of them (Assembler65Lib's disassembler and
//Asm65816Flow): text in the assembler's syntax, lengths, calls, returns, jumps, branches and where they go.  The
//immediates' widths are the target's now (its P and E, numbered as W65C816DebugRegisters numbers them), changed by
//any rep and sep among the instructions.
class CAsmW65C816DebugDecoder : public CDebugDecoder
{
public:
	size	Decode(CDebugTarget* pcTarget, uint32 uiAddress, size uiCount, SDebugInstruction* asDest) override;
};


#endif // __ASM65816_DEBUG_DECODER_H__

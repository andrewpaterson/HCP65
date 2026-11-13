#include <string.h>
#include "BaseLib/Chars.h"
#include "Assembler65Lib/AsmW65C816Disassembler.h"
#include "Assembler65Lib/AsmW65C816Opcodes.h"
#include "EmulatorDebugLib/W65C816DebugRegisters.h"
#include "AsmW65C816DebugDecoder.h"


#define ASM65816_DEBUG_M_FLAG	0x20
#define ASM65816_DEBUG_X_FLAG	0x10


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static EDebugInstruction Asm65816DebugKind(EAsmW65C816Flow eFlow)
{
	switch (eFlow)
	{
	case AW65C816F_Call:	return DI_Call;
	case AW65C816F_Return:	return DI_Return;
	case AW65C816F_Jump:	return DI_Jump;
	case AW65C816F_Branch:	return DI_Branch;
	default:				return DI_Other;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Stops where memory ends (an instruction must be all there).
//
//////////////////////////////////////////////////////////////////////////
size CAsmW65C816DebugDecoder::Decode(CDebugTarget* pcTarget, uint32 uiAddress, size uiCount, SDebugInstruction* asDest)
{
	SAsmW65C816Widths		sWidths;
	uint64				uiP;
	uint8				auiBytes[4];
	size				uiAvailable;
	size				uiLength;
	size				i;
	CChars				szText;
	SDebugInstruction*	psInstruction;
	uint32				uiTarget;

	uiP = pcTarget->GetRegisterValue(D816R_P);
	sWidths.bMemory8 = (uiP & ASM65816_DEBUG_M_FLAG) != 0;
	sWidths.bIndex8 = (uiP & ASM65816_DEBUG_X_FLAG) != 0;
	if (pcTarget->GetRegisterValue(D816R_E) != 0)
	{
		sWidths.bMemory8 = true;
		sWidths.bIndex8 = true;
	}

	for (i = 0; i < uiCount; i++)
	{
		memset(auiBytes, 0, sizeof(auiBytes));
		uiAvailable = pcTarget->PeekMemory(uiAddress, auiBytes, sizeof(auiBytes));
		szText.Init();
		uiLength = Asm65816Disassemble(uiAddress, auiBytes, uiAvailable, &sWidths, &szText);
		if (uiLength == 0)
		{
			szText.Kill();
			break;
		}

		psInstruction = &asDest[i];
		StartDebugInstruction(psInstruction, pcTarget, uiAddress, uiLength);
		strncpy(psInstruction->szText, szText.Text(), DEBUG_INSTRUCTION_TEXT - 1);
		szText.Kill();
		psInstruction->uiKind = (uint8)Asm65816DebugKind(Asm65816Flow(auiBytes[0]));
		if (Asm65816FlowTarget(uiAddress, auiBytes, &uiTarget))
		{
			psInstruction->uiTarget = uiTarget;
			psInstruction->uiHasTarget = true;
		}
		uiAddress += (uint32)uiLength;
	}
	return i;
}

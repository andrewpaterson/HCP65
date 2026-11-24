/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela Assembler65Lib

Codaphela Assembler65Lib is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela Assembler65Lib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela Assembler65Lib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __ASM65_DEBUG_RECORDS_H__
#define __ASM65_DEBUG_RECORDS_H__
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/ArrayUint32.h"
#include "BaseLib/Chars.h"
#include "BaseLib/ChunkFile.h"
#include "BaseLib/HashMapString.h"


#define ASM65_DEBUG_RECORDS_MAGIC	"Asm65DebugRecords"
#define ASM65_DEBUG_RECORDS_VERSION	6

#define ASM65_DEBUG_NONE		0xFFFFFFFF		//No type, function or section.
#define ASM65_DEBUG_ABSOLUTE	0xFFFFFFFE		//A place's section once linked: its offset is its address.
#define ASM65_DEBUG_FRAME		0xFF			//A function's variables are from its frame address (SAsm65DebugFunction::uiBase).
#define ASM65_DEBUG_REGISTERS	8				//The most registers a processor has for a debugger.


//The processor a program's records are for.
enum EAsm65DebugProcessor
{
	ADP_None,
	ADP_W65C816,
	ADP_65C64,
};


//The registers the records name (a frame's register, a saved register, the variables' base), by each processor's own
//names (Asm65DebugRegisterName), S first for every processor.
enum EAsmW65C816DebugRegister
{
	AW65DR_S,
	AW65DR_D,
	AW65DR_C,
	AW65DR_X,
	AW65DR_Y,
	AW65DR_P,
	AW65DR_Count,
};


enum EAsm65C64DebugRegister
{
	A65DR_S,
	A65DR_V,
	A65DR_W,
	A65DR_A,
	A65DR_B,
	A65DR_X,
	A65DR_Y,
	A65DR_P,
	A65DR_Count,
};


#define ASM65_DEBUG_S	0		//S is each processor's first register.


//How a function is got to, so what is above S as it starts: where its caller goes on.
enum EAsm65DebugEntry
{
	ADE_Long,		//jsl: the return address less one, three bytes.
	ADE_Short,		//jsr: the return address less one, two bytes, in the function's own bank.
	ADE_Interrupt,	//An interrupt (or cop, brk): P, then the address it goes back to, three bytes.
	ADE_UserInterrupt,	//A kernel's interrupt (or call) from user mode: as ADE_Interrupt, its caller in user mode's memory.
};


//How a function's frame address (S as its caller had it just before the call: the return address is just below it) is
//found while its body runs (DebuggerPlan.txt's frame model).  Its saved registers are at places from it.  Before its
//body (the prologue), and after its epilogue starts, a stack walker follows its instructions instead.
enum EAsm65DebugFrame
{
	ADF_FollowS,	//From S, following its instructions from its start: it has no frame register.
	ADF_Register,	//A register plus iFrameOffset.
	ADF_Memory,		//The address held at a register plus iFrameOffset, plus iFrameAdjust (an aligned frame's kept S).
};


//Where a variable is while its function runs, from its function's base (a register, or the frame address).
enum EAsm65DebugWhere
{
	ADW_None,		//Not kept anywhere the debugger can read (optimised away).
	ADW_Frame,		//At the base + iOffset.
	ADW_Indirect,	//At the address held at the base + iOffset (an object passed by address).
	ADW_Ranges,		//Where its ranges say, from code address to code address (SAsm65DebugRange); elsewhere not kept.
	ADW_Register,	//A range's: in a register (uiRegister).
	ADW_Memory,		//A range's: at a register (uiRegister) + iOffset.
	ADW_Address,	//A range's: at the address iOffset (the 65C64's pseudo registers, $00 to $FF).
};


//Where a variable whose place moves (ADW_Ranges, optimised code) is from uiFrom up to uiTo (its function's section's
//offsets, or once linked its addresses): at its function's base + iOffset (ADW_Frame), in a register (ADW_Register),
//at a register + iOffset (ADW_Memory) or at the address iOffset (ADW_Address).
struct SAsm65DebugRange
{
	uint32	uiFrom;
	uint32	uiTo;
	uint8	uiWhere;		//EAsm65DebugWhere.
	uint8	uiRegister;
	int32	iOffset;
};


//What a type is.  A typedef is not a type of its own: its name is its type's.
enum EAsm65DebugType
{
	ADT_Void,
	ADT_Bool,
	ADT_Integer,	//ADTF_Signed, ADTF_Character.
	ADT_Float,
	ADT_Enum,		//uiTarget: its underlying integer type; its enumerators are its members.
	ADT_Pointer,	//uiTarget: what it points at (ASM65_DEBUG_NONE for a function).
	ADT_Reference,	//uiTarget: what it refers to.  Held as a pointer.
	ADT_Array,		//uiTarget: the element's type, uiCount of them.
	ADT_Class,		//Its bases, fields and vtable pointer are its members; ADTF_Union, ADTF_Incomplete.
	ADT_Function,
};


#define ADTF_Signed			0x01
#define ADTF_Character		0x02	//Shown as a character (char, signed char, unsigned char).
#define ADTF_Union			0x04
#define ADTF_Incomplete		0x08	//Only declared where it was described: another unit's description replaces it.


enum EAsm65DebugMember
{
	ADM_Base,		//A base class at iValue.
	ADM_Field,		//A field at iValue, its bits from uiBitOffset when uiBits isn't 0.
	ADM_VTable,		//The vtable pointer at iValue.
	ADM_Enumerator,	//An enumerator, iValue.
};


//A place in the program: an offset in one of an object's sections, or once linked an address (uiSection
//ASM65_DEBUG_ABSOLUTE).
struct SAsm65DebugPlace
{
	uint32	uiSection;
	uint32	uiOffset;
};


//A function the compiler wrote: its code is from sStart to uiEnd (exclusive, in sStart's section), its prologue
//ends at uiBody, its variables are uiNumVariables from uiFirstVariable and its saved registers uiNumSaved from
//uiFirstSaved.  uiResult is the type it returns, ASM65_DEBUG_NONE for void (or code that doesn't say).
struct SAsm65DebugFunction
{
	CChars				szName;			//As C++ writes it (CShape::Area).
	SAsm65DebugPlace	sStart;
	uint32				uiEnd;
	uint32				uiBody;
	uint8				uiEntry;		//EAsm65DebugEntry.
	uint8				uiFrame;		//EAsm65DebugFrame,
	uint8				uiFrameRegister;	//from this register,
	int32				iFrameOffset;
	int32				iFrameAdjust;
	uint8				uiBase;			//The register its variables are from, ASM65_DEBUG_FRAME for its frame address.
	uint32				uiResult;
	uint32				uiFirstVariable;
	uint32				uiNumVariables;
	uint32				uiFirstSaved;
	uint32				uiNumSaved;
	uint32				uiFirstInline;
	uint32				uiNumInlines;
};


//What a function saved for its caller, at its frame address plus iOffset once its body runs: a register (its width
//the processor's) or uiBytes of memory at an address (a pseudo register a function keeps).
struct SAsm65DebugSaved
{
	uint32	uiWhat;			//A register, or with bMemory an address.
	bool	bMemory;
	uint8	uiBytes;
	int32	iOffset;
};


//A parameter or local of a function, in scope from uiFirstLine to uiLastLine of its function's file.
struct SAsm65DebugVariable
{
	CChars	szName;
	uint32	uiType;
	uint8	uiWhere;		//EAsm65DebugWhere.
	bool	bParameter;
	int32	iOffset;
	uint32	uiFirstLine;
	uint32	uiLastLine;
	uint32	uiFirstRange;	//ADW_Ranges: its ranges.
	uint32	uiNumRanges;

	bool	IsInScope(uint32 uiLine);
};


//A call inlined into a function (DebuggerFPlan.txt F2): the function inlined (as C++ names it), the inline it is
//in (iParent, an index among its function's inlines; -1 for none), the file and line of the call (in the parent's
//code, or the function's), and the code addresses that are its (SAsm65DebugSpan, from uiFirstSpan).
struct SAsm65DebugInline
{
	CChars	szName;
	CChars	szFile;
	uint32	uiLine;
	int32	iParent;
	uint32	uiFirstSpan;
	uint32	uiNumSpans;
};


//Code addresses from uiFrom up to uiTo (its function's section's offsets, or once linked its addresses).
struct SAsm65DebugSpan
{
	uint32	uiFrom;
	uint32	uiTo;
};


//A variable with a place of its own: a global, a static member, or a function's static local (uiFunction).
struct SAsm65DebugGlobal
{
	CChars				szName;
	uint32				uiType;
	SAsm65DebugPlace	sPlace;
	uint32				uiFunction;		//ASM65_DEBUG_NONE for a global.
};


struct SAsm65DebugType
{
	CChars	szKey;			//The same type in every unit has the same key: linked, they are one.
	CChars	szName;			//As C++ writes it.
	uint8	uiKind;			//EAsm65DebugType.
	uint8	uiFlags;		//ADTF_.
	uint32	uiBytes;
	uint32	uiTarget;
	uint32	uiCount;
	uint32	uiFirstMember;
	uint32	uiNumMembers;
};


struct SAsm65DebugMember
{
	CChars	szName;
	uint8	uiKind;			//EAsm65DebugMember.
	uint32	uiType;			//ASM65_DEBUG_NONE for an enumerator.
	int64	iValue;
	uint8	uiBitOffset;
	uint8	uiBits;
};


typedef CArrayTemplate<SAsm65DebugFunction>	CArrayAsm65DebugFunctions;
typedef CArrayTemplate<SAsm65DebugVariable>	CArrayAsm65DebugVariables;
typedef CArrayTemplate<SAsm65DebugGlobal>	CArrayAsm65DebugGlobals;
typedef CArrayTemplate<SAsm65DebugType>		CArrayAsm65DebugTypes;
typedef CArrayTemplate<SAsm65DebugMember>	CArrayAsm65DebugMembers;
typedef CArrayTemplate<SAsm65DebugSaved>	CArrayAsm65DebugSaved;
typedef CArrayTemplate<SAsm65DebugRange>	CArrayAsm65DebugRanges;
typedef CArrayTemplate<SAsm65DebugInline>	CArrayAsm65DebugInlines;
typedef CArrayTemplate<SAsm65DebugSpan>		CArrayAsm65DebugSpans;


//What a debugger knows of a program beyond its lines: its functions' frames, variables and inlined calls, its globals
//and their types.  An object has its units' (the assembler's .dfunction, .dframe, .dvariable, .drange, .dinline,
//.dinspan, .dglobal, .dstatic, .dtype and .dmember directives), at places in its sections; the linker appends each object's placed ones (Append), at their
//addresses, a type that more than one unit describes kept once.  Written in the object and in the program's debug
//file (CAsm65DebugInfo) the same way.
class CAsm65DebugRecords
{
protected:
	CArrayAsm65DebugFunctions	masFunctions;
	CArrayAsm65DebugVariables	masVariables;
	CArrayAsm65DebugGlobals		masGlobals;
	CArrayAsm65DebugTypes		masTypes;
	CArrayAsm65DebugMembers		masMembers;
	CArrayAsm65DebugSaved		masSaved;
	CArrayAsm65DebugRanges		masRanges;
	CArrayAsm65DebugInlines		masInlines;
	CArrayAsm65DebugSpans		masSpans;
	uint8						muiProcessor;	//EAsm65DebugProcessor.
	CHashMapString<uint32>		mcTypeKeys;		//Each type's index by its key.
	CArrayUint32				mauiFunctionOrder;	//The functions' indices in address order (Sort), for FindFunction.

public:
	void					Init(void);
	void					Kill(void);
	void					Clear(void);

	SAsm65DebugFunction*	AddFunction(const char* szName, uint32 uiSection, uint32 uiOffset, EAsm65DebugEntry eEntry, uint32 uiResult = ASM65_DEBUG_NONE);
	void					SetProcessor(EAsm65DebugProcessor eProcessor);
	EAsm65DebugProcessor	GetProcessor(void) const;
	bool					SetFrame(uint32 uiBody, EAsm65DebugFrame eFrame, uint8 uiRegister, int32 iOffset, int32 iAdjust);
	bool					SetBase(uint8 uiRegister);
	bool					AddSaved(uint32 uiWhat, bool bMemory, uint8 uiBytes, int32 iOffset);
	bool					EndFunction(uint32 uiEnd);
	void					EndFunctions(uint32 uiSection, uint32 uiEnd);
	bool					AddVariable(const char* szName, uint32 uiType, EAsm65DebugWhere eWhere, bool bParameter, int32 iOffset, uint32 uiFirstLine, uint32 uiLastLine);
	bool					AddRange(uint32 uiSection, uint32 uiFrom, uint32 uiTo, EAsm65DebugWhere eWhere, uint8 uiRegister, int32 iOffset);
	bool					AddInline(const char* szName, int32 iParent, const char* szFile, uint32 uiLine);
	bool					AddInlineSpan(uint32 uiSection, uint32 uiFrom, uint32 uiTo);
	bool					AddGlobal(const char* szName, uint32 uiType, uint32 uiSection, uint32 uiOffset, bool bStatic);
	bool					AddType(const char* szKey, const char* szName, EAsm65DebugType eKind, uint8 uiFlags, uint32 uiBytes, uint32 uiTarget, uint32 uiCount);
	bool					AddMember(const char* szName, EAsm65DebugMember eKind, uint32 uiType, int64 iValue, uint8 uiBitOffset, uint8 uiBits);
	bool					IsValid(void);

	void					Append(CAsm65DebugRecords* pcSource, CArrayUint32* pauiSectionAddresses);
	void					Sort(void);
	void					Move(int64 iOffset);

	size					NumFunctions(void) const;
	SAsm65DebugFunction*	GetFunction(size uiIndex);
	SAsm65DebugFunction*	FindFunction(uint32 uiAddress);
	size					NumVariables(void) const;
	SAsm65DebugVariable*	GetVariable(size uiIndex);
	SAsm65DebugVariable*	GetVariable(SAsm65DebugFunction* psFunction, size uiIndex);
	SAsm65DebugVariable*	FindVariable(SAsm65DebugFunction* psFunction, const char* szName, uint32 uiLine);
	bool					IsVisible(SAsm65DebugFunction* psFunction, SAsm65DebugVariable* psVariable, uint32 uiLine);
	size					FindVisible(SAsm65DebugFunction* psFunction, uint32 uiLine, CArrayUint32* pauiVariables);
	size					NumGlobals(void) const;
	SAsm65DebugGlobal*		GetGlobal(size uiIndex);
	SAsm65DebugGlobal*		FindGlobal(const char* szName, SAsm65DebugFunction* psFunction);
	size					NumTypes(void) const;
	SAsm65DebugType*		GetType(uint32 uiIndex);
	SAsm65DebugType*		FindType(const char* szKey);
	uint32					FindTypeIndex(const char* szKey);
	SAsm65DebugMember*		GetMember(SAsm65DebugType* psType, size uiIndex);
	SAsm65DebugSaved*		GetSaved(SAsm65DebugFunction* psFunction, size uiIndex);
	SAsm65DebugRange*		GetRange(SAsm65DebugVariable* psVariable, size uiIndex);
	SAsm65DebugRange*		FindRange(SAsm65DebugVariable* psVariable, uint32 uiAddress);
	SAsm65DebugInline*		GetInline(SAsm65DebugFunction* psFunction, size uiIndex);
	SAsm65DebugSpan*		GetSpan(SAsm65DebugInline* psInline, size uiIndex);
	int32					FindInline(SAsm65DebugFunction* psFunction, uint32 uiAddress);

	bool					WriteChunks(CChunkFile* pcChunkFile);
	bool					ReadChunks(CChunkFile* pcChunkFile);

protected:
	SAsm65DebugFunction*	LastFunction(void);
	SAsm65DebugType*		LastType(void);
	SAsm65DebugVariable*	LastVariable(void);
	bool					IsTypeOrNone(uint32 uiType);
	bool					IsWhole(void);
	uint32					MapType(uint32 uiType, CArrayUint32* pauiTypes);
	uint32					SectionAddress(uint32 uiSection, CArrayUint32* pauiSectionAddresses);
	void					AddMemberTo(SAsm65DebugMember* psSource, uint32 uiType);
	void					AddGlobalTo(const char* szName, uint32 uiType, uint32 uiAddress, uint32 uiFunction);
};


EAsm65DebugProcessor	Asm65DebugProcessorNamed(const char* szName);
const char*		Asm65DebugRegisterName(EAsm65DebugProcessor eProcessor, size uiRegister);
size			Asm65DebugNumRegisters(EAsm65DebugProcessor eProcessor);
uint8			Asm65DebugRegisterBytes(EAsm65DebugProcessor eProcessor, size uiRegister);


#endif // __ASM65_DEBUG_RECORDS_H__

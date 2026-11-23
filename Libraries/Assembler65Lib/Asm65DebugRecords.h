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
#define ASM65_DEBUG_RECORDS_VERSION	2

#define ASM65_DEBUG_NONE		0xFFFFFFFF		//No type, function or section.
#define ASM65_DEBUG_ABSOLUTE	0xFFFFFFFE		//A place's section once linked: its offset is its address.


//How a function is got to, so what is above S as it starts: where its caller goes on.
enum EAsm65DebugEntry
{
	ADE_Long,		//jsl: the return address less one, three bytes.
	ADE_Short,		//jsr: the return address less one, two bytes, in the function's own bank.
	ADE_Interrupt,	//An interrupt (or cop, brk): P, then the address it goes back to, three bytes.
};


//How a function's caller is found from the D its body runs on (the stack walker's rule, CAsm65StackWalker).
enum EAsm65DebugLink
{
	ADL_None,		//No frame: D is the caller's, and the return address is on top of the stack as the body starts.
	ADL_Frame,		//S after phd is D + iLink: the caller's D is at D + iLink + 1, the return address after it.
	ADL_SavedS,		//The word at D + iLink is S as it was after phd (an aligned frame).
	ADL_User,		//A kernel's interrupt from user mode, run on the kernel's own stack: the word at S + iLink + 1 is the
					//interrupted D and the one after it its S, where the interrupt's frame is in user mode's memory.
};


//Where a variable is while its function runs.
enum EAsm65DebugWhere
{
	ADW_None,		//Not kept anywhere the debugger can read (optimised away).
	ADW_Frame,		//At D + iOffset.
	ADW_Indirect,	//At the address held at D + iOffset (an object passed by address).
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
//ends at uiBody, its variables are uiNumVariables from uiFirstVariable.
struct SAsm65DebugFunction
{
	CChars				szName;			//As C++ writes it (CShape::Area).
	SAsm65DebugPlace	sStart;
	uint32				uiEnd;
	uint32				uiBody;
	uint8				uiEntry;		//EAsm65DebugEntry.
	uint8				uiLink;			//EAsm65DebugLink.
	int32				iLink;
	uint32				uiFirstVariable;
	uint32				uiNumVariables;
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


//What a debugger knows of a program beyond its lines: its functions' frames and variables, its globals and their
//types.  An object has its units' (the assembler's .dfunction, .dframe, .dvariable, .dglobal, .dstatic, .dtype and
//.dmember directives), at places in its sections; the linker appends each object's placed ones (Append), at their
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
	CHashMapString<uint32>		mcTypeKeys;		//Each type's index by its key.
	CArrayUint32				mauiFunctionOrder;	//The functions' indices in address order (Sort), for FindFunction.

public:
	void					Init(void);
	void					Kill(void);
	void					Clear(void);

	SAsm65DebugFunction*	AddFunction(const char* szName, uint32 uiSection, uint32 uiOffset, EAsm65DebugEntry eEntry);
	bool					SetFrame(uint32 uiBody, EAsm65DebugLink eLink, int32 iLink);
	bool					EndFunction(uint32 uiEnd);
	void					EndFunctions(uint32 uiSection, uint32 uiEnd);
	bool					AddVariable(const char* szName, uint32 uiType, EAsm65DebugWhere eWhere, bool bParameter, int32 iOffset, uint32 uiFirstLine, uint32 uiLastLine);
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
	size					NumGlobals(void) const;
	SAsm65DebugGlobal*		GetGlobal(size uiIndex);
	SAsm65DebugGlobal*		FindGlobal(const char* szName, SAsm65DebugFunction* psFunction);
	size					NumTypes(void) const;
	SAsm65DebugType*		GetType(uint32 uiIndex);
	SAsm65DebugType*		FindType(const char* szKey);
	SAsm65DebugMember*		GetMember(SAsm65DebugType* psType, size uiIndex);

	bool					WriteChunks(CChunkFile* pcChunkFile);
	bool					ReadChunks(CChunkFile* pcChunkFile);

protected:
	SAsm65DebugFunction*	LastFunction(void);
	SAsm65DebugType*		LastType(void);
	bool					IsTypeOrNone(uint32 uiType);
	bool					IsWhole(void);
	uint32					MapType(uint32 uiType, CArrayUint32* pauiTypes);
	uint32					SectionAddress(uint32 uiSection, CArrayUint32* pauiSectionAddresses);
	void					AddMemberTo(SAsm65DebugMember* psSource, uint32 uiType);
	void					AddGlobalTo(const char* szName, uint32 uiType, uint32 uiAddress, uint32 uiFunction);
};


#endif // __ASM65_DEBUG_RECORDS_H__

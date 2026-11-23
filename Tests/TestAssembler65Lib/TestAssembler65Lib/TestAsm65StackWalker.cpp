#include "BaseLib/Chars.h"
#include "MicroprocessorLib/W65C816InstructionFactory.h"
#include "Assembler65Lib/AsmW65C816Assembler.h"
#include "Assembler65Lib/AsmW65C816FrameCheck.h"
#include "Assembler65Lib/Asm65StackWalker.h"
#include "TestLib/Assert.h"


//Functions as the compiler makes them, and as it doesn't: pushes of both widths and pea round a call in a function
//with no frame, a frame of its own, an aligned frame that keeps S, and a leaf that pushes and pulls.  Start has no
//records: a walk stops there.
static const char* gszTestAsm65StackWalker =
	".org $8000\n"
	"Start:\n"
	"\tclc\n"
	"\txce\n"
	"\trep (m, x)\n"
	"\tjsl Outer\n"
	"\tstp\n"
	"Outer:\n"
	"\tsep m\n"
	"\tpha\n"
	"\trep m\n"
	"\tpea $1234\n"
	"\tjsl Framed\n"
	"\tpla\n"
	"\tsep m\n"
	"\tpla\n"
	"\trep m\n"
	"\trtl\n"
	"OuterEnd:\n"
	"Framed:\n"
	"\tphd\n"
	"\ttsc\n"
	"\tsec\n"
	"\tsbc 4\n"
	"\ttcs\n"
	"\ttcd\n"
	"FramedBody:\n"
	"\tjsl Leaf\n"
	"\tjsl Aligned\n"
	"\ttsc\n"
	"\tclc\n"
	"\tadc 4\n"
	"\ttcs\n"
	"\tpld\n"
	"\trtl\n"
	"FramedEnd:\n"
	"Leaf:\n"
	"\tphx\n"
	"\tldx 3\n"
	"\tplx\n"
	"\trtl\n"
	"LeafEnd:\n"
	"Aligned:\n"
	"\tphd\n"
	"\ttsc\n"
	"\ttax\n"
	"\tsec\n"
	"\tsbc 8\n"
	"\tand $FFF0\n"
	"\ttcs\n"
	"\ttcd\n"
	"\tstx [D + 6]\n"
	"AlignedBody:\n"
	"\tjsl Leaf\n"
	"\tlda [D + 6]\n"
	"\ttcs\n"
	"\tpld\n"
	"\trtl\n"
	"AlignedEnd:\n"
	".org $FFFC\n"
	"\t.word Start\n";


//////////////////////////////////////////////////////////////////////////
//
// The function from szName's label to szName End's, its body from szBody's
// (the start when NULL), its caller found as eLink and iLink say.
//
//////////////////////////////////////////////////////////////////////////
static void TestAsm65StackWalkerFunction(CAsm65DebugRecords* pcRecords, CAsmW65C816Assembler* pcAssembler, const char* szName, const char* szBody, EAsm65DebugLink eLink, int32 iLink)
{
	CChars	szEnd;
	int64	iStart;
	int64	iBody;
	int64	iEnd;

	szEnd.Init(szName)->Append("End");
	AssertTrue(pcAssembler->GetSymbol(szName, &iStart));
	iBody = iStart;
	AssertTrue(!szBody || pcAssembler->GetSymbol(szBody, &iBody));
	AssertTrue(pcAssembler->GetSymbol(szEnd.Text(), &iEnd));
	szEnd.Kill();

	//Each in a section of its own.
	pcRecords->AddFunction(szName, (uint32)pcRecords->NumFunctions(), (uint32)iStart);
	pcRecords->SetFrame((uint32)iBody, eLink, iLink);
	pcRecords->EndFunctions((uint32)pcRecords->NumFunctions() - 1, (uint32)iEnd);
}


//////////////////////////////////////////////////////////////////////////
//
// At every step the walk is the calls being run (CAsmW65C816FrameCheck),
// to Start's code, whose records it hasn't; and from registers that point
// nowhere a walk ends without reading past memory.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65StackWalkerSteps(void)
{
	CAsmW65C816Assembler	cAssembler;
	CAsm65DebugRecords		cRecords;
	CAsm65Memory			cMemory;
	CFastW65C816			cFast;
	CAsmW65C816FrameCheck	cCheck;
	CAsm65StackWalker		cWalker;
	CArrayAsm65StackFrames	asFrames;
	SAsm65StackRegisters	sRegisters;
	size					uiSteps;
	size					uiWrong;
	size					uiDeepest;
	uint32					uiFirstWrong;
	uint8					uiOpcode;
	int64					iBody;

	cAssembler.Init();
	AssertTrue(cAssembler.Assemble(gszTestAsm65StackWalker));
	AssertString("", cAssembler.GetErrors());
	cRecords.Init();
	TestAsm65StackWalkerFunction(&cRecords, &cAssembler, "Outer", NULL, ADL_None, 0);
	TestAsm65StackWalkerFunction(&cRecords, &cAssembler, "Framed", "FramedBody", ADL_Frame, 4);
	TestAsm65StackWalkerFunction(&cRecords, &cAssembler, "Leaf", NULL, ADL_None, 0);
	TestAsm65StackWalkerFunction(&cRecords, &cAssembler, "Aligned", "AlignedBody", ADL_SavedS, 6);
	AssertTrue(cAssembler.GetSymbol("FramedBody", &iBody));

	cMemory.Init();
	cMemory.Load(cAssembler.GetOutput());
	cAssembler.Kill();
	cMemory.InitFast65816(&cFast);
	cFast.Reset();
	cCheck.Init(&cMemory, &cFast);
	cWalker.Init(&cMemory);
	cWalker.AddRecords(&cRecords);
	asFrames.Init();

	uiWrong = 0;
	uiFirstWrong = 0;
	uiDeepest = 0;
	for (uiSteps = 0; (uiSteps < 1000) && !cFast.IsStopped(); uiSteps++)
	{
		sRegisters.uiPC = cFast.GetProgramAddress();
		sRegisters.uiS = cFast.GetS();
		sRegisters.uiD = cFast.GetD();
		sRegisters.uiC = cFast.GetC();
		sRegisters.uiX = cFast.GetX();
		sRegisters.uiY = cFast.GetY();
		sRegisters.uiP = cFast.GetP();
		cWalker.Walk(&sRegisters, &asFrames, 16);
		if (cCheck.WrongFrame(&asFrames) != 0)
		{
			uiFirstWrong = (uiWrong == 0) ? sRegisters.uiPC : uiFirstWrong;
			uiWrong++;
		}
		uiDeepest = (asFrames.NumElements() > uiDeepest) ? asFrames.NumElements() : uiDeepest;

		uiOpcode = cCheck.BeforeStep();
		cFast.Step();
		cCheck.AfterStep(uiOpcode);
	}
	AssertTrue(cFast.IsStopped());
	AssertString("", cCheck.GetWrites());
	AssertInt(0, (int)uiWrong);
	AssertInt(0, (int)uiFirstWrong);

	//Leaf in Aligned in Framed in Outer, called from Start.
	AssertInt(5, (int)uiDeepest);

	//A frame's D and S that point at the stack's top: its caller is read from whatever is there, past it nothing.
	sRegisters.uiPC = (uint32)iBody;
	sRegisters.uiS = 0xFFFE;
	sRegisters.uiD = 0xFFF0;
	AssertTrue(cWalker.Walk(&sRegisters, &asFrames, 16) <= 2);
	AssertInt(0, (int)cMemory.NumBadAccesses());

	asFrames.Kill();
	cWalker.Kill();
	cCheck.Kill();
	cFast.Kill();
	cMemory.Kill();
	cRecords.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65StackWalker(void)
{
	BeginTests();
	CW65C816InstructionFactory::GetInstance()->Init();

	TestAsm65StackWalkerSteps();

	CW65C816InstructionFactory::GetInstance()->Kill();
	TestStatistics();
}


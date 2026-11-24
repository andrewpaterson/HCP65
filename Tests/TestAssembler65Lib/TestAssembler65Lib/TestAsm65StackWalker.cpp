#include "BaseLib/Chars.h"
#include "MicroprocessorLib/W65C816InstructionFactory.h"
#include "Assembler65Lib/AsmW65C816Assembler.h"
#include "Assembler65Lib/AsmW65C816FrameCheck.h"
#include "Assembler65Lib/AsmW65C816StackWalker.h"
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


//A process's call to its kernel (cop) and the kernel's calls, its interrupt (brk) and a jsr, as HCP65/Kernal/Start.asm
//and KernalInterrupts.asm have them.  The process runs on a stack of its own, at $1FFF with D $1000; Cop keeps the
//process's S and D on the kernel's stack at $0FFF (.dframe user, 0), and its record ends after the instruction its
//call returns to (.dend): past it, S is going back to the process's.
static const char* gszTestAsm65StackWalkerKernel =
	".org $8000\n"
	"Start:\n"
	"\tclc\n"
	"\txce\n"
	"\trep (m, x)\n"
	"\tlda $1FFF\n"
	"\ttcs\n"
	"\tlda $1000\n"
	"\ttcd\n"
	"\tjsl Process\n"
	"\tstp\n"
	"Process:\n"
	"\tpea $5678\n"
	"\tcop 1\n"
	"\tpla\n"
	"\trtl\n"
	"ProcessEnd:\n"
	"Cop:\n"
	"\trep (m, x)\n"
	"\ttsc\n"
	"\tsta [Saved]:24\n"
	"\tlda $0FFF\n"
	"CopSwitch:\n"
	"\ttcs\n"
	"\tlda [Saved]:24\n"
	"\tpha\n"
	"\tphd\n"
	"CopBody:\n"
	"\tlda 0\n"
	"\ttcd\n"
	"\tjsl Kernel\n"
	"\tpld\n"
	"CopEnd:\n"
	"\tlda [Saved]:24\n"
	"\ttcs\n"
	"\trti\n"
	"Kernel:\n"
	"\t.width (A, X), 16\n"
	"\tphd\n"
	"\ttsc\n"
	"\tsec\n"
	"\tsbc 2\n"
	"\ttcs\n"
	"\ttcd\n"
	"KernelBody:\n"
	"\tjsr Short\n"
	"\tbrk 0\n"
	"\ttsc\n"
	"\tclc\n"
	"\tadc 2\n"
	"\ttcs\n"
	"\tpld\n"
	"\trtl\n"
	"KernelEnd:\n"
	"Short:\n"
	"\tpha\n"
	"\tpla\n"
	"\trts\n"
	"ShortEnd:\n"
	"Brk:\n"
	"\t.width (A, X), 16\n"
	"\tpha\n"
	"\tphd\n"
	"\tlda 0\n"
	"\ttcd\n"
	"\tjsr Short\n"
	"\tpld\n"
	"\tpla\n"
	"\trti\n"
	"BrkEnd:\n"
	"Saved:\n"
	"\t.word 0\n"
	".org $FFE4\n"
	"\t.word Cop\n"
	"\t.word Brk\n"
	".org $FFFC\n"
	"\t.word Start\n";


//A program run a step at a time, its stack walked before each step and checked against the calls being run.
struct STestAsm65StackWalker
{
	CAsm65DebugRecords		acRecords[2];		//EAsm65StackSpace's.
	CAsm65Memory			cMemory;
	CFastW65C816			cFast;
	CAsmW65C816FrameCheck	cCheck;
	CAsmW65C816StackWalker	cWalker;
	CArrayAsm65StackFrames	asFrames;
	SAsm65StackRegisters	sRegisters;
	int64					iKernel;			//Code from here on is the kernel's: before, user mode's.
};


//////////////////////////////////////////////////////////////////////////
//
// Assembles szSource; kernel mode's code is from szKernel's label (none
// when NULL).
//
//////////////////////////////////////////////////////////////////////////
static void TestAsm65StackWalkerInit(STestAsm65StackWalker* psTest, CAsmW65C816Assembler* pcAssembler, const char* szSource, const char* szKernel)
{
	pcAssembler->Init();
	AssertString("", pcAssembler->Assemble(szSource) ? "" : pcAssembler->GetErrors());
	psTest->iKernel = 0x1000000;
	AssertTrue(!szKernel || pcAssembler->GetSymbol(szKernel, &psTest->iKernel));
	psTest->acRecords[ASS_Kernel].Init();
	psTest->acRecords[ASS_User].Init();
	psTest->acRecords[ASS_Kernel].SetProcessor(ADP_W65C816);
	psTest->acRecords[ASS_User].SetProcessor(ADP_W65C816);
}


//////////////////////////////////////////////////////////////////////////
//
// The function from szName's label to szName End's, its body from szBody's
// (the start when NULL), got to as eEntry says, its frame address found
// as eFrame, uiRegister, iOffset and iAdjust say; in the kernel's records
// if it is after iKernel.  Returns those records, for what it saved.
//
//////////////////////////////////////////////////////////////////////////
static CAsm65DebugRecords* TestAsm65StackWalkerFunction(STestAsm65StackWalker* psTest, CAsmW65C816Assembler* pcAssembler, const char* szName, const char* szBody, EAsm65DebugEntry eEntry,
														EAsm65DebugFrame eFrame = ADF_FollowS, uint8 uiRegister = AW65DR_S, int32 iOffset = 0, int32 iAdjust = 0)
{
	CAsm65DebugRecords*	pcRecords;
	CChars				szEnd;
	int64				iStart;
	int64				iBody;
	int64				iEnd;

	szEnd.Init(szName)->Append("End");
	AssertTrue(pcAssembler->GetSymbol(szName, &iStart));
	iBody = iStart;
	AssertTrue(!szBody || pcAssembler->GetSymbol(szBody, &iBody));
	AssertTrue(pcAssembler->GetSymbol(szEnd.Text(), &iEnd));
	szEnd.Kill();

	//Each in a section of its own.
	pcRecords = &psTest->acRecords[(iStart >= psTest->iKernel) ? ASS_Kernel : ASS_User];
	pcRecords->AddFunction(szName, (uint32)pcRecords->NumFunctions(), (uint32)iStart, eEntry);
	pcRecords->SetFrame((uint32)iBody, eFrame, uiRegister, iOffset, iAdjust);
	pcRecords->EndFunctions((uint32)pcRecords->NumFunctions() - 1, (uint32)iEnd);
	return pcRecords;
}


//////////////////////////////////////////////////////////////////////////
//
// Loads the program, its records described.
//
//////////////////////////////////////////////////////////////////////////
static void TestAsm65StackWalkerLoad(STestAsm65StackWalker* psTest, CAsmW65C816Assembler* pcAssembler)
{
	psTest->cMemory.Init();
	psTest->cMemory.Load(pcAssembler->GetOutput());
	pcAssembler->Kill();
	psTest->cMemory.InitFast65816(&psTest->cFast);
	psTest->cFast.Reset();
	psTest->cCheck.Init(&psTest->cMemory, &psTest->cFast);
	psTest->cWalker.Init(&psTest->cMemory);
	psTest->cWalker.AddRecords(&psTest->acRecords[ASS_Kernel], ASS_Kernel);
	psTest->cWalker.AddRecords(&psTest->acRecords[ASS_User], ASS_User);
	psTest->asFrames.Init();
}


//////////////////////////////////////////////////////////////////////////
//
// Runs the program to its stp, its walk at every step the calls being
// run (CAsmW65C816FrameCheck) to the start up's code, whose records it
// hasn't; or, with the program address from iShortFrom to iShortTo, the
// walk may end early at a function described.  Returns the deepest walk.
//
//////////////////////////////////////////////////////////////////////////
static size TestAsm65StackWalkerRun(STestAsm65StackWalker* psTest, int64 iShortFrom, int64 iShortTo)
{
	SAsm65StackRegisters*	psRegisters;
	size					uiSteps;
	size					uiWrong;
	size					uiWrongFrame;
	size					uiDeepest;
	uint32					uiFirstWrong;
	uint8					uiOpcode;
	bool					bShort;

	psRegisters = &psTest->sRegisters;
	uiWrong = 0;
	uiFirstWrong = 0;
	uiDeepest = 0;
	for (uiSteps = 0; (uiSteps < 1000) && !psTest->cFast.IsStopped(); uiSteps++)
	{
		psRegisters->Init(&psTest->cFast, (psTest->cFast.GetProgramAddress() >= psTest->iKernel) ? ASS_Kernel : ASS_User);
		psTest->cWalker.Walk(psRegisters, &psTest->asFrames, 16);
		uiWrongFrame = psTest->cCheck.WrongFrame(&psTest->asFrames);
		bShort = (uiWrongFrame == psTest->asFrames.NumElements()) && (psRegisters->uiPC >= iShortFrom) && (psRegisters->uiPC < iShortTo);
		if ((uiWrongFrame != 0) && !bShort)
		{
			uiFirstWrong = (uiWrong == 0) ? psRegisters->uiPC : uiFirstWrong;
			uiWrong++;
		}
		uiDeepest = (psTest->asFrames.NumElements() > uiDeepest) ? psTest->asFrames.NumElements() : uiDeepest;

		uiOpcode = psTest->cCheck.BeforeStep();
		psTest->cFast.Step();
		psTest->cCheck.AfterStep(uiOpcode);
	}
	AssertTrue(psTest->cFast.IsStopped());
	AssertString("", psTest->cCheck.GetWrites());
	AssertInt(0, (int)uiWrong);
	AssertInt(0, (int)uiFirstWrong);
	return uiDeepest;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static void TestAsm65StackWalkerKill(STestAsm65StackWalker* psTest)
{
	psTest->asFrames.Kill();
	psTest->cWalker.Kill();
	psTest->cCheck.Kill();
	psTest->cFast.Kill();
	psTest->cMemory.Kill();
	psTest->acRecords[ASS_User].Kill();
	psTest->acRecords[ASS_Kernel].Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// At every step the walk is the calls being run, to Start's code; and from
// registers that point nowhere a walk ends without reading past memory.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65StackWalkerSteps(void)
{
	STestAsm65StackWalker	sTest;
	CAsmW65C816Assembler	cAssembler;
	int64					iBody;

	TestAsm65StackWalkerInit(&sTest, &cAssembler, gszTestAsm65StackWalker, NULL);
	//Framed's S after phd is D + 4: its frame address (S before jsl pushed 3 bytes and phd 2) is D + 9, the caller's D
	//below jsl's push.  Aligned keeps S after phd at D + 6.
	TestAsm65StackWalkerFunction(&sTest, &cAssembler, "Outer", NULL, ADE_Long);
	TestAsm65StackWalkerFunction(&sTest, &cAssembler, "Framed", "FramedBody", ADE_Long, ADF_Register, AW65DR_D, 9)->AddSaved(AW65DR_D, false, 0, -4);
	TestAsm65StackWalkerFunction(&sTest, &cAssembler, "Leaf", NULL, ADE_Long);
	TestAsm65StackWalkerFunction(&sTest, &cAssembler, "Aligned", "AlignedBody", ADE_Long, ADF_Memory, AW65DR_D, 6, 5)->AddSaved(AW65DR_D, false, 0, -4);
	AssertTrue(cAssembler.GetSymbol("FramedBody", &iBody));
	TestAsm65StackWalkerLoad(&sTest, &cAssembler);

	//Leaf in Aligned in Framed in Outer, called from Start.
	AssertInt(5, (int)TestAsm65StackWalkerRun(&sTest, 0, 0));

	//A frame's D and S that point at the stack's top: its caller is read from whatever is there, past it nothing.
	sTest.sRegisters.Init((uint32)iBody, ASS_User);
	sTest.sRegisters.Set(AW65DR_S, 0xFFFE);
	sTest.sRegisters.Set(AW65DR_D, 0xFFF0);
	AssertTrue(sTest.cWalker.Walk(&sTest.sRegisters, &sTest.asFrames, 16) <= 2);
	AssertInt(0, (int)sTest.cMemory.NumBadAccesses());
	TestAsm65StackWalkerKill(&sTest);
}


//////////////////////////////////////////////////////////////////////////
//
// A walk from the kernel's code goes through its interrupts: brk from the
// kernel to the code it interrupted, and the process's cop to the process
// from the process's S and D the kernel kept (ADE_UserInterrupt, S and D
// saved).  Stopped while Cop moves to the kernel's stack it ends at Cop.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65StackWalkerInterrupts(void)
{
	STestAsm65StackWalker	sTest;
	CAsmW65C816Assembler	cAssembler;
	CAsm65DebugRecords*		pcRecords;
	int64					iSwitch;
	int64					iBody;

	TestAsm65StackWalkerInit(&sTest, &cAssembler, gszTestAsm65StackWalkerKernel, "Cop");
	//Cop's body keeps the process's D at S + 1 and its S at S + 3, on the kernel's stack: its frame address there is S + 4.
	TestAsm65StackWalkerFunction(&sTest, &cAssembler, "Process", NULL, ADE_Long);
	pcRecords = TestAsm65StackWalkerFunction(&sTest, &cAssembler, "Cop", "CopBody", ADE_UserInterrupt, ADF_Register, AW65DR_S, 4);
	pcRecords->AddSaved(AW65DR_D, false, 0, -3);
	pcRecords->AddSaved(AW65DR_S, false, 0, -1);
	TestAsm65StackWalkerFunction(&sTest, &cAssembler, "Kernel", "KernelBody", ADE_Long, ADF_Register, AW65DR_D, 7)->AddSaved(AW65DR_D, false, 0, -4);
	TestAsm65StackWalkerFunction(&sTest, &cAssembler, "Short", NULL, ADE_Short);
	TestAsm65StackWalkerFunction(&sTest, &cAssembler, "Brk", NULL, ADE_Interrupt);
	AssertTrue(cAssembler.GetSymbol("CopSwitch", &iSwitch));
	AssertTrue(cAssembler.GetSymbol("CopBody", &iBody));
	TestAsm65StackWalkerLoad(&sTest, &cAssembler);

	//Short in Brk in Kernel in Cop in Process, called from Start.
	AssertInt(6, (int)TestAsm65StackWalkerRun(&sTest, iSwitch + 1, iBody));
	TestAsm65StackWalkerKill(&sTest);
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
	TestAsm65StackWalkerInterrupts();

	CW65C816InstructionFactory::GetInstance()->Kill();
	TestStatistics();
}


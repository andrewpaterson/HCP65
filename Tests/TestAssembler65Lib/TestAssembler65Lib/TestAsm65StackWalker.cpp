#include "BaseLib/Chars.h"
#include "MicroprocessorLib/W65C816InstructionFactory.h"
#include "Assembler65Lib/AsmW65C816Assembler.h"
#include "Assembler65Lib/AsmW65C816FrameCheck.h"
#include "Assembler65Lib/AsmW65C816StackWalker.h"
#include "Assembler65Lib/Asm65C64Assembler.h"
#include "Assembler65Lib/Asm65C64FrameCheck.h"
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


//A 65C64 program's functions as the compiler makes them: a frameless caller pushing arguments (one narrow, wth round
//it), then pushing A at 32 bits on the branch it takes past a wth A, 64 and a bra; a frame of its own from V (V
//pushed, S moved, V a copy of S, a kept unit [$80 + W] pushed); a leaf that pushes and pulls; a function whose V is a
//home with an exit before its last; brk's handler.  Exits set a width back after pulling V, as the back end's do.
//Start has no records.
static const char* gszTestAsm65StackWalker65C64 =
	".org $1000\n"
	"Start:\n"
	"\twth A, 32\n"
	"\twth B, 32\n"
	"\tjsr Outer\n"
	"\tstp\n"
	"Outer:\n"
	"\twth A, 8\n"
	"\tpsh A\n"
	"\twth A, 32\n"
	"\tpsh A\n"
	"\tjsr Framed\n"
	"\tadd S, 5\n"
	"\tcmp S, S\n"
	"\tbras z, OuterThin\n"
	"\twth A, 64\n"
	"\tbra OuterWide\n"
	"OuterThin:\n"
	"\tpsh A\n"
	"\tjsr Leaf\n"
	"\tpul A\n"
	"OuterWide:\n"
	"\twth A, 32\n"
	"\tret\n"
	"OuterEnd:\n"
	"Framed:\n"
	"\tpsh V\n"
	"\tsub S, 8\n"
	"\tcpy V, S\n"
	"\tpsh [$80 + W]\n"
	"FramedBody:\n"
	"\tjsr Leaf\n"
	"\tjsr HomeV\n"
	"\tbrk 5\n"
	"\tpul [$80 + W]\n"
	"\tadd S, 8\n"
	"\tpul V\n"
	"\twth A, 32\n"
	"\tret\n"
	"FramedEnd:\n"
	"Leaf:\n"
	"\tpsh (A, B)\n"
	"\tpul (A, B)\n"
	"\tret\n"
	"LeafEnd:\n"
	"HomeV:\n"
	"\tpsh V\n"
	"\tcpy V, 0\n"
	"\tbras z, HomeVOther\n"
	"\tpul V\n"
	"\tret\n"
	"HomeVOther:\n"
	"\tjsr Leaf\n"
	"\tpul V\n"
	"\twth A, 32\n"
	"\tret\n"
	"HomeVEnd:\n"
	"Brk:\n"
	"\tpsh A\n"
	"\tjsr Leaf\n"
	"\tpul A\n"
	"\trti\n"
	"BrkEnd:\n";


//A program run a step at a time on either processor, its stack walked before each step and checked against the calls
//being run.
struct STestAsm65StackWalker
{
	EAsm65DebugProcessor	eProcessor;
	CAsm65DebugRecords		acRecords[2];		//EAsm65StackSpace's.
	CAsm65Memory			cMemory;
	CFastW65C816			cFastW65C816;
	CFast65C64				cFast65C64;
	CAsmW65C816FrameCheck	cCheckW65C816;
	CAsm65C64FrameCheck		cCheck65C64;
	CAsm65FrameCheck*		pcCheck;
	CAsm65StackWalker*		pcWalker;
	CArrayAsm65StackFrames	asFrames;
	SAsm65StackRegisters	sRegisters;
	int64					iKernel;			//Code from here on is the kernel's: before, user mode's.
};


//////////////////////////////////////////////////////////////////////////
//
// Assembles szSource (pcAssembler is Init'ed, for eProcessor); kernel
// mode's code is from szKernel's label (none when NULL).
//
//////////////////////////////////////////////////////////////////////////
static void TestAsm65StackWalkerInit(STestAsm65StackWalker* psTest, CAsm65BaseAssembler* pcAssembler, EAsm65DebugProcessor eProcessor, const char* szSource, const char* szKernel)
{
	psTest->eProcessor = eProcessor;
	AssertString("", pcAssembler->Assemble(szSource) ? "" : pcAssembler->GetErrors());
	psTest->iKernel = 0x100000000LL;
	AssertTrue(!szKernel || pcAssembler->GetSymbol(szKernel, &psTest->iKernel));
	psTest->acRecords[ASS_Kernel].Init();
	psTest->acRecords[ASS_User].Init();
	psTest->acRecords[ASS_Kernel].SetProcessor(eProcessor);
	psTest->acRecords[ASS_User].SetProcessor(eProcessor);
}


//////////////////////////////////////////////////////////////////////////
//
// The function from szName's label to szName End's, its body from szBody's
// (the start when NULL), got to as eEntry says, its frame address found
// as eFrame, uiRegister, iOffset and iAdjust say; in the kernel's records
// if it is after iKernel.  Returns those records, for what it saved.
//
//////////////////////////////////////////////////////////////////////////
static CAsm65DebugRecords* TestAsm65StackWalkerFunction(STestAsm65StackWalker* psTest, CAsm65BaseAssembler* pcAssembler, const char* szName, const char* szBody, EAsm65DebugEntry eEntry,
														EAsm65DebugFrame eFrame = ADF_FollowS, uint8 uiRegister = ASM65_DEBUG_S, int32 iOffset = 0, int32 iAdjust = 0)
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
// Loads the program, its records described, on its processor reset.
//
//////////////////////////////////////////////////////////////////////////
static void TestAsm65StackWalkerLoad(STestAsm65StackWalker* psTest, CAsm65BaseAssembler* pcAssembler, uint32 uiReset = 0)
{
	psTest->cMemory.Init();
	if (psTest->eProcessor == ADP_65C64)
	{
		psTest->cMemory.LoadProgram(pcAssembler->GetOutput(), uiReset);
		psTest->cMemory.InitFast65C64(&psTest->cFast65C64);
		psTest->cFast65C64.Reset();
		psTest->cCheck65C64.Init(&psTest->cMemory, &psTest->cFast65C64);
		psTest->pcCheck = &psTest->cCheck65C64;
	}
	else
	{
		psTest->cMemory.Load(pcAssembler->GetOutput());
		psTest->cMemory.InitFast65816(&psTest->cFastW65C816);
		psTest->cFastW65C816.Reset();
		psTest->cCheckW65C816.Init(&psTest->cMemory, &psTest->cFastW65C816);
		psTest->pcCheck = &psTest->cCheckW65C816;
	}
	psTest->pcWalker = Asm65NewStackWalker(psTest->eProcessor, &psTest->cMemory);
	psTest->pcWalker->AddRecords(&psTest->acRecords[ASS_Kernel], ASS_Kernel);
	psTest->pcWalker->AddRecords(&psTest->acRecords[ASS_User], ASS_User);
	psTest->asFrames.Init();
}


//////////////////////////////////////////////////////////////////////////
//
// The processor's registers as they are, in the space its code is in.
//
//////////////////////////////////////////////////////////////////////////
static void TestAsm65StackWalkerRegisters(STestAsm65StackWalker* psTest)
{
	uint32	uiPC;

	if (psTest->eProcessor == ADP_65C64)
	{
		uiPC = psTest->cFast65C64.GetProgramCounter();
		psTest->sRegisters.Init(&psTest->cFast65C64, (uiPC >= psTest->iKernel) ? ASS_Kernel : ASS_User);
	}
	else
	{
		uiPC = psTest->cFastW65C816.GetProgramAddress();
		psTest->sRegisters.Init(&psTest->cFastW65C816, (uiPC >= psTest->iKernel) ? ASS_Kernel : ASS_User);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Runs the program to its stp, its walk at every step the calls being
// run (CAsm65FrameCheck) to the start up's code, whose records it
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
	bool					bShort;

	psRegisters = &psTest->sRegisters;
	uiWrong = 0;
	uiFirstWrong = 0;
	uiDeepest = 0;
	for (uiSteps = 0; (uiSteps < 1000) && !psTest->pcCheck->IsStopped(); uiSteps++)
	{
		TestAsm65StackWalkerRegisters(psTest);
		psTest->pcWalker->Walk(psRegisters, &psTest->asFrames, 16);
		uiWrongFrame = psTest->pcCheck->WrongFrame(&psTest->asFrames);
		bShort = (uiWrongFrame == psTest->asFrames.NumElements()) && (psRegisters->uiPC >= iShortFrom) && (psRegisters->uiPC < iShortTo);
		if ((uiWrongFrame != 0) && !bShort)
		{
			uiFirstWrong = (uiWrong == 0) ? psRegisters->uiPC : uiFirstWrong;
			uiWrong++;
		}
		uiDeepest = (psTest->asFrames.NumElements() > uiDeepest) ? psTest->asFrames.NumElements() : uiDeepest;
		psTest->pcCheck->Run(1);
	}
	AssertTrue(psTest->pcCheck->IsStopped());
	if (psTest->eProcessor == ADP_W65C816)
	{
		AssertString("", psTest->cCheckW65C816.GetWrites());
	}
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
	SafeKill(psTest->pcWalker);
	psTest->pcCheck->Kill();
	if (psTest->eProcessor == ADP_65C64)
	{
		psTest->cFast65C64.Kill();
	}
	else
	{
		psTest->cFastW65C816.Kill();
	}
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

	cAssembler.Init();
	TestAsm65StackWalkerInit(&sTest, &cAssembler, ADP_W65C816, gszTestAsm65StackWalker, NULL);
	//Framed's S after phd is D + 4: its frame address (S before jsl pushed 3 bytes and phd 2) is D + 9, the caller's D
	//below jsl's push.  Aligned keeps S after phd at D + 6.
	TestAsm65StackWalkerFunction(&sTest, &cAssembler, "Outer", NULL, ADE_Long);
	TestAsm65StackWalkerFunction(&sTest, &cAssembler, "Framed", "FramedBody", ADE_Long, ADF_Register, AW65DR_D, 9)->AddSaved(AW65DR_D, false, 0, -4);
	TestAsm65StackWalkerFunction(&sTest, &cAssembler, "Leaf", NULL, ADE_Long);
	TestAsm65StackWalkerFunction(&sTest, &cAssembler, "Aligned", "AlignedBody", ADE_Long, ADF_Memory, AW65DR_D, 6, 5)->AddSaved(AW65DR_D, false, 0, -4);
	AssertTrue(cAssembler.GetSymbol("FramedBody", &iBody));
	TestAsm65StackWalkerLoad(&sTest, &cAssembler);
	cAssembler.Kill();

	//Leaf in Aligned in Framed in Outer, called from Start.
	AssertInt(5, (int)TestAsm65StackWalkerRun(&sTest, 0, 0));

	//A frame's D and S that point at the stack's top: its caller is read from whatever is there, past it nothing.
	sTest.sRegisters.Init((uint32)iBody, ASS_User);
	sTest.sRegisters.Set(AW65DR_S, 0xFFFE);
	sTest.sRegisters.Set(AW65DR_D, 0xFFF0);
	AssertTrue(sTest.pcWalker->Walk(&sTest.sRegisters, &sTest.asFrames, 16) <= 2);
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

	cAssembler.Init();
	TestAsm65StackWalkerInit(&sTest, &cAssembler, ADP_W65C816, gszTestAsm65StackWalkerKernel, "Cop");
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
	cAssembler.Kill();

	//Short in Brk in Kernel in Cop in Process, called from Start.
	AssertInt(6, (int)TestAsm65StackWalkerRun(&sTest, iSwitch + 1, iBody));
	TestAsm65StackWalkerKill(&sTest);
}


//////////////////////////////////////////////////////////////////////////
//
// The 65C64's walks at every step are the calls being run, to Start's
// code: a framed function by its rule (V + 16, the caller's V and its kept
// unit saved below the return), in its prologue and epilogue by following
// its code, the others by following theirs (an exit before the last
// passed over as the stack was before it), brk's handler by its 4 byte
// return and P.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65StackWalker65C64(void)
{
	STestAsm65StackWalker	sTest;
	CAsm65C64Assembler		cAssembler;
	CAsm65DebugRecords*		pcRecords;
	int64					iBrk;

	cAssembler.Init();
	TestAsm65StackWalkerInit(&sTest, &cAssembler, ADP_65C64, gszTestAsm65StackWalker65C64, NULL);
	TestAsm65StackWalkerFunction(&sTest, &cAssembler, "Outer", NULL, ADE_Long);
	pcRecords = TestAsm65StackWalkerFunction(&sTest, &cAssembler, "Framed", "FramedBody", ADE_Long, ADF_Register, A65DR_V, 16);
	pcRecords->AddSaved(A65DR_V, false, 0, -7);
	pcRecords->AddSaved(0x80, true, 4, -19);
	TestAsm65StackWalkerFunction(&sTest, &cAssembler, "Leaf", NULL, ADE_Long);
	TestAsm65StackWalkerFunction(&sTest, &cAssembler, "HomeV", NULL, ADE_Long);
	TestAsm65StackWalkerFunction(&sTest, &cAssembler, "Brk", NULL, ADE_Interrupt);
	AssertTrue(cAssembler.GetSymbol("Brk", &iBrk));
	TestAsm65StackWalkerLoad(&sTest, &cAssembler, 0x1000);
	cAssembler.Kill();
	sTest.cMemory.SetLong(VECTOR_65C64_BASE + 4 * 5, (uint32)iBrk);

	//Leaf in HomeV (or Brk) in Framed in Outer, called from Start.
	AssertInt(5, (int)TestAsm65StackWalkerRun(&sTest, 0, 0));
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
	TestAsm65StackWalker65C64();

	CW65C816InstructionFactory::GetInstance()->Kill();
	TestStatistics();
}


#include "TestLib/Assert.h"
#include "TestAsm65C64Context.h"


//A program that copies the console's input to $20000 until it ends and stops with the bytes copied in A.
static const char* gszConsoleCopy =
	"		.org $1000\n"
	"		wth B, 8\n"
	"		wtth (A, X), 32\n"
	"		wth Y, 32\n"
	"		cpy Y, $20000\n"
	"		cpy A, 0\n"
	"Loop:\n"
	"		cpy [$FF0104], B\n"
	"		cpy B, [$FF0105]\n"
	"		bras z, Done\n"
	"		cpy B, [$FF0106]\n"
	"		cpy [Y], B\n"
	"		add Y, 1\n"
	"		add A, 1\n"
	"		bra Loop\n"
	"Done:\n"
	"		stp\n";


//////////////////////////////////////////////////////////////////////////
//
// A program reads its input from a CChars to the end; reading the
// registers afterwards changes nothing.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65ConsoleInput(void)
{
	STestAsm65Run	sRun;
	CChars			szInput;

	szInput.Init("Hi\n\xFF!");
	AssertTrue(TestAsm65Begin(&sRun, gszConsoleCopy, 0x1000));
	sRun.pcContext->SetConsoleInput(&szInput);
	AssertTrue(TestAsm65Run(&sRun, 1000) < 1000);

	AssertLong(5, TestAsm65MPU(&sRun)->GetA());
	AssertInt('H', sRun.pcContext->GetByte(0x20000));
	AssertInt('i', sRun.pcContext->GetByte(0x20001));
	AssertInt('\n', sRun.pcContext->GetByte(0x20002));
	AssertInt(0xFF, sRun.pcContext->GetByte(0x20003));
	AssertInt('!', sRun.pcContext->GetByte(0x20004));
	AssertInt(0, sRun.pcContext->GetByte(0x20005));
	AssertInt(0, sRun.pcContext->GetByte(ASM65_CONSOLE_INPUT + ASM65_CONSOLE_STATUS));
	AssertInt(0, sRun.pcContext->GetByte(ASM65_CONSOLE_INPUT + ASM65_CONSOLE_STATUS));
	AssertSize(0, sRun.pcContext->NumBadAccesses());

	TestAsm65End(&sRun);
	szInput.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// With no input the first read is the end; the status and data registers
// are read only.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65ConsoleNoInput(void)
{
	STestAsm65Run	sRun;
	CAsm65Memory	cMemory;

	AssertTrue(TestAsm65Begin(&sRun, gszConsoleCopy, 0x1000));
	AssertTrue(TestAsm65Run(&sRun, 1000) < 1000);
	AssertLong(0, TestAsm65MPU(&sRun)->GetA());
	TestAsm65End(&sRun);

	cMemory.Init();
	cMemory.SetByte(ASM65_CONSOLE_INPUT + ASM65_CONSOLE_STATUS, 1);
	cMemory.SetByte(ASM65_CONSOLE_INPUT + ASM65_CONSOLE_DATA, 7);
	AssertSize(2, cMemory.NumBadAccesses());
	AssertInt(0, cMemory.GetByte(ASM65_CONSOLE_INPUT + ASM65_CONSOLE_STATUS));
	AssertInt(0, cMemory.GetByte(ASM65_CONSOLE_INPUT + ASM65_CONSOLE_DATA));
	cMemory.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A callback's input, taken a byte at a time as the program asks.
//
//////////////////////////////////////////////////////////////////////////
static int TestAsm65ConsoleCount(void* pvCount)
{
	int*	piCount;

	piCount = (int*)pvCount;
	if (*piCount == 3)
	{
		return -1;
	}
	(*piCount)++;
	return 'a' + *piCount;
}


void TestAsm65ConsoleCallback(void)
{
	STestAsm65Run	sRun;
	int				iCount;

	iCount = 0;
	AssertTrue(TestAsm65Begin(&sRun, gszConsoleCopy, 0x1000));
	sRun.pcContext->SetConsoleInput(TestAsm65ConsoleCount, &iCount);
	AssertTrue(TestAsm65Run(&sRun, 1000) < 1000);
	AssertLong(3, TestAsm65MPU(&sRun)->GetA());
	AssertInt('b', sRun.pcContext->GetByte(0x20000));
	AssertInt('d', sRun.pcContext->GetByte(0x20002));
	AssertInt(3, iCount);
	TestAsm65End(&sRun);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65C64Console(void)
{
	BeginTests();

	TestAsm65ConsoleInput();
	TestAsm65ConsoleNoInput();
	TestAsm65ConsoleCallback();

	TestStatistics();
}

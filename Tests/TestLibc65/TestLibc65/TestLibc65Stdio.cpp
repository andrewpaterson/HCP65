#include <stdio.h>
#include "BaseLib/Chars.h"
#include "TestLib/Assert.h"
#include "TestLibc65Context.h"


//////////////////////////////////////////////////////////////////////////
//
// fread and fwrite with more bytes than 65535, on the host file device.
// A size and a count that each fit a size_t and whose product doesn't
// (500 by 200) are read and written whole on both targets.  On the
// W65C816, where a size_t is 16 bits, fread32 and fwrite32 take a count,
// or a size, of more than one holds (malloc32's blocks are what they
// read into and write from).  What is read is what was written, the
// files are as long as they should be, and the end is the end.
//
//////////////////////////////////////////////////////////////////////////
void TestLibc65StdioLargeReadWrite(void)
{
	static const char*	aszUnits[] = { "stdio.cpp", "host.cpp", "stdlib.cpp", "string.cpp", "string.asm", "ctype.cpp", "softfloat.cpp", "softfloat.asm", NULL };
	static const int32	aiExpected[] = { 1, 200, 100000, 200, 0, 1, 1, 100000, 1, 170000, 100000, 1, 1, 1, 0 };
	STestLibc65Run		sRun;
	FILE*				pFile;
	CChars				szFile;
	size				i;
	long				iLength;

	AssertTrue(TestLibc65Begin(&sRun, aszUnits, "",
		"#include <stdio.h>\n"
		"#include <stdlib.h>\n"
		"#include <string.h>\n"
		"#if defined(__W65C816__)\n"
		"#define big_malloc malloc32\n"
		"#define big_fread fread32\n"
		"#define big_fwrite fwrite32\n"
		"#define big_memcmp memcmp32\n"
		"#else\n"
		"#define big_malloc malloc\n"
		"#define big_fread fread\n"
		"#define big_fwrite fwrite\n"
		"#define big_memcmp memcmp\n"
		"#endif\n"
		"int main()\n"
		"{\n"
		"	long*			p = (long*)0x100000;\n"
		"	unsigned char*	pucWrite = (unsigned char*)big_malloc(100000UL);\n"
		"	unsigned char*	pucRead = (unsigned char*)big_malloc(100000UL);\n"
		"	unsigned long	i;\n"
		"	FILE*			f;\n"
		"	for (i = 0; i < 100000UL; i++) { pucWrite[i] = (unsigned char)(i * 7 + (i >> 8)); pucRead[i] = 0; }\n"
		"	f = fopen(\"Product.bin\", \"wb\");\n"
		"	*p++ = f != NULL;\n"
		"	*p++ = (long)fwrite(pucWrite, 500, 200, f);\n"
		"	*p++ = ftell(f);\n"
		"	fclose(f);\n"
		"	f = fopen(\"Product.bin\", \"rb\");\n"
		"	*p++ = (long)fread(pucRead, 500, 200, f);\n"
		"	*p++ = (long)fread(pucRead, 1, 1, f);\n"
		"	*p++ = feof(f) != 0;\n"
		"	fclose(f);\n"
		"	*p++ = big_memcmp(pucWrite, pucRead, 100000UL) == 0;\n"
		"	for (i = 0; i < 100000UL; i++) { pucRead[i] = 0; }\n"
		"	f = fopen(\"Count.bin\", \"wb\");\n"
		"	*p++ = (long)big_fwrite(pucWrite, 1, 100000UL, f);\n"
		"	*p++ = (long)big_fwrite(pucWrite, 70000UL, 1, f);\n"
		"	*p++ = ftell(f);\n"
		"	fclose(f);\n"
		"	f = fopen(\"Count.bin\", \"rb\");\n"
		"	*p++ = (long)big_fread(pucRead, 1, 100000UL, f);\n"
		"	*p++ = big_memcmp(pucWrite, pucRead, 100000UL) == 0;\n"
		"	for (i = 0; i < 100000UL; i++) { pucRead[i] = 0; }\n"
		"	*p++ = (long)big_fread(pucRead, 70000UL, 1, f);\n"
		"	*p++ = big_memcmp(pucWrite, pucRead, 70000UL) == 0;\n"
		"	*p++ = (long)big_fread(pucRead, 70000UL, 1, f);\n"
		"	fclose(f);\n"
		"	return 0;\n"
		"}\n"));
	TestLibc65HostFiles(&sRun);
	AssertTrue(TestLibc65Run(&sRun, 200000000));

	for (i = 0; i < sizeof(aiExpected) / sizeof(int32); i++)
	{
		AssertInt(aiExpected[i], TestLibc65Result(&sRun, i));
	}

	//The host's files are as long as the program was told.
	szFile.Init("Output" _FS_ "Host");
	szFile.Append((geTestLibc65Target == CPPT_W65C816) ? "W65C816" : "65C64")->Append(_FS_ "Count.bin");
	pFile = fopen(szFile.Text(), "rb");
	AssertNotNull(pFile);
	if (pFile)
	{
		fseek(pFile, 0, SEEK_END);
		iLength = ftell(pFile);
		fclose(pFile);
		AssertInt(170000, (int)iLength);
	}
	szFile.Kill();

	TestLibc65End(&sRun);
}


//////////////////////////////////////////////////////////////////////////
//
// stdin reads the console's input (CAsm65Memory's SetConsoleInput):
// getchar, and fgets to a new line, to the buffer's size and to the
// input's end; then EOF.  With no input the first read is EOF.
//
//////////////////////////////////////////////////////////////////////////
void TestLibc65StdioConsoleInput(void)
{
	static const char*	aszUnits[] = { "stdio.cpp", "host.cpp", "stdlib.cpp", "string.cpp", "string.asm", "ctype.cpp", "softfloat.cpp", "softfloat.asm", NULL };
	static const int32	aiExpected[] = { 0, 'a', 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1 };
	static const char*	szSource =
		"#include <stdio.h>\n"
		"#include <string.h>\n"
		"int main()\n"
		"{\n"
		"	long*	p = (long*)0x100000;\n"
		"	char	sz[8];\n"
		"	*p++ = feof(stdin);\n"
		"	*p++ = getchar();\n"
		"	*p++ = fgets(sz, 8, stdin) == sz;\n"
		"	*p++ = strcmp(sz, \"b\\n\") == 0;\n"
		"	*p++ = fgets(sz, 5, stdin) == sz;\n"
		"	*p++ = strcmp(sz, \"line\") == 0;\n"
		"	*p++ = fgets(sz, 8, stdin) == sz;\n"
		"	*p++ = strcmp(sz, \" two\\n\") == 0;\n"
		"	*p++ = fgets(sz, 8, stdin) == sz;\n"
		"	*p++ = strcmp(sz, \"xyz\") == 0;\n"
		"	*p++ = feof(stdin) != 0;\n"
		"	*p++ = fgets(sz, 8, stdin) == NULL;\n"
		"	*p++ = getchar();\n"
		"	return 0;\n"
		"}\n";
	STestLibc65Run		sRun;
	CChars				szInput;
	size				i;

	szInput.Init("ab\nline two\nxyz");
	AssertTrue(TestLibc65Begin(&sRun, aszUnits, "", szSource));
	sRun.pcMemory->SetConsoleInput(&szInput);
	AssertTrue(TestLibc65Run(&sRun, 10000000));
	for (i = 0; i < sizeof(aiExpected) / sizeof(int32); i++)
	{
		AssertInt(aiExpected[i], TestLibc65Result(&sRun, i));
	}
	TestLibc65End(&sRun);
	szInput.Kill();

	AssertTrue(TestLibc65Begin(&sRun, aszUnits, "",
		"#include <stdio.h>\n"
		"int main()\n"
		"{\n"
		"	long*	p = (long*)0x100000;\n"
		"	*p++ = getchar();\n"
		"	*p++ = feof(stdin) != 0;\n"
		"	return 0;\n"
		"}\n"));
	AssertTrue(TestLibc65Run(&sRun, 10000000));
	AssertInt(-1, TestLibc65Result(&sRun, 0));
	AssertInt(1, TestLibc65Result(&sRun, 1));
	TestLibc65End(&sRun);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestLibc65Stdio(void)
{
	BeginTests();

	TestLibc65StdioLargeReadWrite();
	TestLibc65StdioConsoleInput();

	TestStatistics();
}

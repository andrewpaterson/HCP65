#ifndef __HCP65_PROGRAM_BUILD_H__
#define __HCP65_PROGRAM_BUILD_H__
#include "CppParserLib/CPP65Builder.h"


//Adds what the HCP65 kernal or one of its programs is built from to pcBuilder, as HCP65-IDE builds the program's
//solution: compiled for the W65C816 with line tables, unused code removed, Libc65's system headers, the common and
//HCP65 libraries' headers and the project's own (and szIncludeDirectory's, if given: the user programs' HCP65User.h),
//then the project's units in link order (szProjectFile, a Visual Studio project), BaseLib and aszLibraries (NULL
//terminated, or NULL: more common libraries, StandardLib, SupportLib) linked as libraries, and Libc65's runtime.
//Directories are szCommonLibraries' and szHCP65Libraries' (which holds Libc65); relative ones are made full.  False,
//with the builder's errors, if a project can't be read.
bool HCP65AddProgram(CCPP65Builder* pcBuilder, const char* szCommonLibraries, const char* szHCP65Libraries, const char* szProjectFile, const char* szIncludeDirectory, const char** aszLibraries);


#endif // __HCP65_PROGRAM_BUILD_H__


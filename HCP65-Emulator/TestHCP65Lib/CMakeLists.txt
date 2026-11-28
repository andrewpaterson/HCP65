# Linux build of TestHCP65Lib (BaseLib + TestLib + StandardLib + SupportLib + MicroprocessorLib +
# Assembler65Lib + CppCompilerLib + ThreadLib + EmulatorDebugLib + HCP65Lib + TestHCP65Lib).  The tests run the HCP65
# emulator and build and boot the kernal ROM (compiled for the W65C816).
#
# Builds 32-bit (-m32, needs g++-multilib) by default.  Add -DBASELIB_64=ON
# for a 64-bit build, in its own build directory:
#
#   cmake -S HCP65/HCP65-Emulator/TestHCP65Lib -B build/TestHCP65Lib64 -G Ninja -DBASELIB_64=ON
#
#   cmake -S HCP65/HCP65-Emulator/TestHCP65Lib -B build/TestHCP65Lib -G Ninja -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build/TestHCP65Lib
#   ctest --test-dir build/TestHCP65Lib -V
#
# Add -DBASELIB_ASAN=ON for AddressSanitizer and LeakSanitizer.
#
# The targets are read from HCP65Emulator.sln, the solution it is a project of (codaphela_solution): the libraries its
# GCC-x64 rows build, each one's sources from its .vcxproj, so Visual Studio stays the source of truth.  Building
# re-runs the configure step when the solution or a project changes.  Only the libraries the project references are
# built (REFERENCED).  HCP65/Kernal is W65C816 code built by the tests, not by this file.

cmake_minimum_required(VERSION 3.24)
project(TestHCP65Lib CXX)


# The emulator is in the HCP65 repository (with Assembler65Lib and Libc65), which is beside CommonLibraries.
get_filename_component(HCP65_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
get_filename_component(CODAPHELA_ROOT "${HCP65_ROOT}/.." ABSOLUTE)
set(HCP65_LIBRARIES_DIR "${HCP65_ROOT}/Libraries")
set(COMMON_DIR "${CODAPHELA_ROOT}/CommonLibraries/Libraries")

include("${COMMON_DIR}/../CMake/Codaphela.cmake")

codaphela_options("${COMMON_DIR}" "${HCP65_LIBRARIES_DIR}" "${HCP65_ROOT}/HCP65-Emulator")

codaphela_solution("${CMAKE_CURRENT_SOURCE_DIR}/../HCP65Emulator.sln" TESTS TestHCP65Lib REFERENCED)

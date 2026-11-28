# Linux build of TestLibc65 (BaseLib + TestLib + StandardLib + CppCompilerLib +
# MicroprocessorLib + Assembler65Lib + TestLibc65).  Libc65 is compiled with the
# 65C64 generator together with each test program, assembled and run on the
# 65C64 simulator.
#
# Builds 32-bit (-m32, needs g++-multilib) by default.  Add -DBASELIB_64=ON
# for a 64-bit build, in its own build directory:
#
#   cmake -S HCP65/Tests/TestLibc65 -B build/TestLibc6564 -G Ninja -DBASELIB_64=ON
#
#   cmake -S HCP65/Tests/TestLibc65 -B build/TestLibc65 -G Ninja -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build/TestLibc65
#   ctest --test-dir build/TestLibc65 -V
#
# Add -DBASELIB_ASAN=ON for AddressSanitizer and LeakSanitizer.
#
# The targets are read from TestLibc65.sln (codaphela_solution): the libraries its GCC-x64 rows build, each one's
# sources from its .vcxproj, so Visual Studio stays the source of truth.  Building re-runs the configure step when the
# solution or a project changes.  Test directories can contain .cpp files that are test data, so never glob *.cpp to
# find sources.

cmake_minimum_required(VERSION 3.24)
project(TestLibc65 CXX)


# This test is in the HCP65 repository (with Assembler65Lib and Libc65), which is beside CommonLibraries.
get_filename_component(HCP65_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
get_filename_component(CODAPHELA_ROOT "${HCP65_ROOT}/.." ABSOLUTE)
set(HCP65_LIBRARIES_DIR "${HCP65_ROOT}/Libraries")
set(COMMON_DIR "${CODAPHELA_ROOT}/CommonLibraries/Libraries")

include("${COMMON_DIR}/../CMake/Codaphela.cmake")

codaphela_options("${COMMON_DIR}" "${HCP65_LIBRARIES_DIR}")

codaphela_solution("${CMAKE_CURRENT_SOURCE_DIR}/TestLibc65.sln" TESTS TestLibc65)

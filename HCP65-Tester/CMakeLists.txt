# Linux build of the HCP65 Tester's empty window (BaseLib + StandardLib +
# SupportLib + InputLib + WindowLib + X11Lib + the tester's window sources).  The board and its COM port are Windows only and are left
# out.
#
# 64-bit only: there is no 32-bit libX11 installed.  Needs libx11-dev.
#
#   cmake -S HCP65/HCP65-Tester -B build/HCP65Tester -G Ninja -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build/HCP65Tester
#   cd HCP65/HCP65-Tester/HCP65Tester && ../../../build/HCP65Tester/HCP65Tester
#
# Add -DBASELIB_ASAN=ON for AddressSanitizer and LeakSanitizer.
#
# The targets are read from HCP65Tester.sln (codaphela_solution): the libraries its GCC-x64 rows build, each one's
# sources from its .vcxproj, so Visual Studio stays the source of truth.  Building re-runs the configure step when the
# solution or a project changes.

cmake_minimum_required(VERSION 3.24)
project(HCP65Tester CXX)


# The tester is in the HCP65 repository, which is beside CommonLibraries.
get_filename_component(CODAPHELA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(COMMON_DIR "${CODAPHELA_ROOT}/CommonLibraries/Libraries")

include("${COMMON_DIR}/../CMake/Codaphela.cmake")

codaphela_options(ONLY_64 "${COMMON_DIR}")

codaphela_solution("${CMAKE_CURRENT_SOURCE_DIR}/HCP65Tester.sln" PROGRAMS HCP65Tester)

# Linux build of the X65C64 Emulator: a 65C64 with memory, its console and the host file device (MicroprocessorLib's
# CFast65C64 on Assembler65Lib's CAsm65Memory), a console program debuggable through EmulatorDebugLib and ThreadLib's
# channels: --debug-shm <name> or --debug-tcp <port>, --debug-wait, --exit-on-detach.
#
#   cmake -S HCP65/X65C64-Emulator/X65C64Emulator -B build/X65C64Emulator -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build/X65C64Emulator
#   build/X65C64Emulator/X65C64Emulator program.bin
#
# Add -DBASELIB_ASAN=ON for AddressSanitizer and LeakSanitizer.
#
# The targets are read from X65C64Emulator.sln (codaphela_solution): the libraries its GCC-x64 rows build, each one's
# sources from its .vcxproj, so Visual Studio stays the source of truth.  Building re-runs the configure step when the
# solution or a project changes.

cmake_minimum_required(VERSION 3.24)
project(X65C64Emulator CXX)


# The emulator is in the HCP65 repository (with Assembler65Lib), which is beside CommonLibraries.
get_filename_component(HCP65_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
get_filename_component(CODAPHELA_ROOT "${HCP65_ROOT}/.." ABSOLUTE)
set(COMMON_DIR "${CODAPHELA_ROOT}/CommonLibraries/Libraries")

include("${COMMON_DIR}/../CMake/Codaphela.cmake")

codaphela_options(ONLY_64 "${COMMON_DIR}" "${HCP65_ROOT}/Libraries")

codaphela_solution("${CMAKE_CURRENT_SOURCE_DIR}/../X65C64Emulator.sln" PROGRAMS X65C64Emulator)

# Linux build of the HCP65 Emulator: the HCP65 (HCP65Lib on MicroprocessorLib's W65C816, debuggable through EmulatorDebugLib
# and ThreadLib's channels: --debug-shm <name> or --debug-tcp <port>, --debug-wait) in an X11 window (WindowLib +
# X11Lib), its sound board played through the window's sound mixer on PulseAudio (PulseAudioLib, which loads it at run
# time: only libpulse-simple.so.0 is needed).  HCP65EmulatorRef is the same emulator on WinRefLib's reference screen, drawn
# in memory with no display and no sound, for the IDE's checks to debug in (HCP65_IDE_EMULATOR names it).
#
# 64-bit only: there is no 32-bit libX11 installed.  Needs libx11-dev.
#
#   cmake -S HCP65/HCP65-Emulator/HCP65Emulator -B build/HCP65Emulator -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build/HCP65Emulator
#   cd HCP65/HCP65-Emulator/HCP65Emulator && ../../../build/HCP65Emulator/HCP65Emulator
#
# The ROM and SD card image it boots are built by TestHCP65Lib (TestHCP65Kernal) into TestHCP65Lib/Output; run it first.  HCP65_EMULATOR_TRACE=1 prints
# how far the HCP65 has got each second; HCP65_EMULATOR_SECONDS=n closes the window after n seconds.
#
# Add -DBASELIB_ASAN=ON for AddressSanitizer and LeakSanitizer.
#
# The targets are read from HCP65Emulator.sln (codaphela_solution): the libraries its GCC-x64 rows build, each one's
# sources from its .vcxproj, so Visual Studio stays the source of truth.  HCP65Emulator.vcxproj's configurations say
# which main each program has (its Reference configuration the reference screen's), and Reference's rows build
# WinRefLib and neither X11Lib nor PulseAudioLib.

cmake_minimum_required(VERSION 3.24)
project(HCP65Emulator CXX)


# The emulator is in the HCP65 repository (with Assembler65Lib and Libc65), which is beside CommonLibraries.
get_filename_component(EMULATOR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
get_filename_component(HCP65_ROOT "${EMULATOR_ROOT}/.." ABSOLUTE)
get_filename_component(CODAPHELA_ROOT "${HCP65_ROOT}/.." ABSOLUTE)
set(COMMON_DIR "${CODAPHELA_ROOT}/CommonLibraries/Libraries")

include("${COMMON_DIR}/../CMake/Codaphela.cmake")

codaphela_options(ONLY_64 "${COMMON_DIR}" "${HCP65_ROOT}/Libraries" "${EMULATOR_ROOT}")

codaphela_solution("${EMULATOR_ROOT}/HCP65Emulator.sln"
	PROGRAMS HCP65Emulator
	REFERENCE HCP65Emulator=HCP65EmulatorRef)

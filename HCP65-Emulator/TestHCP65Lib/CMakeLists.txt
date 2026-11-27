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
# Source lists are read from the .vcxproj files so Visual Studio stays the
# source of truth.  HCP65/Kernal is W65C816 code built by the tests, not
# by this file.

cmake_minimum_required(VERSION 3.19)
project(TestHCP65Lib CXX)


# The emulator is in the HCP65 repository (with Assembler65Lib and Libc65), which is beside CommonLibraries.
get_filename_component(HCP65_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
get_filename_component(CODAPHELA_ROOT "${HCP65_ROOT}/.." ABSOLUTE)
set(HCP65_LIBRARIES_DIR "${HCP65_ROOT}/Libraries")
set(COMMON_DIR "${CODAPHELA_ROOT}/CommonLibraries/Libraries")
set(BASELIB_DIR "${COMMON_DIR}/BaseLib")
set(TESTLIB_DIR "${COMMON_DIR}/TestLib")
set(STANDARDLIB_DIR "${COMMON_DIR}/StandardLib")
set(SUPPORTLIB_DIR "${COMMON_DIR}/SupportLib")
set(CPPCOMPILERLIB_DIR "${COMMON_DIR}/CppCompilerLib")
set(MICROPROCESSORLIB_DIR "${COMMON_DIR}/MicroprocessorLib")
set(THREADLIB_DIR "${COMMON_DIR}/ThreadLib")
set(EMULATORDEBUGLIB_DIR "${COMMON_DIR}/EmulatorDebugLib")
set(ASSEMBLER65LIB_DIR "${HCP65_LIBRARIES_DIR}/Assembler65Lib")
set(KICADLIB_DIR "${COMMON_DIR}/KicadLib")
set(DEVICEROMLIB_DIR "${HCP65_LIBRARIES_DIR}/DeviceRomLib")
set(HCP65LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../HCP65Lib")
set(TESTHCP65LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/TestHCP65Lib")

include("${COMMON_DIR}/../CMake/Codaphela.cmake")


vcxproj_sources(BASELIB_SOURCES "${BASELIB_DIR}/BaseLib.vcxproj")

vcxproj_sources(TESTLIB_SOURCES "${TESTLIB_DIR}/TestLib.vcxproj")

vcxproj_sources(STANDARDLIB_SOURCES "${STANDARDLIB_DIR}/StandardLib.vcxproj")

vcxproj_sources(SUPPORTLIB_SOURCES "${SUPPORTLIB_DIR}/SupportLib.vcxproj")

vcxproj_sources(CPPCOMPILERLIB_SOURCES "${CPPCOMPILERLIB_DIR}/CppCompilerLib.vcxproj")

vcxproj_sources(MICROPROCESSORLIB_SOURCES "${MICROPROCESSORLIB_DIR}/MicroprocessorLib.vcxproj")

vcxproj_sources(THREADLIB_SOURCES "${THREADLIB_DIR}/ThreadLib.vcxproj")

vcxproj_sources(EMULATORDEBUGLIB_SOURCES "${EMULATORDEBUGLIB_DIR}/EmulatorDebugLib.vcxproj")

vcxproj_sources(ASSEMBLER65LIB_SOURCES "${ASSEMBLER65LIB_DIR}/Assembler65Lib.vcxproj")

vcxproj_sources(KICADLIB_SOURCES "${KICADLIB_DIR}/KicadLib.vcxproj")

vcxproj_sources(DEVICEROMLIB_SOURCES "${DEVICEROMLIB_DIR}/DeviceRomLib.vcxproj")

vcxproj_sources(HCP65LIB_SOURCES "${HCP65LIB_DIR}/HCP65Lib.vcxproj")

vcxproj_sources(TESTHCP65LIB_SOURCES "${TESTHCP65LIB_DIR}/TestHCP65Lib.vcxproj")


codaphela_options("${COMMON_DIR}" "${HCP65_LIBRARIES_DIR}")


# CMutex uses pthreads on Linux.
find_package(Threads REQUIRED)

add_library(BaseLib STATIC ${BASELIB_SOURCES})
target_link_libraries(BaseLib PUBLIC baselib_options)
target_link_libraries(BaseLib PUBLIC Threads::Threads)

# Uses AVX intrinsics unconditionally; MSVC allows that without a flag, g++ does not.

add_library(TestLib STATIC ${TESTLIB_SOURCES})
target_link_libraries(TestLib PUBLIC BaseLib)

add_library(StandardLib STATIC ${STANDARDLIB_SOURCES})
target_link_libraries(StandardLib PUBLIC BaseLib)

# The Debug configurations of StandardLib (only) define DEBUG_POINTER.
target_compile_definitions(StandardLib PRIVATE $<$<CONFIG:Debug>:DEBUG_POINTER>)

add_library(SupportLib STATIC ${SUPPORTLIB_SOURCES})
target_link_libraries(SupportLib PUBLIC StandardLib BaseLib)

add_library(CppCompilerLib STATIC ${CPPCOMPILERLIB_SOURCES})
target_link_libraries(CppCompilerLib PUBLIC Assembler65Lib MicroprocessorLib StandardLib BaseLib)

add_library(MicroprocessorLib STATIC ${MICROPROCESSORLIB_SOURCES})
target_link_libraries(MicroprocessorLib PUBLIC StandardLib BaseLib)

add_library(Assembler65Lib STATIC ${ASSEMBLER65LIB_SOURCES})
target_link_libraries(Assembler65Lib PUBLIC MicroprocessorLib StandardLib BaseLib)

add_library(ThreadLib STATIC ${THREADLIB_SOURCES})
target_link_libraries(ThreadLib PUBLIC BaseLib)

add_library(EmulatorDebugLib STATIC ${EMULATORDEBUGLIB_SOURCES})
target_link_libraries(EmulatorDebugLib PUBLIC ThreadLib MicroprocessorLib StandardLib BaseLib)

add_library(KicadLib STATIC ${KICADLIB_SOURCES})
target_link_libraries(KicadLib PUBLIC BaseLib)

add_library(DeviceRomLib STATIC ${DEVICEROMLIB_SOURCES})
target_link_libraries(DeviceRomLib PUBLIC KicadLib BaseLib)

add_library(HCP65Lib STATIC ${HCP65LIB_SOURCES})
target_link_libraries(HCP65Lib PUBLIC DeviceRomLib EmulatorDebugLib Assembler65Lib MicroprocessorLib StandardLib BaseLib)

add_executable(TestHCP65Lib ${TESTHCP65LIB_SOURCES})
target_include_directories(TestHCP65Lib PRIVATE "${HCP65_ROOT}/HCP65-Emulator")
target_link_libraries(TestHCP65Lib PRIVATE TestLib HCP65Lib EmulatorDebugLib ThreadLib CppCompilerLib Assembler65Lib MicroprocessorLib SupportLib StandardLib BaseLib)

# The tests read their input (../../HCP65Emulator/Input ...) relative to the project directory,
# as Visual Studio does.
enable_testing()
add_test(NAME TestHCP65Lib COMMAND TestHCP65Lib WORKING_DIRECTORY "${TESTHCP65LIB_DIR}")

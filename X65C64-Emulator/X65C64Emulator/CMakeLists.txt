# Linux build of the X65C64 Emulator: a 65C64 with memory, its console and the host file device (MicroprocessorLib's
# CFast65C64 on Assembler65Lib's CAsm65Memory), a console program debuggable through EmulatorDebugLib and ThreadLib's
# channels: --debug-shm <name> or --debug-tcp <port>, --debug-wait, --exit-on-detach.
#
#   cmake -S HCP65/X65C64-Emulator/X65C64Emulator -B build/X65C64Emulator -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build/X65C64Emulator
#   build/X65C64Emulator/X65C64Emulator program.bin
#
# Add -DBASELIB_ASAN=ON for AddressSanitizer and LeakSanitizer.

cmake_minimum_required(VERSION 3.16)
project(X65C64Emulator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

option(BASELIB_ASAN "Build with AddressSanitizer and LeakSanitizer" OFF)

# The emulator is in the HCP65 repository (with Assembler65Lib), which is beside CommonLibraries.
get_filename_component(HCP65_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
get_filename_component(CODAPHELA_ROOT "${HCP65_ROOT}/.." ABSOLUTE)
set(COMMON_DIR "${CODAPHELA_ROOT}/CommonLibraries/Libraries")
set(EMULATOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

include("${CODAPHELA_ROOT}/CommonLibraries/CMake/VcxprojSources.cmake")
vcxproj_sources(BASELIB_SOURCES "${COMMON_DIR}/BaseLib/BaseLib.vcxproj")
vcxproj_sources(STANDARDLIB_SOURCES "${COMMON_DIR}/StandardLib/StandardLib.vcxproj")
vcxproj_sources(MICROPROCESSORLIB_SOURCES "${COMMON_DIR}/MicroprocessorLib/MicroprocessorLib.vcxproj")
vcxproj_sources(THREADLIB_SOURCES "${COMMON_DIR}/ThreadLib/ThreadLib.vcxproj")
vcxproj_sources(EMULATORDEBUGLIB_SOURCES "${COMMON_DIR}/EmulatorDebugLib/EmulatorDebugLib.vcxproj")
vcxproj_sources(ASSEMBLER65LIB_SOURCES "${HCP65_ROOT}/Libraries/Assembler65Lib/Assembler65Lib.vcxproj")
vcxproj_sources(EMULATOR_SOURCES "${EMULATOR_DIR}/X65C64Emulator.vcxproj")


# As HCP65Emulator's CMakeLists.txt says why for each warning and option.
add_library(baselib_options INTERFACE)
target_include_directories(baselib_options INTERFACE "${COMMON_DIR}" "${HCP65_ROOT}/Libraries")
target_compile_definitions(baselib_options INTERFACE
	LINUX_GNU
	$<$<CONFIG:Debug>:_DEBUG>
	$<$<CONFIG:Debug>:DEBUG>
	$<$<NOT:$<CONFIG:Debug>>:NDEBUG>
)
target_compile_options(baselib_options INTERFACE -m64 -fno-strict-aliasing -Wall -Wextra -Wno-ignored-attributes -Wno-write-strings -Wno-multichar -Wno-class-memaccess -Wno-overloaded-virtual -Wno-comment -Wno-unused-parameter -march=haswell)
target_link_options(baselib_options INTERFACE -m64)
if(BASELIB_ASAN)
	target_compile_options(baselib_options INTERFACE -fsanitize=address -fno-omit-frame-pointer)
	target_link_options(baselib_options INTERFACE -fsanitize=address)
endif()

find_package(Threads REQUIRED)

add_library(BaseLib STATIC ${BASELIB_SOURCES})
target_link_libraries(BaseLib PUBLIC baselib_options Threads::Threads)

add_library(StandardLib STATIC ${STANDARDLIB_SOURCES})
target_link_libraries(StandardLib PUBLIC BaseLib)

add_library(MicroprocessorLib STATIC ${MICROPROCESSORLIB_SOURCES})
target_link_libraries(MicroprocessorLib PUBLIC StandardLib BaseLib)

add_library(ThreadLib STATIC ${THREADLIB_SOURCES})
target_link_libraries(ThreadLib PUBLIC BaseLib)

add_library(EmulatorDebugLib STATIC ${EMULATORDEBUGLIB_SOURCES})
target_link_libraries(EmulatorDebugLib PUBLIC ThreadLib MicroprocessorLib StandardLib BaseLib)

add_library(Assembler65Lib STATIC ${ASSEMBLER65LIB_SOURCES})
target_link_libraries(Assembler65Lib PUBLIC MicroprocessorLib StandardLib BaseLib)

add_executable(X65C64Emulator ${EMULATOR_SOURCES})
target_link_libraries(X65C64Emulator PRIVATE EmulatorDebugLib Assembler65Lib ThreadLib MicroprocessorLib StandardLib BaseLib ${CMAKE_DL_LIBS})

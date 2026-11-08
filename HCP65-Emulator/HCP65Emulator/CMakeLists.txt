# Linux build of the HCP65 Emulator: the HCP65 (HCP65Lib on MicroprocessorLib's W65C816, debuggable through EmulatorDebugLib
# and ThreadLib's channels: --debug-shm <name> or --debug-tcp <port>, --debug-wait) in an X11 window (WindowLib +
# X11Lib), its sound board played through the window's sound mixer on PulseAudio (PulseAudioLib, which loads it at run
# time: only libpulse-simple.so.0 is needed).
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
# Library source lists are read from the .vcxproj files so Visual Studio stays
# the source of truth.

cmake_minimum_required(VERSION 3.16)
project(HCP65Emulator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

option(BASELIB_ASAN "Build with AddressSanitizer and LeakSanitizer" OFF)

get_filename_component(EMULATOR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
# The emulator is in the HCP65 repository (with Assembler65Lib and Libc65), which is beside CommonLibraries.
get_filename_component(HCP65_ROOT "${EMULATOR_ROOT}/.." ABSOLUTE)
get_filename_component(CODAPHELA_ROOT "${HCP65_ROOT}/.." ABSOLUTE)
set(COMMON_DIR "${CODAPHELA_ROOT}/commonlibraries/Libraries")
set(BASELIB_DIR "${COMMON_DIR}/BaseLib")
set(STANDARDLIB_DIR "${COMMON_DIR}/StandardLib")
set(SUPPORTLIB_DIR "${COMMON_DIR}/SupportLib")
set(RASTERLIB_DIR "${COMMON_DIR}/RasterLib")
set(INPUTLIB_DIR "${COMMON_DIR}/InputLib")
set(WINDOWLIB_DIR "${COMMON_DIR}/WindowLib")
set(X11LIB_DIR "${COMMON_DIR}/X11Lib")
set(PULSEAUDIOLIB_DIR "${COMMON_DIR}/PulseAudioLib")
set(MICROPROCESSORLIB_DIR "${COMMON_DIR}/MicroprocessorLib")
set(THREADLIB_DIR "${COMMON_DIR}/ThreadLib")
set(EMULATORDEBUGLIB_DIR "${COMMON_DIR}/EmulatorDebugLib")
set(ASSEMBLER65LIB_DIR "${HCP65_ROOT}/Libraries/Assembler65Lib")
set(KICADLIB_DIR "${COMMON_DIR}/KicadLib")
set(DEVICEROMLIB_DIR "${HCP65_ROOT}/Libraries/DeviceRomLib")
set(HCP65LIB_DIR "${EMULATOR_ROOT}/HCP65Lib")
set(EMULATOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}")


# Reads <ClCompile Include="..."/> entries from a .vcxproj and resolves them
# against the files on disk case-insensitively (Windows paths are not
# case-sensitive).  Files named in EXCLUDE (paths relative to the project,
# any case) are left out.
function(vcxproj_sources OUT_VAR VCXPROJ)
	cmake_parse_arguments(ARG "" "" "EXCLUDE" ${ARGN})
	get_filename_component(PROJ_DIR "${VCXPROJ}" DIRECTORY)

	# Re-run the configure step when Visual Studio adds or removes files.
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${VCXPROJ}")

	file(GLOB_RECURSE DISK_FILES RELATIVE "${PROJ_DIR}" "${PROJ_DIR}/*.cpp" "${PROJ_DIR}/*.c")
	foreach(DISK_FILE ${DISK_FILES})
		string(TOLOWER "${DISK_FILE}" KEY)
		set("DISK_${KEY}" "${DISK_FILE}")
	endforeach()

	set(EXCLUDED "")
	foreach(EXCLUDE_FILE ${ARG_EXCLUDE})
		string(TOLOWER "${EXCLUDE_FILE}" KEY)
		list(APPEND EXCLUDED "${KEY}")
	endforeach()

	file(STRINGS "${VCXPROJ}" LINES REGEX "<ClCompile Include=\"[^\"]+\"")
	set(SOURCES "")
	foreach(LINE ${LINES})
		string(REGEX REPLACE ".*<ClCompile Include=\"([^\"]+)\".*" "\\1" SOURCE "${LINE}")
		string(REPLACE "\\" "/" SOURCE "${SOURCE}")
		string(TOLOWER "${SOURCE}" KEY)
		if(KEY IN_LIST EXCLUDED)
			continue()
		endif()
		if(NOT DEFINED "DISK_${KEY}")
			message(FATAL_ERROR "${VCXPROJ} lists ${SOURCE} but it was not found on disk")
		endif()
		list(APPEND SOURCES "${PROJ_DIR}/${DISK_${KEY}}")
	endforeach()

	set(${OUT_VAR} ${SOURCES} PARENT_SCOPE)
endfunction()


vcxproj_sources(BASELIB_SOURCES "${BASELIB_DIR}/BaseLib.vcxproj")

vcxproj_sources(STANDARDLIB_SOURCES "${STANDARDLIB_DIR}/StandardLib.vcxproj")

vcxproj_sources(SUPPORTLIB_SOURCES "${SUPPORTLIB_DIR}/SupportLib.vcxproj")

vcxproj_sources(RASTERLIB_SOURCES "${RASTERLIB_DIR}/RasterLib.vcxproj")

vcxproj_sources(INPUTLIB_SOURCES "${INPUTLIB_DIR}/InputLib.vcxproj")

vcxproj_sources(WINDOWLIB_SOURCES "${WINDOWLIB_DIR}/WindowLib.vcxproj")

vcxproj_sources(X11LIB_SOURCES "${X11LIB_DIR}/X11Lib.vcxproj")

vcxproj_sources(PULSEAUDIOLIB_SOURCES "${PULSEAUDIOLIB_DIR}/PulseAudioLib.vcxproj")

vcxproj_sources(MICROPROCESSORLIB_SOURCES "${MICROPROCESSORLIB_DIR}/MicroprocessorLib.vcxproj")

vcxproj_sources(THREADLIB_SOURCES "${THREADLIB_DIR}/ThreadLib.vcxproj")

vcxproj_sources(EMULATORDEBUGLIB_SOURCES "${EMULATORDEBUGLIB_DIR}/EmulatorDebugLib.vcxproj")

vcxproj_sources(ASSEMBLER65LIB_SOURCES "${ASSEMBLER65LIB_DIR}/Assembler65Lib.vcxproj")

vcxproj_sources(KICADLIB_SOURCES "${KICADLIB_DIR}/KicadLib.vcxproj")

vcxproj_sources(DEVICEROMLIB_SOURCES "${DEVICEROMLIB_DIR}/DeviceRomLib.vcxproj")

vcxproj_sources(HCP65LIB_SOURCES "${HCP65LIB_DIR}/HCP65Lib.vcxproj")

# The emulator's sources but its WinMain.
vcxproj_sources(EMULATOR_SOURCES "${EMULATOR_DIR}/HCP65Emulator.vcxproj" EXCLUDE "HCP65EmulatorWindows.cpp")
list(APPEND EMULATOR_SOURCES "${EMULATOR_DIR}/HCP65EmulatorLinux.cpp")


# Matches the Visual Studio configurations, except BREAK_ON_ERROR and
# BREAK_ON_WARNING: on Linux BREAK() raises SIGTRAP, which kills the test run
# when no debugger is attached.
add_library(baselib_options INTERFACE)
target_include_directories(baselib_options INTERFACE "${COMMON_DIR}" "${HCP65_ROOT}/Libraries")
target_compile_definitions(baselib_options INTERFACE
	LINUX_GNU
	$<$<CONFIG:Debug>:_DEBUG>
	$<$<CONFIG:Debug>:DEBUG>
	$<$<NOT:$<CONFIG:Debug>>:NDEBUG>
)
# -fno-strict-aliasing: MSVC never optimises on the strict aliasing rule and the
# code relies on that (e.g. (CUnknown**)&pcTyped), so optimised builds break without it.
# -Wno-ignored-attributes: g++ drops ALIGN_64 when int64/uint64/float64 is a
# template argument.  Templates that store such a type by value use MSVC_ALIGN
# instead (see PrimitiveTypes.h), so the warning only reports the drop.
# -Wno-write-strings: string literals are passed as char* throughout; MSVC allows it.
# -Wno-multichar: chunk and file IDs are multi-character constants like 'ABCD'.
# -Wno-class-memaccess: the containers memcpy objects with constructors on purpose.
# -Wno-overloaded-virtual: subclasses overload virtuals with other argument types
# (e.g. Save(CObjectWriter*) hides CUnknown::Save(CFileWriter*)) on purpose.
# -Wno-comment: ASCII diagrams in comments end lines with a backslash.
# -Wno-unused-parameter: virtual overrides and callbacks often ignore parameters.
target_compile_options(baselib_options INTERFACE -m64 -fno-strict-aliasing -Wall -Wextra -Wno-ignored-attributes -Wno-write-strings -Wno-multichar -Wno-class-memaccess -Wno-overloaded-virtual -Wno-comment -Wno-unused-parameter)
target_link_options(baselib_options INTERFACE -m64)
if(BASELIB_ASAN)
	target_compile_options(baselib_options INTERFACE -fsanitize=address -fno-omit-frame-pointer)
	target_link_options(baselib_options INTERFACE -fsanitize=address)
endif()


# CMutex uses pthreads on Linux.
find_package(Threads REQUIRED)
find_package(X11 REQUIRED)

add_library(BaseLib STATIC ${BASELIB_SOURCES})
target_link_libraries(BaseLib PUBLIC baselib_options)
target_link_libraries(BaseLib PUBLIC Threads::Threads)

add_library(StandardLib STATIC ${STANDARDLIB_SOURCES})
target_link_libraries(StandardLib PUBLIC BaseLib)

add_library(SupportLib STATIC ${SUPPORTLIB_SOURCES})
target_link_libraries(SupportLib PUBLIC StandardLib BaseLib)

add_library(RasterLib STATIC ${RASTERLIB_SOURCES})
target_link_libraries(RasterLib PUBLIC SupportLib StandardLib BaseLib)

add_library(InputLib STATIC ${INPUTLIB_SOURCES})
target_link_libraries(InputLib PUBLIC StandardLib BaseLib)

add_library(WindowLib STATIC ${WINDOWLIB_SOURCES})
target_link_libraries(WindowLib PUBLIC InputLib SupportLib StandardLib BaseLib)

add_library(X11Lib STATIC ${X11LIB_SOURCES})
target_link_libraries(X11Lib PUBLIC WindowLib InputLib SupportLib StandardLib BaseLib X11::X11 X11::Xext)

add_library(PulseAudioLib STATIC ${PULSEAUDIOLIB_SOURCES})
target_link_libraries(PulseAudioLib PUBLIC WindowLib SupportLib StandardLib BaseLib ${CMAKE_DL_LIBS})

add_library(MicroprocessorLib STATIC ${MICROPROCESSORLIB_SOURCES})
target_link_libraries(MicroprocessorLib PUBLIC StandardLib BaseLib)

add_library(ThreadLib STATIC ${THREADLIB_SOURCES})
target_link_libraries(ThreadLib PUBLIC BaseLib)

add_library(EmulatorDebugLib STATIC ${EMULATORDEBUGLIB_SOURCES})
target_link_libraries(EmulatorDebugLib PUBLIC ThreadLib MicroprocessorLib StandardLib BaseLib)

add_library(Assembler65Lib STATIC ${ASSEMBLER65LIB_SOURCES})
target_link_libraries(Assembler65Lib PUBLIC MicroprocessorLib StandardLib BaseLib)

add_library(KicadLib STATIC ${KICADLIB_SOURCES})
target_link_libraries(KicadLib PUBLIC BaseLib)

add_library(DeviceRomLib STATIC ${DEVICEROMLIB_SOURCES})
target_link_libraries(DeviceRomLib PUBLIC KicadLib BaseLib)

add_library(HCP65Lib STATIC ${HCP65LIB_SOURCES})
target_include_directories(HCP65Lib PUBLIC "${EMULATOR_ROOT}")
target_link_libraries(HCP65Lib PUBLIC DeviceRomLib EmulatorDebugLib Assembler65Lib MicroprocessorLib StandardLib BaseLib)

add_executable(HCP65Emulator ${EMULATOR_SOURCES})
target_include_directories(HCP65Emulator PRIVATE "${EMULATOR_DIR}")
target_link_libraries(HCP65Emulator PRIVATE HCP65Lib EmulatorDebugLib Assembler65Lib ThreadLib X11Lib PulseAudioLib WindowLib InputLib RasterLib SupportLib MicroprocessorLib StandardLib BaseLib ${CMAKE_DL_LIBS})

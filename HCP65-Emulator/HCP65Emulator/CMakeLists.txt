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
# Library source lists are read from the .vcxproj files so Visual Studio stays
# the source of truth.

cmake_minimum_required(VERSION 3.19)
project(HCP65Emulator CXX)


get_filename_component(EMULATOR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
# The emulator is in the HCP65 repository (with Assembler65Lib and Libc65), which is beside CommonLibraries.
get_filename_component(HCP65_ROOT "${EMULATOR_ROOT}/.." ABSOLUTE)
get_filename_component(CODAPHELA_ROOT "${HCP65_ROOT}/.." ABSOLUTE)
set(COMMON_DIR "${CODAPHELA_ROOT}/CommonLibraries/Libraries")
set(BASELIB_DIR "${COMMON_DIR}/BaseLib")
set(STANDARDLIB_DIR "${COMMON_DIR}/StandardLib")
set(SUPPORTLIB_DIR "${COMMON_DIR}/SupportLib")
set(RASTERLIB_DIR "${COMMON_DIR}/RasterLib")
set(RENDERLIB_DIR "${COMMON_DIR}/RenderLib")
set(RASTERRENDERLIB_DIR "${COMMON_DIR}/RasterRenderLib")
set(INPUTLIB_DIR "${COMMON_DIR}/InputLib")
set(WINDOWLIB_DIR "${COMMON_DIR}/WindowLib")
set(X11LIB_DIR "${COMMON_DIR}/X11Lib")
set(WINREFLIB_DIR "${COMMON_DIR}/WinRefLib")
set(PULSEAUDIOLIB_DIR "${COMMON_DIR}/PulseAudioLib")
set(MICROPROCESSORLIB_DIR "${COMMON_DIR}/MicroprocessorLib")
set(THREADLIB_DIR "${COMMON_DIR}/ThreadLib")
set(EMULATORDEBUGLIB_DIR "${COMMON_DIR}/EmulatorDebugLib")
set(ASSEMBLER65LIB_DIR "${HCP65_ROOT}/Libraries/Assembler65Lib")
set(KICADLIB_DIR "${COMMON_DIR}/KicadLib")
set(DEVICEROMLIB_DIR "${HCP65_ROOT}/Libraries/DeviceRomLib")
set(HCP65LIB_DIR "${EMULATOR_ROOT}/HCP65Lib")
set(EMULATOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

include("${COMMON_DIR}/../CMake/Codaphela.cmake")

vcxproj_sources(BASELIB_SOURCES "${BASELIB_DIR}/BaseLib.vcxproj")

vcxproj_sources(STANDARDLIB_SOURCES "${STANDARDLIB_DIR}/StandardLib.vcxproj")

vcxproj_sources(SUPPORTLIB_SOURCES "${SUPPORTLIB_DIR}/SupportLib.vcxproj")

vcxproj_sources(RASTERLIB_SOURCES "${RASTERLIB_DIR}/RasterLib.vcxproj")
vcxproj_sources(RENDERLIB_SOURCES "${RENDERLIB_DIR}/RenderLib.vcxproj")
vcxproj_sources(RASTERRENDERLIB_SOURCES "${RASTERRENDERLIB_DIR}/RasterRenderLib.vcxproj")

vcxproj_sources(INPUTLIB_SOURCES "${INPUTLIB_DIR}/InputLib.vcxproj")

vcxproj_sources(WINDOWLIB_SOURCES "${WINDOWLIB_DIR}/WindowLib.vcxproj")

vcxproj_sources(X11LIB_SOURCES "${X11LIB_DIR}/X11Lib.vcxproj")

vcxproj_sources(WINREFLIB_SOURCES "${WINREFLIB_DIR}/WinRefLib.vcxproj")

vcxproj_sources(PULSEAUDIOLIB_SOURCES "${PULSEAUDIOLIB_DIR}/PulseAudioLib.vcxproj")

vcxproj_sources(MICROPROCESSORLIB_SOURCES "${MICROPROCESSORLIB_DIR}/MicroprocessorLib.vcxproj")

vcxproj_sources(THREADLIB_SOURCES "${THREADLIB_DIR}/ThreadLib.vcxproj")

vcxproj_sources(EMULATORDEBUGLIB_SOURCES "${EMULATORDEBUGLIB_DIR}/EmulatorDebugLib.vcxproj")

vcxproj_sources(ASSEMBLER65LIB_SOURCES "${ASSEMBLER65LIB_DIR}/Assembler65Lib.vcxproj")

vcxproj_sources(KICADLIB_SOURCES "${KICADLIB_DIR}/KicadLib.vcxproj")

vcxproj_sources(DEVICEROMLIB_SOURCES "${DEVICEROMLIB_DIR}/DeviceRomLib.vcxproj")

vcxproj_sources(HCP65LIB_SOURCES "${HCP65LIB_DIR}/HCP65Lib.vcxproj")

# The emulator's sources but its mains, in a library the X11 and the reference screen's mains share.
vcxproj_sources(EMULATOR_SOURCES "${EMULATOR_DIR}/HCP65Emulator.vcxproj" EXCLUDE "HCP65Emulator_Windows.cpp" "HCP65Emulator_Linux.cpp")


codaphela_options(ONLY_64 "${COMMON_DIR}" "${HCP65_ROOT}/Libraries")


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
target_link_libraries(RasterLib PUBLIC StandardLib BaseLib)

add_library(RenderLib STATIC ${RENDERLIB_SOURCES})
target_link_libraries(RenderLib PUBLIC SupportLib StandardLib BaseLib)

add_library(RasterRenderLib STATIC ${RASTERRENDERLIB_SOURCES})
target_link_libraries(RasterRenderLib PUBLIC RenderLib RasterLib SupportLib StandardLib BaseLib)

add_library(InputLib STATIC ${INPUTLIB_SOURCES})
target_link_libraries(InputLib PUBLIC StandardLib BaseLib)

add_library(WindowLib STATIC ${WINDOWLIB_SOURCES})
target_link_libraries(WindowLib PUBLIC InputLib SupportLib StandardLib BaseLib)

add_library(WinRefLib STATIC ${WINREFLIB_SOURCES})
target_link_libraries(WinRefLib PUBLIC WindowLib InputLib SupportLib StandardLib BaseLib)

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

add_library(HCP65EmulatorLib STATIC ${EMULATOR_SOURCES})
target_include_directories(HCP65EmulatorLib PUBLIC "${EMULATOR_DIR}")
target_link_libraries(HCP65EmulatorLib PUBLIC HCP65Lib EmulatorDebugLib Assembler65Lib ThreadLib WindowLib InputLib RasterRenderLib RenderLib RasterLib SupportLib MicroprocessorLib StandardLib BaseLib ${CMAKE_DL_LIBS})

add_executable(HCP65Emulator "${EMULATOR_DIR}/HCP65Emulator_Linux.cpp")
target_link_libraries(HCP65Emulator PRIVATE HCP65EmulatorLib X11Lib PulseAudioLib)

add_executable(HCP65EmulatorRef "${EMULATOR_DIR}/HCP65EmulatorRef.cpp")
target_link_libraries(HCP65EmulatorRef PRIVATE HCP65EmulatorLib WinRefLib)

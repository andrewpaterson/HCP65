# Linux build of DeviceROM, the device ROM tool (BaseLib + DeviceRomLib + DeviceROM).
#
# Builds 32-bit (-m32, needs g++-multilib) by default.  Add -DBASELIB_64=ON
# for a 64-bit build, in its own build directory:
#
#   cmake -S HCP65/DeviceROM -B build/DeviceROM -G Ninja -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build/DeviceROM
#
# Add -DBASELIB_ASAN=ON for AddressSanitizer and LeakSanitizer.
#
# Source lists are read from the .vcxproj files so Visual Studio stays the
# source of truth.  Building re-runs the configure step automatically when a
# .vcxproj changes.

cmake_minimum_required(VERSION 3.19)
project(DeviceROM CXX)


# The tool is in the HCP65 repository (with DeviceRomLib), which is beside CommonLibraries.
get_filename_component(HCP65_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
get_filename_component(CODAPHELA_ROOT "${HCP65_ROOT}/.." ABSOLUTE)
set(HCP65_LIBRARIES_DIR "${HCP65_ROOT}/Libraries")
set(COMMON_DIR "${CODAPHELA_ROOT}/CommonLibraries/Libraries")
set(BASELIB_DIR "${COMMON_DIR}/BaseLib")
set(KICADLIB_DIR "${COMMON_DIR}/KicadLib")
set(DEVICEROMLIB_DIR "${HCP65_LIBRARIES_DIR}/DeviceRomLib")
set(DEVICEROM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/DeviceROM")

include("${COMMON_DIR}/../CMake/Codaphela.cmake")


vcxproj_sources(BASELIB_SOURCES "${BASELIB_DIR}/BaseLib.vcxproj")

vcxproj_sources(KICADLIB_SOURCES "${KICADLIB_DIR}/KicadLib.vcxproj")

vcxproj_sources(DEVICEROMLIB_SOURCES "${DEVICEROMLIB_DIR}/DeviceRomLib.vcxproj")

vcxproj_sources(DEVICEROM_SOURCES "${DEVICEROM_DIR}/DeviceROM.vcxproj")


codaphela_options("${COMMON_DIR}" "${HCP65_LIBRARIES_DIR}")


# CMutex uses pthreads on Linux.
find_package(Threads REQUIRED)

add_library(BaseLib STATIC ${BASELIB_SOURCES})
target_link_libraries(BaseLib PUBLIC baselib_options)
target_link_libraries(BaseLib PUBLIC Threads::Threads)

add_library(KicadLib STATIC ${KICADLIB_SOURCES})
target_link_libraries(KicadLib PUBLIC BaseLib)

add_library(DeviceRomLib STATIC ${DEVICEROMLIB_SOURCES})
target_link_libraries(DeviceRomLib PUBLIC KicadLib BaseLib)

add_executable(DeviceROM ${DEVICEROM_SOURCES})
target_link_libraries(DeviceROM PRIVATE DeviceRomLib BaseLib)

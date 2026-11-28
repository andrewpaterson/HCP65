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
# The targets are read from DeviceROM.sln (codaphela_solution): the libraries its GCC-x64 rows build, each one's
# sources from its .vcxproj, so Visual Studio stays the source of truth.  Building re-runs the configure step when the
# solution or a project changes.

cmake_minimum_required(VERSION 3.24)
project(DeviceROM CXX)


# The tool is in the HCP65 repository (with DeviceRomLib), which is beside CommonLibraries.
get_filename_component(HCP65_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
get_filename_component(CODAPHELA_ROOT "${HCP65_ROOT}/.." ABSOLUTE)
set(HCP65_LIBRARIES_DIR "${HCP65_ROOT}/Libraries")
set(COMMON_DIR "${CODAPHELA_ROOT}/CommonLibraries/Libraries")

include("${COMMON_DIR}/../CMake/Codaphela.cmake")

codaphela_options("${COMMON_DIR}" "${HCP65_LIBRARIES_DIR}")

codaphela_solution("${CMAKE_CURRENT_SOURCE_DIR}/DeviceROM.sln" PROGRAMS DeviceROM)

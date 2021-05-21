#
# project: hex_weekend
#
cmake_minimum_required(VERSION 3.14)
project(hex_weekend)
set(CMAKE_C_STANDARD 99)
if (CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
    set(CMAKE_CXX_STANDARD 17)
else()
    set(CMAKE_CXX_STANDARD 14)
endif()

if (SOKOL_USE_WGPU_DAWN)
    set(USE_DAWN_SDK ON)
endif()
# include the fips main cmake file
get_filename_component(FIPS_ROOT_DIR "../fips" ABSOLUTE)
include("${FIPS_ROOT_DIR}/cmake/fips.cmake")

fips_setup()
 fips_add_subdirectory(src)
 fips_add_subdirectory(sapp)
 fips_add_subdirectory(html5)
 fips_add_subdirectory(wgpu)
fips_finish()



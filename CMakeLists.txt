cmake_minimum_required(VERSION 3.10)
set(CMAKE_C_COMPILER clang)
project (XR_Test LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options(-std=c99 -Wall -Wextra -Wno-unused-parameter)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)

add_executable(xr src/main.c src/glad/gl.c)
target_include_directories(xr PRIVATE include ${GLIB_INCLUDE_DIRS})
target_link_libraries(xr PRIVATE glfw openxr_loader EGL ${GLIB_LIBRARIES})

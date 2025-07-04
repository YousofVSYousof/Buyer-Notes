cmake_minimum_required(VERSION 3.10)
project(buyer_notes)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_BUILD_TYPE Release)
add_compile_options(-Ofast -march=native)

set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)

add_executable(main main.cpp nuklear.c)
target_link_libraries(main glfw OpenGL::GL sqlite3)

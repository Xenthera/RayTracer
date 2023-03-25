cmake_minimum_required(VERSION 3.25)
project(raytracer)

set(CMAKE_CXX_STANDARD 14)


link_directories("/opt/homebrew/lib")
include_directories("/opt/homebrew/include")

find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(raytracer
        Sources/main.cpp
        spxe.h)

target_link_libraries(raytracer ${OPENGL_gl_LIBRARY} glfw)

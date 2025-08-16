cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(ShadersToCharArray)

if (MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-missing-field-initializers)
endif()

add_executable(${PROJECT_NAME} main.cpp)

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ShadersToCharArray> ${CMAKE_SOURCE_DIR}/resources/shaders
    COMMENT "Copy ShadersToCharArray to shaders"
)

cmake_minimum_required(VERSION 3.16)
project(emojilang CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)
find_package(LLVM REQUIRED CONFIG)

add_definitions(${LLVM_DEFINITIONS})

BISON_TARGET(EmojiBison src/parser.y ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp
             DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.h)
FLEX_TARGET(EmojiFlex src/lexer.l ${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp)
ADD_FLEX_BISON_DEPENDENCY(EmojiFlex EmojiBison)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LLVM_INCLUDE_DIRS}
)

add_executable(emojilang
    ${BISON_EmojiBison_OUTPUTS}
    ${FLEX_EmojiFlex_OUTPUTS}
    src/interp.cpp
    src/codegen.cpp
    src/main.cpp
)

target_link_libraries(emojilang
    LLVM
)

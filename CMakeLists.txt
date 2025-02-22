cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Config
set(LUAU_DIR "./lib/luau")
option(DEBUG_ASAN "Debug mode with address sanitizer" OFF)

project(
    lbuild 
    VERSION 0.0.1
    LANGUAGES CXX
)

if(DEBUG_ASAN)
    add_compile_options(-fsanitize=address -g)
    add_link_options(-fsanitize=address -g)
endif()

add_executable(
    ${PROJECT_NAME} 
    src/main.cpp 
    include/main.h

    src/luau_executor.cpp
    include/luau_executor.h
    
    src/lbuild_util.cpp
    include/lbuild_util.h

    src/lbuild_target.cpp
    include/lbuild_target.h
)

find_library(LUAU_VM Luau.VM "${LUAU_DIR}")
find_library(LUAU_CMP Luau.Compiler "${LUAU_DIR}")
find_library(LUAU_CLI Luau.CLI.lib "${LUAU_DIR}")
find_library(LUAU_AST Luau.Ast "${LUAU_DIR}")

if (NOT TARGET LUAU_VM OR NOT TARGET LUAU_CMP OR NOT TARGET LUAU_CLI OR NOT TARGET LUAU_AST)
    execute_process(COMMAND cmake -S ${LUAU_DIR} -B ${LUAU_DIR} -DCMAKE_BUILD_TYPE=Release)
    execute_process(COMMAND cmake --build ${LUAU_DIR} --config Release --target Luau.VM Luau.Compiler Luau.CLI.lib Luau.Ast)

    find_library(LUAU_VM Luau.VM "${LUAU_DIR}")
    find_library(LUAU_CMP Luau.Compiler "${LUAU_DIR}")
    find_library(LUAU_CLI Luau.CLI.lib "${LUAU_DIR}")
    find_library(LUAU_AST Luau.Ast "${LUAU_DIR}")
endif()

target_include_directories(
    ${PROJECT_NAME} 
    PRIVATE 
    ./include 
    "${LUAU_DIR}/VM/include"
    "${LUAU_DIR}/Compiler/include"
)

target_link_libraries(${PROJECT_NAME} PUBLIC ${LUAU_VM} ${LUAU_CMP} ${LUAU_AST} ${LUAU_CLI})
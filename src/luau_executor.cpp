#include <string>
#include <filesystem>

#include <stdio.h>
#include <stdlib.h>

#include "luau_executor.h"
#include "lua.h"
#include "luacode.h"

#define LINE_BUFF 2048

using namespace std;

int luau_exec::luau_dofile(lua_State* L, char* src_dir){
    // Manipulate the file a bit too
    filesystem::path filePath = src_dir;
    string filename = filePath.stem();
    string extension = filePath.extension();

    // Open the file
    FILE* src_file = fopen(src_dir, "r");
    if (src_file == NULL){
        perror("Unable to open source file");
    }

    // Get the size of the file and allocate an entire buffer for it
    fseek(src_file, 0, SEEK_END);
    unsigned long block_size = ftell(src_file); // + 1 for the null byte
    rewind(src_file);

    char* file_buff = (char*) malloc(sizeof(char) * (block_size + 1));
    if (file_buff == NULL){
        perror("Unable to allocate memory to consume luau file");
        fclose(src_file);
        exit(1);
    }

    if (fread(file_buff, 1, block_size, src_file) != (block_size)){
        fprintf(stderr, "Failed to read file %s: Mismatch between expected block size and actual size", filePath.c_str());
        fclose(src_file);
        free(file_buff);
        exit(1);
    }
    file_buff[block_size] = '\0';

    size_t bytecode_len = 0;
    char* bytecode = luau_compile(file_buff, block_size, NULL, &bytecode_len);
    
    //for (size_t i = 0; i < bytecode_len; i++){
    //    printf("0x%.2x\n", bytecode[i]);
    //}

    int ret_val = 
        luau_load(L, filePath.filename().c_str(), bytecode, bytecode_len, 0) ||
        lua_pcall(L, 0, LUA_MULTRET, 0);

    free(bytecode);
    free(file_buff);

    if (fclose(src_file)){
        perror("Unable to close file");
    }

    return ret_val;
}
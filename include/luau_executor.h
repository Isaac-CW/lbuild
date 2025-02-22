#ifndef LEXEC_H
#define LEXEC_H

#include "lua.h"
#include <string>

namespace luau_exec {
    extern int luau_dofile(lua_State* L, char* file_dir);
}

#endif
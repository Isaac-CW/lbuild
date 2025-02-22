
#include "lua.h"
#include "luacode.h"

#include <string>

namespace LBUILD {
    extern void init_lua(lua_State* l);
    extern void cleanup();

    extern void setup_dependencies();
    extern void run_task(lua_State* l, std::string task_name);
    extern void lua_stackDump(lua_State* l);
}
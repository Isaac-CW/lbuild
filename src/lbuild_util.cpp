#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <exception>
#include <memory>
#include <map>
#include <filesystem>

#include "lbuild_util.h"
#include "lbuild_target.h"

#include "lua.h"
#include "lualib.h"

using namespace LBUILD;
using namespace std;

struct lbuild_task_udata {
    unique_ptr<string> task_name;
};

static void show_lua_type (lua_State* L, int index){
    int t = lua_type(L, index);
    switch(t){
        case LUA_TSTRING:{  /* strings */
            printf("`%s'", lua_tostring(L, index));
            break;
        }

        case LUA_TBOOLEAN:{  /* booleans */
            printf(lua_toboolean(L, index) ? "true" : "false");
            break;
        }

        case LUA_TNUMBER:{  /* numbers */
            printf("%g", lua_tonumber(L, index));
            break;
        }

        /*case LUA_TTABLE:{
            lua_pushnil(L);
            printf("{");
            while (lua_next(L, -2) != 0) {
                // Thanks lua docs on lua_next for letting me blatantly pilfer this block
                show_lua_type(L, -1);

                lua_pop(L, 1);
            }
            printf("}");

            break;
        }*/

        default:{  /* other values */
            printf("%s", lua_typename(L, t));
            break;
        }
    }
}

void LBUILD::lua_stackDump (lua_State *L) {
    int i;
    int top = lua_gettop(L);
    for (i = 1; i <= top; i++) {  /* repeat for each level */
        //int t = lua_type(L, i);
        printf("%d: ", i);

        show_lua_type(L, i);
        
        /*switch (t) {

            case LUA_TSTRING:{  // strings
                printf("`%s'", lua_tostring(L, i));
                break;
            }

            case LUA_TBOOLEAN:{ // booleans
                printf(lua_toboolean(L, i) ? "true" : "false");
                break;
            }

            case LUA_TNUMBER:{ // numbers
                printf("%g", lua_tonumber(L, i));
                break;
            }

            default:  // others
            printf("%s", lua_typename(L, t));
            break;

        }*/
        printf(",  ");  /* put a separator */
    }
    printf("\n");  /* end the listing */
}

static unordered_map<string, unique_ptr<vector< string>>> depends_buffer;

static int lbuild_task_dependsOn(lua_State* l){
    int argn = lua_gettop(l);

    int t = lua_type(l , 1);
    if (t != LUA_TUSERDATA){
        luaL_error(l, "Invalid value for self parameter. Expected Userdata, got %s. Did you forget to use \":\" when calling dependsOn?\n", lua_typename(l, t));
        return 0;
    }
    struct lbuild_task_udata* self = (struct lbuild_task_udata*) lua_touserdata(l, 1);
    // The varargs are all the dependencies this task depends on
    unique_ptr<vector<string>> s = make_unique<vector<string>>();
    for (size_t i = 2; i <= argn; i++){
        const char* str = luaL_checkstring(l, i);
        s->push_back(string(str));
    }

    if (s->size() > 0){
        // Defer setting all the dependencies until after we've set up all our builds
        depends_buffer.insert_or_assign(*self->task_name, move(s));
    }
    // Push the userdata back to the top of the stack
    lua_pushvalue(l, 1);
    return 1;
}

static int lbuild_task_run(lua_State* l){
    if (!lua_isuserdata(l, 1)){
        luaL_error(l, "Invalid value for self parameter: Did you forget to use \":\" when calling run?\n");
        return 0;
    }
    struct lbuild_task_udata* self = (struct lbuild_task_udata*) lua_touserdata(l, 1);
    // Argument 2 is the callback we run
    int t = lua_type(l, -1);
    if (t != LUA_TFUNCTION){
        luaL_error(l, "Invalid value for function parameter. Expected function, got %s\n", lua_typename(l, t));
        return 0;
    }
    // Push it to _X since we enforce that all tasks have unique names, we can simply push it and be done with it
    lua_getglobal(l, "_X");
    lua_pushstring(l, self->task_name->c_str());
    lua_gettable(l, -2); // Get the container that stores the userdata
    if (lua_isnil(l, -1)){
        luaL_error(l, "No container is stored in _X for keyname %s\n", self->task_name->c_str());
        // Push the userdata back to the top of the stack
        lua_pushvalue(l, 1);
        return 1;
    }

    lua_pushstring(l, "f");
    lua_pushvalue(l, 2);
    lua_settable(l, -3);

    // Push the userdata back to the top of the stack
    lua_pushvalue(l, 1);
    return 1;
}

static int lbuild_inst_exec(lua_State* l){
    int t = lua_type(l, -2);
    if (t != LUA_TUSERDATA){
        luaL_error(l, "Invalid value for argument 1: Must provide task object\n");
        return 0;
    }
    struct lbuild_task_udata* self = (struct lbuild_task_udata*) lua_touserdata(l, -2);

    int arg_type = lua_type(l, -1);
    vector<string> args;
    switch (arg_type){
        case LUA_TSTRING:{
            const char* input_str = luaL_checkstring(l, -1);
            istringstream str_strm(input_str);
            string output;
            // Split the string by [^\s"']+|"([^"]*)"|'([^']*)' 
            while (str_strm >> quoted(output)){
                args.push_back(output);
                //printf("%d: %s\n", args.size(), output.c_str());
            }

            break;
        }

        case LUA_TTABLE:{
            int t = lua_gettop(l);
            lua_pushnil(l);
            while (lua_next(l, t) != 0) {
                // Thanks lua docs on lua_next for letting me blatantly pilfer this block
                const char* exec_line = luaL_checkstring(l, -1);
                args.push_back(exec_line);

                lua_pop(l, 1);
            }

            break;
        }
        
        default:{
            luaL_error(l, "Expected table or string for execv calls, got type %s\n", lua_typename(l, arg_type));
            break;
        }
    }
    vector<char*> as_chars;

    transform(args.begin(), args.end(), back_inserter(as_chars), [](const string& s){
        char* str = new char[s.size() + 1];
        strcpy(str, s.c_str());

        return str;
    });
    as_chars.push_back(NULL);

    pid_t exec_process = fork();
    if (exec_process < 0){
        fprintf(stderr, "Unable to fork process\n");
    } else if (exec_process > 0) {
        // Parent process
        int status = 0;
        waitpid(exec_process, &status, 0);
        //printf("%s completed\n", as_chars.at(0));
    } else {
        // Child process
        bool first = true;
        for (auto v : as_chars){
            if (!first){
                printf(" ");
            } else {first = false;}
            //printf("%s", v);
        }
        //printf("\n");
        int res = execvp(as_chars.at(0), as_chars.data());

        exit(res);
    }

    for (size_t i = 0; i < (as_chars.size() - 1); i++){
        delete as_chars.at(i);
    }
}

static int lbuild_create_lua_obj(lua_State* l){
    const char* task_name = luaL_checkstring(l, 1);
    // Create the lbuild_target object
    try {
        BuildTarget::create_target(string(task_name));
    } catch (invalid_argument e){
        luaL_error(l, "Cannot create build target %s: target already exists\n", task_name);
        return 0;
    }
    // Allocate userdata purely to hold onto the task name
    size_t len = strlen(task_name);

    struct lbuild_task_udata* as_udata = (struct lbuild_task_udata*) lua_newuserdata(l, sizeof(struct lbuild_task_udata));
    as_udata->task_name = make_unique<string>(task_name); // I'm not sure if this leaks memory

    luaL_getmetatable(l, "taskmt");
    lua_setmetatable(l, -2);

    //printf("Created task %s\n", as_udata->task_name->c_str());

    // Store it at _X
    lua_getglobal(l, "_X");
    lua_pushstring(l, task_name);
    lua_createtable(l, 0, 2);
    lua_settable(l, -3);
    // We need to re-fetch the value from _X
    lua_pushstring(l, task_name);
    lua_gettable(l, -2);

    lua_pushstring(l, "u");
    lua_pushvalue(l, 2);
    lua_settable(l, -3);

    // Push the userdata back on
    lua_pushvalue(l, 2);

    return 1;
}

static int lbuild_run_task(lua_State* l){
    if (!lua_isuserdata(l, 1)){
        luaL_error(l, "Invalid value for self parameter: Did you forget to use \":\" when calling run?\n");
        return 0;
    }
    struct lbuild_task_udata* self = (struct lbuild_task_udata*) lua_touserdata(l, 1);
    auto self_obj = BuildTarget::get_target(*(self->task_name));
    if (self_obj == NULL){
        luaL_error(l, "No task with name %s exists\n", self->task_name->c_str());
        return 0;
    }

    // Argument 2 is the task we want to run
    int t = lua_type(l, -1);
    if (t != LUA_TSTRING){
        luaL_error(l, "Invalid value for task parameter. Expected string, got %s\n", lua_typename(l, t));
        return 0;
    }
    const char* target_task = luaL_checkstring(l, -1);
    auto task = BuildTarget::get_target(string(target_task));
    if (task == NULL){
        luaL_error(l, "No task with name %s exists\n", target_task);
        return 0;
    }

    // If the target task has a dependency on the self_obj then error
    if (BuildTarget::has_circular_dependency(self_obj, task)){
        luaL_error(l, "Task \"%s\" has task \"%s\" as a dependency\n", target_task, self->task_name->c_str());
        return 0;
    }

    task->run(l);
}

static int lbuild_get_files(lua_State* l){

    lua_newtable(l);
    size_t t = lua_gettop(l);

    int index = 1;
    for (size_t i = 1; i < t; i++){
        const char* dir = luaL_checkstring(l, i);
        filesystem::path file_path(dir);
        for (auto &k : filesystem::directory_iterator(file_path)){
            // Prep the lbuild.file type
            lua_pushinteger(l, index);
            lua_createtable(l, 0, 2);
            lua_settable(l, t);

            // Put the table back on top of the stack
            lua_pushinteger(l, index);
            lua_gettable(l, t);

            filesystem::path child_path = k.path();
            string file_name = child_path.stem();
            string ext = child_path.extension();
            string path = child_path.relative_path();
            // First filename
            lua_pushstring(l, "filename");
            lua_pushstring(l, file_name.c_str());
            lua_settable(l, -3);
            // Then extension
            lua_pushstring(l, "extension");
            lua_pushstring(l, ext.c_str());
            lua_settable(l, -3);
            // Then file path
            lua_pushstring(l, "path");
            lua_pushstring(l, path.c_str());
            lua_settable(l, -3);

            //printf("file: %s%s, path: %s\n", file_name.c_str(), ext.c_str(), child_path.relative_path().c_str());

            index += 1;
        }
    }
    // Put the main table back onto the top of the stack
    lua_pushvalue(l, t); // This is the value immediately after all the arguments

    return 1;
}

static const luaL_Reg lbuild_task_methods[] = {
    {"dependsOn", lbuild_task_dependsOn},
    {"run", lbuild_task_run},
    {NULL, NULL}
};

static const luaL_Reg lbuild_lib[] = {
    {"task", lbuild_create_lua_obj},
    {"getFiles", lbuild_get_files},
    {"exec", lbuild_inst_exec},
    {"runTask", lbuild_run_task},
    {NULL, NULL}
};

/**
 * This is here just to quiet the require error
 */
static int dummy_require(lua_State* l){
    // Behold, the worst thing I've ever written
    string require_tgt(luaL_checkstring(l, -1));
    if (require_tgt == "LBuildLib.lua"){
        lua_getglobal(l, "lbuild");
        return 1;
    }

    return 0;
}

void LBUILD::init_lua(lua_State* l){
    // Setup the _X env that stores all the exec functions
    lua_newtable(l);
    lua_setglobal(l, "_X");

    lua_pushcfunction(l, dummy_require, NULL);
    lua_setglobal(l, "require");

    luaL_register(l, "lbuild", lbuild_lib);

    // Create a metatable for task instances
    luaL_newmetatable(l, "taskmt");
    lua_pushstring(l, "__index");
    luaL_register(l, "lbuild_task", lbuild_task_methods);

    lua_settable(l, -3);
}

void LBUILD::setup_dependencies(){
    for (auto &[k,v] : depends_buffer){
        //printf("task %s: ", k.c_str());
        
        auto self = BuildTarget::get_target(k);
        if (self == NULL){
            fprintf(stderr, "Build target %s does not exist in registered_targets\n", k.c_str());
            return;
        }

        for (string task_name : *v){
            //printf("%s, ", task_name.c_str());
            self->add_dependency(task_name);
        }
        //printf("\n");
    }
}

void LBUILD::run_task(lua_State* l, string task_name){
    shared_ptr<BuildTarget> p = BuildTarget::get_target(task_name);
    if (p == NULL){
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), "%s is not an existing task", task_name.c_str());
        throw invalid_argument(buffer);
    }

    p->run(l);
}

void LBUILD::cleanup(){
    depends_buffer.clear();
}

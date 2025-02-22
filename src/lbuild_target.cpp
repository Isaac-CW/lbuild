#include "lua.h"
#include "lualib.h"

#include "lbuild_target.h"
#include "lbuild_args.h"
#include "lbuild_util.h"

#include <string>
#include <memory>
#include <unordered_map>
#include <stack>
#include <queue>

using namespace LBUILD;

std::unordered_map<std::string, std::shared_ptr<BuildTarget>> BuildTarget::registered_targets = {};

bool BuildTarget::has_circular_dependency(shared_ptr<BuildTarget> tgt1, shared_ptr<BuildTarget> tgt2){
    std::queue<std::shared_ptr<BuildTarget>> process_queue;
    process_queue.push(tgt2);
    while (!process_queue.empty()){
        auto cur = process_queue.front();
        process_queue.pop();

        //printf("dep check %ld, %p, %p\n", process_queue.size(), cur.get(), tgt1.get());

        if (cur.get() == tgt1.get()){return true;}
        for (auto v : cur->dependencies){
            process_queue.push(v);
        }
    }

    return false;
}

static bool key_exists(std::string key_name){
    try {
        BuildTarget::registered_targets.at(key_name);
    } catch (std::out_of_range e){
        return false;
    }
    return true;
}

BuildTarget::BuildTarget(std::string task_name){
    this->target_name = task_name;
    this->dependencies = {};
}

std::shared_ptr<BuildTarget> BuildTarget::create_target(std::string task_name){
    if (key_exists(task_name)){
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), "task %s already exists", task_name.c_str());
        throw std::invalid_argument(buffer);
    }

    std::shared_ptr<BuildTarget> ret_val(new BuildTarget(task_name));
    registered_targets.insert_or_assign(task_name, ret_val);

    return ret_val;
}

std::shared_ptr<BuildTarget> BuildTarget::get_target(std::string task_name){
    if (key_exists(task_name)){
        std::shared_ptr<BuildTarget> ret_val = registered_targets.at(task_name);
        //printf("refs: %d\n", ret_val.use_count());

        return ret_val;
    }

    return std::shared_ptr<BuildTarget>(NULL);
}

void BuildTarget::cleanup(){
    /*for (auto [k,v] : registered_targets){
        printf("cleanup refs: %d\n", v.use_count());
    }*/
    registered_targets.clear();
}

int BuildTarget::run(lua_State* l){
    // Run all the dependencies
    for (std::shared_ptr<BuildTarget> dep : this->dependencies){
        dep->run(l);
    }
    // Fetch the lua function associated with this build rule
    lua_getglobal(l, "_X");
    lua_pushstring(l, this->target_name.c_str());
    lua_gettable(l, -2);

    if (lua_isnil(l, -1)){
        fprintf(stderr, "No lua function is registered in _X for build target %s\n", this->target_name.c_str());
        return LUA_ERRRUN;
    }
    // Get the f and u from the container
    lua_pushstring(l, "f");
    lua_gettable(l, -2);

    lua_pushstring(l, "u");
    lua_gettable(l, -3);

    int head_type = lua_type(l, -2);
    if (head_type != LUA_TFUNCTION){
        fprintf(stderr, "Expected function for entry in _X for %s, got %s\n", this->target_name.c_str(), lua_typename(l, head_type));
        return LUA_ERRRUN;
    }

    int status = lua_pcall(l, 1, 0, 0);
    if (status == LUA_ERRRUN){
        const char* error_msg = luaL_checkstring(l, -1);
        luaL_error(l, "Unable to run function for build target %s: %s\n", this->target_name.c_str(), error_msg);
    } else if (status != LUA_OK){
        const char* error_msg = luaL_checkstring(l, -1);
        fprintf(stderr, "Unable to run build target %s: %s\n", this->target_name.c_str(), error_msg);
    }

    return status;
}

int BuildTarget::add_dependency(std::string target_name){
    std::shared_ptr<BuildTarget> p = BuildTarget::get_target(target_name);

    if (p != NULL){
        // Explore the other target's dependencies to see if this is already in it
        if (has_circular_dependency(BuildTarget::get_target(this->target_name), p)){
            char buffer[1024];
            snprintf(buffer, sizeof(buffer), "task %s has a circular dependency on %s", this->target_name.c_str(), p->target_name.c_str());
            throw std::invalid_argument(buffer);
        }
        this->dependencies.push_back(p);
    }

    return 0;
}


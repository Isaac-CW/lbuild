#ifndef LBUILD_TGT
#define LBUILD_TGT

#include "lbuild_args.h"
#include "lua.h"

#include <unordered_map>
#include <vector>
#include <string>
#include <filesystem>
#include <memory>
#include <exception>

using namespace std;

namespace LBUILD {
    class BuildTarget {
        private:
            string target_name;
            vector<shared_ptr<BuildTarget>> dependencies;
            BuildTarget(string target_name);
        public:
            /**
             * Creates the build target, adds it to the registered_targets hashmap and returns a shared pointer to that
             * 
             * This method will error if a build target with that name already exists, throwing an invalid_argument exception
             */
            static shared_ptr<BuildTarget> create_target(string target_name);

            /**
             * Returns a shared_ptr to the build target that is registered under the given target_name
             */
            static shared_ptr<BuildTarget> get_target(string target_name);

            /**
             * Clears the registered_targets hashmap for the purpose of tearing down the system when execution is finished
             */
            static void cleanup();

            /**
             * Returns true if target2 has target1 as a dependency
             */
            static bool has_circular_dependency(shared_ptr<BuildTarget> target1, shared_ptr<BuildTarget> target2);

            static unordered_map<std::string, std::shared_ptr<BuildTarget>> registered_targets;


            int run(lua_State* l);
            int add_dependency(string obj);
    };

    /**
     * A LuaFuncBuild is a build target that has exec defined as a lua function
     * So it can explicitly modify the environment 
     */
    class LuaFuncBuild : BuildTarget {
        private:
            
    };

    class build_target_exists : exception{

    };
}

#endif
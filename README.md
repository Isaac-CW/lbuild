# LBuild
## What if makefile but lua?
---
A silly little tool that allows defining build scripts in Luau

### Setup
Simply running cmake in the current directory will build luau and the project as needed
```
cmake . --fresh
cmake --build .
```
---
### Usage
The API provided by LBuild is defined in the module `LBuildLib.lua`
```lua
export type file = {
    filename:string,
    extension:string,
    path:string,
}

export type task = {
    -- Instance vars
    name:string,
    -- Methods
    dependsOn:(task, ...string)->task,
    run:(task, (task)->nil)->task,
}

return m :: {
    runTask:(task, string)->nil,

    task:(string)->task,
    exec:(task, string)->nil,
    getFiles:(...string)->{file},
}
```

Similar to gradle, build tasks are defined by using `lbuild.task(task_name)` and its methods can be chained together.
```lua
lbuild.task("build")
    :dependsOn("task1", "task2")
    :run(function(self)
        ...
    end)
```

The given string in `lbuild.task` uniquely identifies the task and `task:run` defines the lua code that will be called when the task is run.
The `:dependsOn` method takes a variable number of task names which will be run before the given task is run

#### exec
`lbuild.exec` requires the task that is executing the command and a string as "raw input" into your shell. This is passed into the `exec` family of functions so any stipulations with usage apply here.
```lua
lbuild.task("build")
    :dependsOn("task1", "task2")
    :run(function(self)
        lbuild.exec(self, `rm -f {BIN_NAME}`)
    end)
```

#### runTask
`lbuild.runTask` requires the task that is calling this method as well as a string representing the name of the task to execute. 
```lua
lbuild.task("build")
    :dependsOn("clean")
    :run(function(self)
        lbuild.runTask(self, "logAction")
    end)
```
Any given task may not run a task which has that task as a dependency
```lua
lbuild.task("clean")
    :run(function(self)
        lbuild.runTask(self, "build") -- Will throw an error
    end)
```

#### getFiles
`lbuild.getFiles(path...)` returns an array of tuples for each file at the given file path. Each tuple is outlined in the `lbuild` file:
```lua
export type file = {
    filename:string,
    extension:string,
    path:string,
}
```
### Sample build script
```lua
local lbuild = require("LBuildLib.lua")

local function to_args(tbl:{string}, sep:string?):string
    sep = sep or " "

    local retVal = ""

    local first = true
    for k,v in tbl do
        if first then
            first = false
        else
            retVal ..= sep
        end

        retVal ..= v
    end

    return retVal
end

local SRC_DIR = "./src"

local file_raw:{string} = {}
local src_files:{lbuild.file} = lbuild.get_files(SRC_DIR)
local target_src:{string} = {}
local output_files:{string} = {}

local OUTPUT_BIN = "sample"
local OUTPUT_DIR = "./bin"

local HEADERS = {"-I./", "-I./include"}
local CMP_FLAGS = {"-g", "-lm"}

local CC = "gcc"
-- Setup rules
for k,v in src_files do
    if (v.extension ~= ".c") then continue end

    local src_file = `{SRC_DIR}/{v.filename}.c`
    local output_file = `{OUTPUT_DIR}/{v.filename}.o`

    table.insert(target_src, src_file)
    table.insert(output_files, output_file)
    table.insert(file_raw, v.filename)

    lbuild.task (v.filename)
        :run(function(self:lbuild.task)
            lbuild.exec(self, `{CC} -c {to_args(HEADERS)} {to_args(CMP_FLAGS)} {src_file} -o {output_file}`)
        end)
end

lbuild.task ("debug")
    :dependsOn()
    :run(function(self:lbuild.task)
        table.insert(HEADERS, "-DDEBUG=1")
    end)

lbuild.task ("debugASAN")
    :dependsOn("debug")
    :run(function(self:lbuild.task)
        table.insert(HEADERS, "-fsanitize=address")
        lbuild.runTask(self, "build")
    end)

lbuild.task ("build")
    :dependsOn(table.unpack(file_raw))
    :run(function(self:lbuild.task)
        lbuild.exec(self, `{CC} {to_args(HEADERS)} {to_args(CMP_FLAGS)} {to_args(output_files)} -o {OUTPUT_BIN}`)
    end)

lbuild.task ("run")
    :dependsOn("build")
    :run(function(self: lbuild.task) 
        lbuild.exec(self, `./{OUTPUT_BIN}`)
    end)

lbuild.task ("lldb_showstack")
    :dependsOn("build")
    :run(function(self:lbuild.task)
        lbuild.exec(self, `lldb ./{OUTPUT_BIN} --source ./lldb_args.txt`)
    end)

lbuild.task ("clean")
    :run(function(self:lbuild.task)
        lbuild.exec(self, `rm -f {to_args(output_files)}`)
        lbuild.exec(self, `rm -f {OUTPUT_BIN}`)
    end)

```
---
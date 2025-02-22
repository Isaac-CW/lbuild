local m = {}

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
std = "luajit"

include_files = {
    "**/*.lua",
    "extra/dist/tarantoolctl.in",
}

exclude_files = {
    "build/**/*.lua",
    "src/box/lua/serpent.lua", -- third-party source code
    "test/**/*.lua",
    "test-run/**/*.lua",
    "third_party/**/*.lua",
    ".rocks/**/*.lua",
    ".git/**/*.lua",
}

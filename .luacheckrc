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

files["extra/dist/tarantoolctl.in"] = {
    globals = {"box", "_TARANTOOL"},
    ignore = {"212/self", "122", "431"}
}
files["**/*.lua"] = {
    globals = {"box", "_TARANTOOL"},
    ignore = {"212/self", "143"}
}
files["src/lua/help.lua"] = {globals = {"help", "tutorial"}}
files["src/lua/init.lua"] = {globals = {"dostring"}, ignore = {"122", "142"}}
files["src/lua/swim.lua"] = {ignore = {"431"}}
files["src/box/lua/console.lua"] = {globals = {"help"}, ignore = {"212"}}
files["src/box/lua/net_box.lua"] = {ignore = {"431", "432"}}
files["src/box/lua/schema.lua"] = {globals = {"tonumber64"}, ignore = {"431", "432"}}
files["test/app/lua/fiber.lua"] = {globals = {"box_fiber_run_test"}}

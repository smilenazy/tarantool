std = "luajit"

include_files = {
    "**/*.lua",
    "extra/dist/tarantoolctl.in",
}

exclude_files = {
    "build/**/*.lua",
    "src/box/lua/serpent.lua", -- third-party source code
    "test/**/*.lua",
    "src/box/lua/serpent.lua", -- third-party source code
    "test/app/**/*.test.lua",
    "test/box/**/*.test.lua",
    "test/box/lua/test_init.lua",
    "test/engine/*.lua",
    "test/engine_long/*.lua",
    "test/long_run-py/**/*.lua",
    "test/replication/*.lua",
    "test/sql/*.lua",
    "test/swim/*.lua",
    "test/var/**/*.lua",
    "test/vinyl/*.lua",
    "test/wal_off/*.lua",
    "test/xlog/*.lua",
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
files["test/app-tap/lua/require_mod.lua"] = {globals = {"exports"}}
files["test/app-tap/string.test.lua"] = {globals = {"utf8"}}
files["test/box/box.lua"] = {globals = {"cfg_filter", "sorted", "iproto_request"}}
files["test/box/lua/push.lua"] = {globals = {"push_collection"}}
files["test/box/lua/index_random_test.lua"] = {globals = {"index_random_test"}}
files["test/box/lua/utils.lua"] = {
	globals = {"space_field_types", "iterate", "arithmetic", "table_shuffle",
	"table_generate", "tuple_to_string", "check_space", "space_bsize",
	"create_iterator", "setmap", "sort"}}
files["test/box/lua/bitset.lua"] = {
	globals = {"create_space", "fill", "delete", "clear", "drop_space",
	"dump", "test_insert_delete"}
}
files["test/box/lua/fifo.lua"] = {globals = {"fifomax", "find_or_create_fifo", "fifo_push", "fifo_top"}}
files["test/box/lua/identifier.lua"] = {globals = {"run_test"}}
files["test/box/lua/require_mod.lua"] = {globals = {"exports"}}

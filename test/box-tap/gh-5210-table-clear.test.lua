#!/usr/bin/env tarantool

local tap = require('tap')
local test = tap.test('gh-5210-table-clear')

test:plan(1)
t = {a = 1, b = 2}
test:is(table.clear(t), nil, 'table clear')

os.exit(test:check() and 0 or 1)

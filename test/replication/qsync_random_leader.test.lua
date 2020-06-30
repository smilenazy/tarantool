os = require('os')
env = require('test_run')
math = require('math')
fiber = require('fiber')
test_run = env.new()
engine = test_run:get_cfg('engine')

NUM_INSTANCES = 3
BROKEN_QUORUM = NUM_INSTANCES + 1

SERVERS = {}
test_run:cmd("setopt delimiter ';'")
for i=1,NUM_INSTANCES do
    SERVERS[i] = 'qsync' .. i
end;
test_run:cmd("setopt delimiter ''");
SERVERS -- print instance names

random = function(excluded_num, min, max)       \
    math.randomseed(os.time())                  \
    local r = math.random(min, max)             \
    if (r == excluded_num) then                 \
        return random(excluded_num, min, max)   \
    end                                         \
    return r                                    \
end

test_run:create_cluster(SERVERS, "replication", {args="0.1"})
test_run:wait_fullmesh(SERVERS)
current_leader_id = 1
test_run:switch(SERVERS[current_leader_id])
box.cfg{replication_synchro_quorum=3, replication_synchro_timeout=0.1}
_ = box.schema.space.create('sync', {is_sync=true})
_ = box.space.sync:create_index('pk')
test_run:switch('default')

-- Testcase body.
for i=1,10 do                                                 \
    new_leader_id = random(current_leader_id, 1, #SERVERS)    \
    test_run:switch(SERVERS[new_leader_id])                   \
    box.cfg{read_only=false}                                  \
    fiber = require('fiber')                                  \
    f1 = fiber.create(function() box.space.sync:delete{} end) \
    f2 = fiber.create(function() for i=1,10000 do box.space.sync:insert{i} end end) \
    f1.status()                                               \
    f2.status()                                               \
    test_run:switch('default')                                \
    test_run:switch(SERVERS[current_leader_id])               \
    box.cfg{read_only=true}                                   \
    test_run:switch('default')                                \
    current_leader_id = new_leader_id                         \
    fiber.sleep(0.1)                                          \
end

-- Teardown.
test_run:switch(SERVERS[current_leader_id])
box.space.sync:drop()
test_run:switch('default')
test_run:drop_cluster(SERVERS)

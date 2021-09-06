local t = require('luatest')
local log = require('log')


local g = t.group()
local fio = require('fio')

local Server = t.Server

g.before_all(function()
    g.master = Server:new({
        alias = 'master',
        command = './test/replication-luatest/instance_files/master.lua',
        workdir = fio.tempdir(),
        http_port = 8081,
        net_box_port = 13301,
    })

    g.replica = Server:new({
        alias = 'replica',
        command = './test/replication-luatest/instance_files/replica.lua',
        workdir = fio.tempdir(),
        http_port = 8082,
        net_box_port = 13302,
    })


    g.master:start()
    g.replica:start()

    t.helpers.retrying({}, function() g.master:connect_net_box() end)
    t.helpers.retrying({}, function() g.replica:connect_net_box() end)

    log.info('Everything is started')
end)


g.after_all(function()
    g.replica:stop()
    g.master:stop()
    fio.rmtree(g.master.workdir)
    fio.rmtree(g.replica.workdir)
end)


local function wait_vclock()
    lsn = g.master:eval("return box.info.vclock[1]")
    _, tbl = g.master:eval("return next(box.info.replication_anon())")
    to_lsn = tbl.downstream.vclock[1]
    while to_lsn == nil or to_lsn < lsn do
        require('fiber').sleep(0.001)
        _, tbl = g.master:eval("return next(box.info.replication_anon())")
        to_lsn = tbl.downstream.vclock[1]
        log.info(string.format("master lsn: %d; replica_anon lsn: %d",
            lsn, to_lsn))
    end
    return
end


g.test_qsync_with_anon = function()
    g.master:eval("box.schema.space.create('sync', {is_sync = true})")
    g.master:eval("box.space.sync:create_index('pk')")

    t.assert_error_msg_content_equals("Quorum collection for a synchronous transaction is timed out",
        function() g.master:eval("return box.space.sync:insert{1}") end)

    -- Wait until everything is replicated from the master to the replica
    wait_vclock()

    t.assert_equals(g.master:eval("return box.space.sync:select()"), {})
    t.assert_equals(g.replica:eval("return box.space.sync:select()"), {})
end

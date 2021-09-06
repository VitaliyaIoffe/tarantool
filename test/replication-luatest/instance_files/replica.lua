#!/usr/bin/env tarantool

local function instance_uri(instance_id)
    return 'localhost:'..(13300 + instance_id)
end

box.cfg({
    work_dir            = os.getenv('TARANTOOL_WORKDIR'),
    listen              = os.getenv('TARANTOOL_LISTEN'),
    replication         = {instance_uri(1), instance_uri(2)},
    memtx_memory        = 107374182,
    replication_timeout = 0.1,
    replication_connect_timeout = 0.5,
    read_only           = true,
    replication_anon    = true
})

require('log').warn("replica is ready")

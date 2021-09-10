env = require('test_run')
test_run = env.new()
test_run:cmd("create server test with script='box/tx_with_timeout.lua'")
test_run:cmd(string.format("start server test"))

test_run:cmd("switch test")
txn_proxy = require('txn_proxy')
fiber = require('fiber')


s = box.schema.space.create('test')
i = s:create_index('pk')
tx = txn_proxy.new()


test_run:cmd("switch default")

test_run:cmd("stop server test")
test_run:cmd("cleanup server test")
test_run:cmd("delete server test")


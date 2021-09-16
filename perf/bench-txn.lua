#!/usr/bin/env tarantool

os.execute("mkdir -p work_dir")
os.execute("cp txn.so ./work_dir/")

box.cfg {
    work_dir            = "./work_dir",
    wal_mode            = "none",
    memtx_use_mvcc_engine = true,
}

s = box.schema.space.create('test')
_ = s:create_index('pk')

txn = require('txn')
txn:bench('test', 'pk')

os.execute("rm -rf ../work_dir")
os.exit(0)


#!/usr/bin/env tarantool

local tap = require('tap')
local test = tap.test("errno")
local date = require('datetime')
local ffi = require('ffi')

ffi.cdef [[
    void tzset(void);
]]

test:plan(8)

local function assert_raises(test, error_msg, func, ...)
    local ok, err = pcall(func, ...)
    local err_tail = err:gsub("^.+:%d+: ", "")
    return test:ok(not ok and err_tail == error_msg,
                   ('"%s" received, "%s" expected'):format(err_tail, error_msg))
end

test:test("Default date creation", function(test)
    test:plan(9)
    -- check empty arguments
    local T1 = date.new()
    test:is(T1.epoch, 0, "T.epoch ==0")
    test:is(T1.nsec, 0, "T.nsec == 0")
    test:is(T1.tzoffset, 0, "T.tzoffset == 0")
    test:is(tostring(T1), "1970-01-01T00:00:00Z", "tostring(T1)")
    -- check empty table
    local T2 = date.new{}
    test:is(T2.epoch, 0, "T.epoch ==0")
    test:is(T2.nsec, 0, "T.nsec == 0")
    test:is(T2.tzoffset, 0, "T.tzoffset == 0")
    test:is(tostring(T2), "1970-01-01T00:00:00Z", "tostring(T2)")
    -- check their equivalence
    test:is(T1, T2, "T1 == T2")
end)

test:test("Datetime string formatting", function(test)
    test:plan(8)
    local t = date()
    test:is(t.epoch, 0, ('t.epoch == %d'):format(tonumber(t.epoch)))
    test:is(t.nsec, 0, ('t.nsec == %d'):format(t.nsec))
    test:is(t.tzoffset, 0, ('t.tzoffset == %d'):format(t.tzoffset))
    test:is(t:format('%d/%m/%Y'), '01/01/1970', '%s: format #1')
    test:is(t:format('%A %d. %B %Y'), 'Thursday 01. January 1970', 'format #2')
    test:is(t:format('%FT%T%z'), '1970-01-01T00:00:00+0000', 'format #3')
    test:is(t:format('%FT%T.%f%z'), '1970-01-01T00:00:00.000+0000', 'format #4')
    test:is(t:format('%FT%T.%4f%z'), '1970-01-01T00:00:00.0000+0000', 'format #5')
end)

test:test("Datetime string formatting detailed", function(test)
    test:plan(79)
    local T = date.new{ timestamp = 0.125 }
    T:set{ tzoffset = 180 }
    test:is(tostring(T), '1970-01-01T03:00:00.125+0300', 'tostring()')
    -- %Z and %+ are local timezone dependent. To make sure that
    -- test is deterministic we enforce timezone via TZ environment
    -- manipulations and calling tzset()
    os.setenv('TZ', 'GMT')
    ffi.C.tzset()
    local formats = {
        { '%A',                         'Thursday' },
        { '%a',                         'Thu' },
        { '%B',                         'January' },
        { '%b',                         'Jan' },
        { '%h',                         'Jan' },
        { '%C',                         '19' },
        { '%c',                         'Thu Jan  1 03:00:00 1970' },
        { '%D',                         '01/01/70' },
        { '%m/%d/%y',                   '01/01/70' },
        { '%d',                         '01' },
        { '%Ec',                        'Thu Jan  1 03:00:00 1970' },
        { '%EC',                        '19' },
        { '%Ex',                        '01/01/70' },
        { '%EX',                        '03:00:00' },
        { '%Ey',                        '70' },
        { '%EY',                        '1970' },
        { '%Od',                        '01' },
        { '%oe',                        'oe' },
        { '%OH',                        '03' },
        { '%OI',                        '03' },
        { '%Om',                        '01' },
        { '%OM',                        '00' },
        { '%OS',                        '00' },
        { '%Ou',                        '4' },
        { '%OU',                        '00' },
        { '%OV',                        '01' },
        { '%Ow',                        '4' },
        { '%OW',                        '00' },
        { '%Oy',                        '70' },
        { '%e',                         ' 1' },
        { '%F',                         '1970-01-01' },
        { '%Y-%m-%d',                   '1970-01-01' },
        { '%H',                         '03' },
        { '%I',                         '03' },
        { '%j',                         '001' },
        { '%k',                         ' 3' },
        { '%l',                         ' 3' },
        { '%M',                         '00' },
        { '%m',                         '01' },
        { '%n',                         '\n' },
        { '%p',                         'AM' },
        { '%R',                         '03:00' },
        { '%H:%M',                      '03:00' },
        { '%r',                         '03:00:00 AM' },
        { '%I:%M:%S %p',                '03:00:00 AM' },
        { '%S',                         '00' },
        { '%s',                         '10800' },
        { '%f',                         '125' },
        { '%3f',                        '125' },
        { '%6f',                        '125000' },
        { '%6d',                        '6d' },
        { '%3D',                        '3D' },
        { '%T',                         '03:00:00' },
        { '%H:%M:%S',                   '03:00:00' },
        { '%t',                         '\t' },
        { '%U',                         '00' },
        { '%u',                         '4' },
        { '%V',                         '01' },
        { '%G',                         '1970' },
        { '%g',                         '70' },
        { '%v',                         ' 1-Jan-1970' },
        { '%e-%b-%Y',                   ' 1-Jan-1970' },
        { '%W',                         '00' },
        { '%w',                         '4' },
        { '%X',                         '03:00:00' },
        { '%x',                         '01/01/70' },
        { '%y',                         '70' },
        { '%Y',                         '1970' },
        { '%Z',                         'GMT' },
        { '%z',                         '+0300' },
        { '%+',                         'Thu Jan  1 03:00:00 GMT 1970' },
        { '%%',                         '%' },
        { '%Y-%m-%dT%H:%M:%S.%9f%z',    '1970-01-01T03:00:00.125000000+0300' },
        { '%Y-%m-%dT%H:%M:%S.%f%z',     '1970-01-01T03:00:00.125+0300' },
        { '%Y-%m-%dT%H:%M:%S.%f',       '1970-01-01T03:00:00.125' },
        { '%FT%T.%f',                   '1970-01-01T03:00:00.125' },
        { '%FT%T.%f%z',                 '1970-01-01T03:00:00.125+0300' },
        { '%FT%T.%9f%z',                '1970-01-01T03:00:00.125000000+0300' },
    }
    for _, row in pairs(formats) do
        local fmt, value = unpack(row)
        test:is(T:format(fmt), value,
                ('format %s, expected %s'):format(fmt, value))
    end
end)


test:test("totable{}", function(test)
    test:plan(9)
    local exp = {sec = 0, min = 0, wday = 5, day = 1,
                 nsec = 0, isdst = false, yday = 1,
                 tzoffset = 0, month = 1, year = 1970, hour = 0}
    local T = date.new()
    local TT = T:totable()
    test:is_deeply(TT, exp, "date:totable()")

    local D = os.date('*t')
    TT = date.new(D):totable()
    local keys = {
        'sec', 'min', 'wday', 'day', 'yday', 'month', 'year', 'hour'
    }
    for _, key in pairs(keys) do
        test:is(TT[key], D[key], ("[%s]: %s == %s"):format(key, TT[key], D[key]))
    end
end)

test:test("Time :set{} operations", function(test)
    test:plan(9)

    local T = date.new{ year = 2021, month = 8, day = 31,
                  hour = 0, min = 31, sec = 11, tzoffset = '+0300'}
    test:is(tostring(T), '2021-08-31T00:31:11+0300', 'initial')
    test:is(tostring(T:set{ year = 2020 }), '2020-08-31T00:31:11+0300', '2020 year')
    test:is(tostring(T:set{ month = 11, day = 30 }), '2020-11-30T00:31:11+0300', 'month = 11, day = 30')
    test:is(tostring(T:set{ day = 9 }), '2020-11-09T00:31:11+0300', 'day 9')
    test:is(tostring(T:set{ hour = 6 }),  '2020-11-09T06:31:11+0300', 'hour 6')
    test:is(tostring(T:set{ min = 12, sec = 23 }), '2020-11-09T04:12:23+0300', 'min 12, sec 23')
    test:is(tostring(T:set{ tzoffset = -8*60 }), '2020-11-08T17:12:23-0800', 'offset -0800' )
    test:is(tostring(T:set{ tzoffset = '+0800' }), '2020-11-09T09:12:23+0800', 'offset +0800' )
    test:is(tostring(T:set{ timestamp = 1630359071.125 }),
            '2021-08-31T05:31:11.125+0800', 'timestamp 1630359071.125' )
end)

local function range_check_error(name, value, range)
    return ('value %s of %s is out of allowed range [%d, %d]'):
              format(value, name, range[1], range[2])
end

local function range_check_3_error(v)
    return ('value %d of %s is out of allowed range [%d, %d..%d]'):
            format(v, 'day', -1, 1, 31)
end

test:test("Time invalid :set{} operations", function(test)
    test:plan(17)

    local T = date.new{}

    assert_raises(test, range_check_error('year', 10000, {1, 9999}),
                  function() T:set{ year = 10000} end)
    assert_raises(test, range_check_error('year', -10, {1, 9999}),
                  function() T:set{ year = -10} end)

    assert_raises(test, range_check_error('month', 20, {1, 12}),
                  function() T:set{ month = 20} end)
    assert_raises(test, range_check_error('month', 0, {1, 12}),
                  function() T:set{ month = 0} end)
    assert_raises(test, range_check_error('month', -20, {1, 12}),
                  function() T:set{ month = -20} end)

    assert_raises(test,  range_check_3_error(40),
                  function() T:set{ day = 40} end)
    assert_raises(test,  range_check_3_error(0),
                  function() T:set{ day = 0} end)
    assert_raises(test,  range_check_3_error(-10),
                  function() T:set{ day = -10} end)

    assert_raises(test,  range_check_error('hour', 31, {0, 23}),
                  function() T:set{ hour = 31} end)
    assert_raises(test,  range_check_error('hour', -1, {0, 23}),
                  function() T:set{ hour = -1} end)

    assert_raises(test,  range_check_error('min', 60, {0, 59}),
                  function() T:set{ min = 60} end)
    assert_raises(test,  range_check_error('min', -1, {0, 59}),
                  function() T:set{ min = -1} end)

    assert_raises(test,  range_check_error('sec', 61, {0, 60}),
                  function() T:set{ sec = 61} end)
    assert_raises(test,  range_check_error('sec', -1, {0, 60}),
                  function() T:set{ sec = -1} end)

    local only1 = 'only one of nsec, usec or msecs may defined simultaneously'
    assert_raises(test, only1, function()
                    T:set{ nsec = 123456, usec = 123}
                  end)
    assert_raises(test, only1, function()
                    T:set{ nsec = 123456, msec = 123}
                  end)
    assert_raises(test, only1, function()
                    T:set{ nsec = 123456, usec = 1234, msec = 123}
                  end)
end)

local function invalid_tz_fmt_error(val)
    return ('invalid time-zone format %s'):format(val)
end

test:test("Time invalid tzoffset in :set{} operations", function(test)
    test:plan(10)

    local T = date.new{}
    local bad_strings = {
        'bogus',
        '0100',
        '+-0100',
        '+25:00',
        '+9900',
        '-99:00',
    }
    for _, val in ipairs(bad_strings) do
        assert_raises(test, invalid_tz_fmt_error(val),
                      function() T:set{ tzoffset = val } end)
    end

    local bad_numbers = {
        800,
        -800,
        10000,
        -10000,
    }
    for _, val in ipairs(bad_numbers) do
        assert_raises(test, range_check_error('tzoffset', val, {-720, 720}),
                      function() T:set{ tzoffset = val } end)
    end
end)


test:test("Time :set{day = -1} operations", function(test)
    test:plan(14)
    local tests = {
        {{ year = 2000, month = 3, day = -1}, '2000-03-31T00:00:00Z'},
        {{ year = 2000, month = 2, day = -1}, '2000-02-29T00:00:00Z'},
        {{ year = 2001, month = 2, day = -1}, '2001-02-28T00:00:00Z'},
        {{ year = 1900, month = 2, day = -1}, '1900-02-28T00:00:00Z'},
        {{ year = 1904, month = 2, day = -1}, '1904-02-29T00:00:00Z'},
    }
    local T
    for _, row in ipairs(tests) do
        local args, str = unpack(row)
        T = date.new(args)
        test:is(tostring(T), str, ('checking -1 with %s'):format(str))
    end
    assert_raises(test, range_check_3_error(0), function() T = date.new{day = 0} end)
    assert_raises(test, range_check_3_error(-2), function() T = date.new{day = -2} end)
    assert_raises(test, range_check_3_error(-10), function() T = date.new{day = -10} end)

    T = date.new{ year = 1904, month = 2, day = -1 }
    test:is(tostring(T), '1904-02-29T00:00:00Z', 'base before :set{}')
    test:is(tostring(T:set{month = 3, day = 2}), '1904-03-02T00:00:00Z', '2 March')
    test:is(tostring(T:set{day = -1}), '1904-03-31T00:00:00Z', '31 March')

    assert_raises(test, range_check_3_error(0), function() T:set{day = 0} end)
    assert_raises(test, range_check_3_error(-2), function() T:set{day = -2} end)
    assert_raises(test, range_check_3_error(-10), function() T:set{day = -10} end)
end)

os.exit(test:check() and 0 or 1)

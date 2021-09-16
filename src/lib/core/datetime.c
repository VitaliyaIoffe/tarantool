/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2021, Tarantool AUTHORS, please see AUTHORS file.
 */

#include <assert.h>
#include <limits.h>
#include <string.h>
#include <time.h>

#include "c-dt/dt.h"
#include "trivia/util.h"
#include "datetime.h"

/*
 * Given the seconds from Epoch (1970-01-01) we calculate date
 * since Rata Die (0001-01-01).
 * DT_EPOCH_1970_OFFSET is the distance in days from Rata Die to Epoch.
 */
static int
local_dt(int64_t secs)
{
	return dt_from_rdn((int)(secs / SECS_PER_DAY) + DT_EPOCH_1970_OFFSET);
}

static void
datetime_to_tm(const struct datetime *date, struct tm *tm)
{
	memset(tm, 0, sizeof(*tm));
	int64_t secs = date->epoch;
	dt_to_struct_tm(local_dt(secs), tm);

	int seconds_of_day = (int64_t)date->epoch % SECS_PER_DAY;
	tm->tm_hour = (seconds_of_day / 3600) % 24;
	tm->tm_min = (seconds_of_day / 60) % 60;
	tm->tm_sec = seconds_of_day % 60;
}

void
datetime_now(struct datetime *now)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	now->epoch = tv.tv_sec;
	now->nsec = tv.tv_usec * 1000;

	struct tm tm;
	localtime_r(&tv.tv_sec, &tm);
	now->tzoffset = tm.tm_gmtoff / 60;
}

size_t
datetime_strftime(char *buf, uint32_t len, const char *fmt,
		  const struct datetime *date)
{
	struct tm tm;
	datetime_to_tm(date, &tm);
	return strftime(buf, len, fmt, &tm);
}

/* NB! buf may be NULL, and we should handle it gracefully, returning
 * calculated length of output string
 */
int
datetime_to_string(char *buf, int len, const struct datetime *date)
{
	int offset = date->tzoffset;
	/* for negative offsets around Epoch date we could get
	 * negative secs value, which should be attributed to
	 * 1969-12-31, not 1970-01-01, thus we first shift
	 * epoch to Rata Die then divide by seconds per day,
	 * not in reverse
	 */
	int64_t rd_seconds = (int64_t)date->epoch + offset * 60 +
			     SECS_EPOCH_1970_OFFSET;
	int rd_number = rd_seconds / SECS_PER_DAY;
	assert(rd_number <= INT_MAX);
	assert(rd_number >= INT_MIN);
	dt_t dt = dt_from_rdn(rd_number);

	int year, month, day, second, nanosec, sign;
	dt_to_ymd(dt, &year, &month, &day);

	int hour = (rd_seconds / 3600) % 24;
	int minute = (rd_seconds / 60) % 60;
	second = rd_seconds % 60;
	nanosec = date->nsec;

	int sz = 0;
	SNPRINT(sz, snprintf, buf, len, "%04d-%02d-%02dT%02d:%02d:%02d",
		year, month, day, hour, minute, second);
	if (nanosec != 0) {
		if (nanosec % 1000000 == 0)
			SNPRINT(sz, snprintf, buf, len, ".%03d",
				nanosec / 1000000);
		else if (nanosec % 1000 == 0)
			SNPRINT(sz, snprintf, buf, len, ".%06d",
				nanosec / 1000);
		else
			SNPRINT(sz, snprintf, buf, len, ".%09d", nanosec);
	}
	if (offset == 0) {
		SNPRINT(sz, snprintf, buf, len, "Z");
	} else {
		if (offset < 0) {
			sign = '-';
			offset = -offset;
		} else {
			sign = '+';
		}
		SNPRINT(sz, snprintf, buf, len, "%c%02d%02d", sign,
			offset / 60, offset % 60);
	}
	return sz;
}

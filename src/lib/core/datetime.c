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

/**
 * NB! buf may be NULL, and we should handle it gracefully, returning
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

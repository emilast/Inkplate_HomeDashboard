#pragma once

// Text and time helpers. These have no Arduino or Inkplate dependencies, so they
// can be unit tested on the host (see test/).

#include <stddef.h>
#include <time.h>

void utf8ToIso88591(const char *utf8, char *iso, size_t maxIsoLen = 0);
void extractHourAndMinute(const char *timestamp, int &hour, int &minute);

// Signed difference in minutes from (h1:m1) to (h2:m2), assuming the two times are
// less than 12 hours apart. Handles midnight wrap-around, e.g. 23:59 -> 00:01 = +2.
int minutesBetween(int h1, int m1, int h2, int m2);

// Seconds since the Unix epoch for a UTC date and time (like timegm()).
time_t utcToEpoch(int year, int month, int day, int hour, int minute, int second);

// Parses an ISO 8601 UTC timestamp ("YYYY-MM-DDTHH:MM...") and converts it to local
// time using the current TZ setting. Returns false if the timestamp can't be parsed.
bool utcTimestampToLocalTime(const char *timestamp, struct tm *localTime);

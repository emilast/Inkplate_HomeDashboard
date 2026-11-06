// Host-side unit tests for src/Utils/utils.cpp. Run with: pio test -e native
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unity.h>

#include "Utils/utils.h"

void setUp()
{
    // Same time zone as the dashboard (see TIME_ZONE in src/config.h)
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
}

void tearDown()
{
}

static void assertConverts(const char *utf8, const char *expectedIso, size_t bufSize = 64)
{
    char iso[64];
    utf8ToIso88591(utf8, iso, bufSize);
    TEST_ASSERT_EQUAL_STRING(expectedIso, iso);
}

// ---- utf8ToIso88591 ----

void test_utf8_ascii_is_copied()
{
    assertConverts("Stuvsta 12", "Stuvsta 12");
}

void test_utf8_swedish_letters()
{
    assertConverts("Sn\xc3\xa4ttringe \xc3\x85rsta \xc3\xb6", "Sn\xe4ttringe \xc5rsta \xf6");
}

void test_utf8_c2_range_degree_sign()
{
    assertConverts("5\xc2\xb0", "5\xb0");
}

void test_utf8_en_dash_becomes_hyphen()
{
    assertConverts("A \xe2\x80\x93 B", "A - B");
}

void test_utf8_curly_quotes()
{
    assertConverts("\xe2\x80\x9cKlart\xe2\x80\x9d \xe2\x80\x99", "\"Klart\" '");
}

void test_utf8_unknown_character_is_single_question_mark()
{
    assertConverts("\xf0\x9f\x9a\x86 Buss", "? Buss");
}

void test_utf8_truncated_sequence_at_end()
{
    assertConverts("abc\xc3", "abc?");
    assertConverts("abc\xe2\x80", "abc?");
}

void test_utf8_stray_continuation_byte()
{
    assertConverts("\x80x", "?x");
}

void test_utf8_respects_buffer_size()
{
    assertConverts("abcdef", "abc", 4);
    assertConverts("\xc3\xa5\xc3\xa4\xc3\xb6", "\xe5", 2);
}

void test_utf8_null_input_gives_empty_string()
{
    char iso[4] = "xyz";
    utf8ToIso88591(nullptr, iso, sizeof(iso));
    TEST_ASSERT_EQUAL_STRING("", iso);
}

// ---- extractHourAndMinute ----

void test_extract_hour_and_minute()
{
    int hour, minute;
    extractHourAndMinute("2026-11-05T17:42:00", hour, minute);
    TEST_ASSERT_EQUAL_INT(17, hour);
    TEST_ASSERT_EQUAL_INT(42, minute);
}

void test_extract_hour_and_minute_invalid()
{
    int hour, minute;
    extractHourAndMinute("17:42", hour, minute);
    TEST_ASSERT_EQUAL_INT(-1, hour);
    extractHourAndMinute(nullptr, hour, minute);
    TEST_ASSERT_EQUAL_INT(-1, minute);
}

// ---- minutesBetween ----

void test_minutes_between_same_hour()
{
    TEST_ASSERT_EQUAL_INT(3, minutesBetween(12, 10, 12, 13));
    TEST_ASSERT_EQUAL_INT(-2, minutesBetween(12, 10, 12, 8));
}

void test_minutes_between_across_midnight()
{
    TEST_ASSERT_EQUAL_INT(2, minutesBetween(23, 59, 0, 1));
    TEST_ASSERT_EQUAL_INT(-2, minutesBetween(0, 1, 23, 59));
}

// ---- utcToEpoch / utcTimestampToLocalTime ----

void test_utc_to_epoch_known_values()
{
    TEST_ASSERT_TRUE(utcToEpoch(1970, 1, 1, 0, 0, 0) == 0);
    TEST_ASSERT_TRUE(utcToEpoch(2000, 2, 29, 0, 0, 0) == 951782400); // leap day
    TEST_ASSERT_TRUE(utcToEpoch(2026, 11, 5, 17, 0, 0) == 1793898000);
}

static void assertLocalTime(const char *utc, int expectedHour, int expectedMinute, int expectedDay)
{
    struct tm local;
    TEST_ASSERT_TRUE(utcTimestampToLocalTime(utc, &local));
    TEST_ASSERT_EQUAL_INT(expectedHour, local.tm_hour);
    TEST_ASSERT_EQUAL_INT(expectedMinute, local.tm_min);
    TEST_ASSERT_EQUAL_INT(expectedDay, local.tm_mday);
}

void test_local_time_winter_and_summer()
{
    assertLocalTime("2026-01-15T12:00:00Z", 13, 0, 15); // CET, UTC+1
    assertLocalTime("2026-07-01T22:30:00Z", 0, 30, 2);  // CEST, UTC+2, next day
}

void test_local_time_dst_changes()
{
    assertLocalTime("2026-03-29T00:59:00Z", 1, 59, 29); // just before spring forward
    assertLocalTime("2026-03-29T01:00:00Z", 3, 0, 29);  // just after
    assertLocalTime("2026-10-25T00:30:00Z", 2, 30, 25); // first 02:30 in autumn
    assertLocalTime("2026-10-25T01:30:00Z", 2, 30, 25); // second 02:30
}

void test_local_time_invalid_timestamp()
{
    struct tm local;
    TEST_ASSERT_FALSE(utcTimestampToLocalTime("not a time", &local));
    TEST_ASSERT_FALSE(utcTimestampToLocalTime(nullptr, &local));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_utf8_ascii_is_copied);
    RUN_TEST(test_utf8_swedish_letters);
    RUN_TEST(test_utf8_c2_range_degree_sign);
    RUN_TEST(test_utf8_en_dash_becomes_hyphen);
    RUN_TEST(test_utf8_curly_quotes);
    RUN_TEST(test_utf8_unknown_character_is_single_question_mark);
    RUN_TEST(test_utf8_truncated_sequence_at_end);
    RUN_TEST(test_utf8_stray_continuation_byte);
    RUN_TEST(test_utf8_respects_buffer_size);
    RUN_TEST(test_utf8_null_input_gives_empty_string);
    RUN_TEST(test_extract_hour_and_minute);
    RUN_TEST(test_extract_hour_and_minute_invalid);
    RUN_TEST(test_minutes_between_same_hour);
    RUN_TEST(test_minutes_between_across_midnight);
    RUN_TEST(test_utc_to_epoch_known_values);
    RUN_TEST(test_local_time_winter_and_summer);
    RUN_TEST(test_local_time_dst_changes);
    RUN_TEST(test_local_time_invalid_timestamp);
    return UNITY_END();
}

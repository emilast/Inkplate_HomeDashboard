#include <stdio.h>
#include <string.h>
#include "utils.h"
// Returns the number of bytes in the UTF-8 sequence that starts with lead byte c,
// or 1 for bytes that cannot start a sequence (stray continuation bytes etc).
static size_t utf8SequenceLength(unsigned char c)
{
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

// Maps a Unicode code point outside ISO-8859-1 to a close ISO-8859-1 replacement,
// or returns 0 if there is none. Covers the punctuation that shows up in SL texts.
static char replacementFor(unsigned long cp)
{
    switch (cp)
    {
    case 0x2010:
    case 0x2011:
    case 0x2012:
    case 0x2013:
    case 0x2014:
    case 0x2212:
        return '-'; // hyphens, dashes, minus
    case 0x2018:
    case 0x2019:
    case 0x201A:
    case 0x2032:
        return '\'';
    case 0x201C:
    case 0x201D:
    case 0x201E:
    case 0x2033:
        return '"';
    case 0x2022:
    case 0x2027:
        return '\xB7'; // bullet -> middle dot
    case 0x2026:
        return '.'; // ellipsis
    case 0x2007:
    case 0x2009:
    case 0x200A:
    case 0x202F:
        return ' '; // thin / narrow spaces
    default:
        return 0;
    }
}

// Converts a UTF-8 encoded string to ISO-8859-1 encoding.
// Characters outside ISO-8859-1 are replaced by a close equivalent when one
// exists (e.g. en dash -> '-'), otherwise by a single '?'.
// Parameters:
//   utf8 - Input string in UTF-8 encoding.
//   iso - Output buffer for the ISO-8859-1 encoded string.
//   maxIsoLen - Size of the output buffer (0 for unlimited / legacy mode).
void utf8ToIso88591(const char *utf8, char *iso, size_t maxIsoLen)
{
    if (!utf8 || !iso)
    {
        if (iso && maxIsoLen > 0) *iso = '\0';
        return;
    }
    if (maxIsoLen == 1)
    {
        *iso = '\0';
        return;
    }
    size_t i = 0, j = 0;

    while (utf8[i] && (maxIsoLen == 0 || j + 1 < maxIsoLen))
    {
        unsigned char c = utf8[i];

        if (c < 0x80)
        {
            // ASCII character, copy directly
            iso[j++] = c;
            i++;
            continue;
        }

        size_t len = utf8SequenceLength(c);
        if (len == 1)
        {
            // Stray continuation byte or invalid lead byte
            iso[j++] = '?';
            i++;
            continue;
        }

        // Decode the sequence, stopping at a missing or invalid continuation byte
        unsigned long cp = c & (0xFF >> (len + 1));
        size_t k = 1;
        for (; k < len; k++)
        {
            unsigned char cc = utf8[i + k];
            if ((cc & 0xC0) != 0x80) break; // also stops at the terminating '\0'
            cp = (cp << 6) | (cc & 0x3F);
        }
        i += k;
        if (k < len)
        {
            iso[j++] = '?'; // truncated or malformed sequence
            continue;
        }

        if (cp >= 0xA0 && cp <= 0xFF)
        {
            iso[j++] = (char)cp; // ISO-8859-1 maps 1:1 to these code points
        }
        else if (cp >= 0x80 && cp < 0xA0)
        {
            iso[j++] = ' '; // C1 control characters
        }
        else
        {
            char r = replacementFor(cp);
            iso[j++] = r ? r : '?';
        }
    }
    iso[j] = '\0'; // Null-terminate the ISO string
}

// Extracts the hour and minute from a timestamp string in the format "YYYY-MM-DDTHH:MM:SS".
// Parameters:
//   timestamp - Input timestamp string.
//   hour - Reference to an integer to store the extracted hour.
//   minute - Reference to an integer to store the extracted minute.
void extractHourAndMinute(const char *timestamp, int &hour, int &minute)
{
    if (timestamp != nullptr && strlen(timestamp) >= 16)
    {                                                                // Ensure the string is valid and long enough
        hour = (timestamp[11] - '0') * 10 + (timestamp[12] - '0');   // Extract hour
        minute = (timestamp[14] - '0') * 10 + (timestamp[15] - '0'); // Extract minute
    }
    else
    {
        hour = -1;   // Invalid hour
        minute = -1; // Invalid minute
    }
}

int minutesBetween(int h1, int m1, int h2, int m2)
{
    int diff = (h2 * 60 + m2) - (h1 * 60 + m1);
    if (diff < -720)
        diff += 1440;
    else if (diff > 720)
        diff -= 1440;
    return diff;
}

time_t utcToEpoch(int year, int month, int day, int hour, int minute, int second)
{
    // Days from civil date, see https://howardhinnant.github.io/date_algorithms.html
    int y = year;
    int m = month;
    if (m <= 2)
    {
        y -= 1;
        m += 12;
    }
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = static_cast<unsigned>(y - era * 400);
    unsigned doy = (153 * (m - 3) + 2) / 5 + day - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    int days = era * 146097 + static_cast<int>(doe) - 719468;
    return (time_t)days * 86400 + hour * 3600 + minute * 60 + second;
}

bool utcTimestampToLocalTime(const char *timestamp, struct tm *localTime)
{
    int yr = 0, mo = 0, dy = 0, hr = 0, mn = 0;
    if (!timestamp || !localTime || sscanf(timestamp, "%d-%d-%dT%d:%d", &yr, &mo, &dy, &hr, &mn) != 5)
        return false;
    time_t utcSec = utcToEpoch(yr, mo, dy, hr, mn, 0);
    localtime_r(&utcSec, localTime);
    return true;
}

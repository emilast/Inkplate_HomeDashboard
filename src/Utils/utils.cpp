#include <string.h>
#include "utils.h"
// Converts a UTF-8 encoded string to ISO-8859-1 encoding.
// Parameters:
//   utf8 - Input string in UTF-8 encoding.
//   iso - Output buffer for the ISO-8859-1 encoded string.
//   maxIsoLen - Size of the output buffer (0 for unlimited / legacy mode).
void utf8ToIso88591(const char *utf8, char *iso, size_t maxIsoLen) {
    if (!utf8 || !iso) {
        if (iso && maxIsoLen > 0) *iso = '\0';
        return;
    }
    if (maxIsoLen == 1) {
        *iso = '\0';
        return;
    }
    size_t i = 0, j = 0;

    while (utf8[i] && (maxIsoLen == 0 || j + 1 < maxIsoLen)) {
        unsigned char c = utf8[i];

        if (c < 128) {
            // ASCII character, copy directly
            iso[j++] = c;
            i++;
        } else if ((c & 0xE0) == 0xC0) {
            // 2-byte UTF-8 sequence (common for Latin-1 characters)
            if (!utf8[i + 1]) break;
            unsigned char c2 = utf8[i + 1];

            if ((c == 0xC3) && (c2 >= 0x80 && c2 <= 0xBF)) {
                // Convert UTF-8 Latin-1 block (C3 xx -> ISO-8859-1)
                iso[j++] = c2 + 0x40;
                i += 2;  // Skip the second byte
            } else {
                // Unrecognized sequence, replace with '?'
                iso[j++] = '?';
                i += 2;
            }
        } else {
            // Unsupported character (e.g., 3-byte or 4-byte UTF-8)
            iso[j++] = '?';
            i++;
        }
    }
    iso[j] = '\0';  // Null-terminate the ISO string
}

// Extracts the hour and minute from a timestamp string in the format "YYYY-MM-DDTHH:MM:SS".
// Parameters:
//   timestamp - Input timestamp string.
//   hour - Reference to an integer to store the extracted hour.
//   minute - Reference to an integer to store the extracted minute.
void extractHourAndMinute(const char *timestamp, int &hour, int &minute) {
    if (timestamp != nullptr && strlen(timestamp) >= 16) {           // Ensure the string is valid and long enough
        hour = (timestamp[11] - '0') * 10 + (timestamp[12] - '0');    // Extract hour
        minute = (timestamp[14] - '0') * 10 + (timestamp[15] - '0');  // Extract minute
    } else {
        hour = -1;    // Invalid hour
        minute = -1;  // Invalid minute
    }
}

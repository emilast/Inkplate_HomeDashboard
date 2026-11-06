#pragma once

// Departures from one stop, as parsed from the SL Transport API. Text fields are
// converted to ISO-8859-1 for the display fonts.

static constexpr int MAX_DEPARTURES = 8;          // more than fit in the departures area
static constexpr int MAX_DEVIATIONS_PER_DEPARTURE = 3;

struct Departure
{
    char line[16];         // line designation, e.g. "744"
    char destination[128];
    char displayTime[16];  // the API's informal time, e.g. "17 min" or "Nu"
    int scheduledHour;     // -1 if unknown
    int scheduledMinute;
    int expectedHour;      // -1 if unknown
    int expectedMinute;
    bool cancelled;
    int deviationCount;
    char deviations[MAX_DEVIATIONS_PER_DEPARTURE][256];
};

enum class DepartureStatus
{
    Ok,
    HttpError,
    ParseError,
};

struct DepartureList
{
    DepartureStatus status;
    int httpCode;           // set when status is HttpError
    char errorMessage[256]; // the API's error message when status is HttpError
    int count;
    Departure departures[MAX_DEPARTURES];
};

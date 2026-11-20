#pragma once

// A read-only web page with the dashboard's health: uptime, the current step,
// the last result of each data source, Wi-Fi, memory and the last reset. It is
// served from its own task, so it still answers when loop() is stuck, which tells
// a stuck loop apart from a board that is down.

#include <stdint.h>
#include <time.h>
#include "../Diagnostics/Diagnostics.h"

enum class DataSource : uint8_t
{
    Weather,
    Bus,
    Train,
    Display, // not a request, but its failures share the per-minute history
};
static constexpr int REQUEST_SOURCE_COUNT = 3;
static constexpr int DATA_SOURCE_COUNT = 4;

struct SourceStatus
{
    bool attempted;
    time_t lastAttempt; // 0 if the clock wasn't set
    time_t lastSuccess;
    bool lastOk;
    char lastError[128]; // set when lastOk is false, ISO-8859-1 like the display text

    // Every failed request counts, also the ones whose retry then succeeded
    uint32_t failedRequests;
    uint32_t recoveredByRetry; // failed requests whose retry succeeded
    time_t lastFailure;
    char lastFailureError[128];
};

struct DisplayStatus
{
    bool beginFailed;
    bool updated;          // at least one update reached the panel
    time_t lastUpdate;     // last update that reached the panel
    uint32_t failedUpdates; // power-up attempts that failed, including retried ones
    uint32_t recoveredByRetry;
    time_t lastFailure;
    int powerGood; // power chip registers from the last failure, -1 if it didn't answer
    int int1;
    int int2;
};

struct DashboardStatus
{
    BootInfo boot;
    DisplayStatus display;
    SourceStatus weather;
    SourceStatus bus;
    SourceStatus train;
};

// Starts the web server task and announces the board as http://<hostname>.local/
void startStatusPage(const char *hostname);

// Hands a copy of the status to the web server task. Call after each update.
void publishStatus(const DashboardStatus &status);

// Records the result of one fetch in source. error may be null when ok is true.
void recordFetch(SourceStatus &source, bool ok, const char *error);

// Records one screen update: whether the panel power came up, after how many
// attempts, and the power chip registers read after a failure
void recordDisplayUpdate(DisplayStatus &display, bool ok, int attempts, int powerGood, int int1, int int2);

// Records one failed request, including attempts that are retried, in source and
// in the status page's per-minute history
void recordFailedRequest(SourceStatus &source, DataSource which, const char *error);

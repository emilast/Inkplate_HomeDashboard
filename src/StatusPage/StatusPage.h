#pragma once

// A read-only web page with the dashboard's health: uptime, the current step,
// the last result of each data source, Wi-Fi, memory and the last reset. It is
// served from its own task, so it still answers when loop() is stuck, which tells
// a stuck loop apart from a board that is down.

#include <time.h>
#include "../Diagnostics/Diagnostics.h"

struct SourceStatus
{
    bool attempted;
    time_t lastAttempt; // 0 if the clock wasn't set
    time_t lastSuccess;
    bool lastOk;
    char lastError[128]; // set when lastOk is false, ISO-8859-1 like the display text
};

struct DashboardStatus
{
    BootInfo boot;
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

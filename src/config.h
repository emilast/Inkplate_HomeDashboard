#pragma once

// User settings for the dashboard: location, public transport stops, display mode
// and timing. Everything you need to change to run it somewhere else is here.
//
// Text shown on the display is ISO-8859-1 encoded, so write å, ä, ö as
// \xe5, \xe4, \xf6 (and Å, Ä, Ö as \xc5, \xc4, \xd6).

// ---- Display ----

// 1 => 1 bit display, black/white + partial display updates
// 0 => 3 bit display, 8 colors (black, white and 6 grays), full refresh on every update
#define MONOCHROME 0

// ---- Weather (SMHI) ----

// Position to show the weather for
static const float WEATHER_LATITUDE = 59.255798826517136f;
static const float WEATHER_LONGITUDE = 17.98211146904839f;

// ---- Public transport (Trafiklab SL Transport API) ----

// Site IDs can be looked up with https://transport.integration.sl.se/v1/sites
static const char *const BUS_SITE_ID = "7083";
static const char *const BUS_TITLE = "Buss fr\xe5n Sn\xe4ttringe";

static const char *const TRAIN_SITE_ID = "9528";
static const char *const TRAIN_TITLE = "T\xe5g fr\xe5n Stuvsta";

// ---- Time ----

static const char *const NTP_SERVER = "pool.ntp.org";
// POSIX TZ string, here Central European Time with daylight saving (CET/CEST)
static const char *const TIME_ZONE = "CET-1CEST,M3.5.0,M10.5.0/3";

// ---- Timing ----

// Time between screen updates. Departures are fetched on every update.
static const int REFRESH_INTERVAL_MS = 20000;
// Weather is fetched, and a full refresh is done in monochrome mode, every Nth update
static const int FULL_REFRESH_EVERY_N_UPDATES = 10;

// Hour of the daily maintenance restart (local time)
static const int DAILY_RESTART_HOUR = 4;
// Minimum uptime before the daily restart, so the board doesn't restart twice
static const unsigned long MIN_UPTIME_BEFORE_RESTART_MS = 10UL * 60UL * 1000UL;

// The task watchdog reboots the board if loop() hangs for longer than this
static const int WDT_TIMEOUT_SECONDS = 60;

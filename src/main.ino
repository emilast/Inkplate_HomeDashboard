/*
   Inkplate Home Dashboard

   Shows the time, the SMHI weather forecast and realtime SL departures on an
   Inkplate 10 or Inkplate 5 V2 e-paper display. User settings are in config.h.
*/

#include "Inkplate.h"

#include "HTTPClient.h"
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "constants.h"

#if MONOCHROME // Defined in config.h
#define DISPLAY_MODE INKPLATE_1BIT
#else
#define DISPLAY_MODE INKPLATE_3BIT
#endif

Inkplate display(DISPLAY_MODE); // 1-bit or 3-bit (grayscale) mode, see MONOCHROME in config.h

#include "Utils/FontCollection.h"
#include "Utils/DrawUtils.h"

#include "Clock/ClockDisplay.h"
#include "Weather/WeatherClient.h"
#include "Weather/WeatherDisplay.h"
#include "PublicTransport/DeparturesClient.h"
#include "PublicTransport/DeparturesDisplay.h"
#include "Diagnostics/Diagnostics.h"
#include "Diagnostics/DiagnosticsDisplay.h"

#if defined(ARDUINO_INKPLATE10) || defined(ARDUINO_INKPLATE10V2)
static constexpr const char *networkName = "Inkplate10 Home Dashboard";

#include "Fonts/AcariSansbd9.h"
#include "Fonts/AcariSans12.h"
#include "Fonts/AcariSans16.h"
#include "Fonts/AcariSansbd16.h"
#include "Fonts/AcariSansbd50.h"

static const FontCollection fonts = {
    AcariSans_Regular16pt8b,
    AcariSans_Bold16pt8b,
    AcariSans_Bold50pt7b,
    AcariSans_Medium12pt8b,
    AcariSans_Bold9pt8b};

#elif defined(ARDUINO_INKPLATE5) || defined(ARDUINO_INKPLATE5V2)
static constexpr const char *networkName = "Inkplate5 Home Dashboard";

#include "Fonts/AcariSansbd12.h"
#include "Fonts/AcariSans20.h"
#include "Fonts/AcariSansbd20.h"
#include "Fonts/AcariSansbd50.h"

static const FontCollection fonts = {
    AcariSans_Regular20pt8b,
    AcariSans_Bold20pt8b,
    AcariSans_Bold50pt7b,
    AcariSans_Bold12pt8b,
    AcariSans_Bold12pt8b}; // The denser panel makes 12pt small enough, so no extra font is needed

#else
#error "Wrong board selection!"
#endif

// Trafiklab transport modes
static constexpr const char *trainTransportType = "TRAIN";
static constexpr const char *busTransportType = "BUS";

// Layout constants
static constexpr int leftXpos = 35;   // Left column start position
static constexpr int rightXpos = 430; // right column start position

static constexpr int rowHeight = 50;

static constexpr int busLineWidth = 80;
static constexpr int trainLineWidth = 60;

static int framesDrawn = 0;
static unsigned long lastOnlineMs = 0; // millis() of the last loop with Wi-Fi
static bool weatherFetchMissed = false; // a due weather fetch was skipped or failed

static constexpr uint32_t ntpWaitMs = 10000; // how long setup() waits for the clock

// WiFiManager settings. autoConnect() can block for all retries plus the setup
// portal, so the watchdog timeout during setup() is derived from them.
static constexpr int wifiConnectTimeoutSeconds = 30;
static constexpr int wifiConnectRetries = 10;
static constexpr int wifiPortalTimeoutSeconds = 300;
static constexpr int setupWdtTimeoutSeconds =
    wifiConnectTimeoutSeconds * wifiConnectRetries + wifiPortalTimeoutSeconds + 300;
static WeatherData weatherData; // Last fetched forecast, kept on screen between fetches
static BootInfo bootInfo;      // why the board last reset, shown after an unexpected reset
static DepartureList departures; // Reused for each stop, too large for the loop task's stack

static void fetchAndDrawDepartures(const char *siteId, const char *transportType, const char *title, int ypos, int lineBadgeWidth, bool useInformalTime);
static void restartIfDue();
static void drawStartupMessage(Inkplate &display, const FontCollection &fonts, int xpos, int ypos);

void setup()
{
    Serial.begin(115200);

    // The time zone first, so the time of the last step is logged in local time
    setenv("TZ", TIME_ZONE, 1);
    tzset();

    bootInfo = readBootInfo();
    setStep(Step::Setup);
    logBootInfo(bootInfo);

    // Watch setup() too, with a timeout long enough for WiFiManager. Without it a
    // hang in the display or Wi-Fi setup would last until someone power-cycles
    // the board.
    esp_task_wdt_init(setupWdtTimeoutSeconds, true);
    esp_task_wdt_add(NULL);

    display.begin();        // Init library (you should call this function ONLY ONCE)
    display.clearDisplay(); // Clear any data that may have been in (software) frame buffer.
                            //(NOTE! This does not clean image on screen, it only clears it in the frame buffer inside
                            // ESP32).

    drawStartupMessage(display, fonts, 100, 200);

    display.display(); // Clear everything that has previously been on a screen

    // Connect to WiFi
    setStep(Step::WifiSetup);
    WiFiManager wm;

    // Ensure the WiFi interface starts cleanly on first boot
    WiFi.mode(WIFI_OFF);
    delay(1000);

    wm.setCleanConnect(true);
    wm.setConnectTimeout(wifiConnectTimeoutSeconds);
    wm.setConnectRetries(wifiConnectRetries);
    wm.setConfigPortalTimeout(wifiPortalTimeoutSeconds); // then give up and retry from loop()

    bool res = wm.autoConnect(networkName);
    if (!res)
    {
        Serial.println("Failed to connect");
    }
    else
    {
        Serial.println("Connected to WiFi.");
        WiFi.setAutoReconnect(true);
        WiFi.persistent(true);
    }

    // WiFiManager is done: from here on nothing should block for long, so switch to
    // the normal timeout (calling init again only changes the timeout)
    esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true);
    esp_task_wdt_reset();

    // Get current time from time server. configTime() resets the time zone to
    // UTC, so set it again (see TIME_ZONE in config.h).
    configTime(0, 0, NTP_SERVER);
    setenv("TZ", TIME_ZONE, 1);
    tzset();

    // loop() doesn't wait for the clock, so give NTP a moment here. The first
    // update then shows the time and doesn't start its requests the instant
    // Wi-Fi comes up.
    struct tm timeinfo;
    if (WiFi.status() == WL_CONNECTED && !getLocalTime(&timeinfo, ntpWaitMs))
        Serial.println("Time not set yet, continuing without it.");
}

void loop()
{
    esp_task_wdt_reset();

    // Active Wi-Fi check & automatic reconnect
    bool online = true;
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi disconnected! Attempting reconnection...");
        setStep(Step::WifiReconnect);
        WiFi.reconnect();
        unsigned long startAttempt = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000)
        {
            delay(500);
            esp_task_wdt_reset();
            Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() != WL_CONNECTED)
        {
            // Keep the clock and the last weather data on screen while offline
            Serial.println("WiFi reconnection failed. Skipping network operations this cycle.");
            online = false;
        }
        else
        {
            Serial.println("WiFi reconnected successfully.");
        }
    }

    if (online)
        lastOnlineMs = millis();
    restartIfDue();

    setStep(Step::Draw);
    display.clearDisplay(); // Clear the frame buffer in ESP32 memory
    drawClock(display, fonts, leftXpos, 100);

    // Fetch weather every FULL_REFRESH_EVERY_N_UPDATES updates. A fetch that was
    // skipped while offline, or that failed, is tried again on the next update.
    bool weatherDue = framesDrawn == 0 || weatherFetchMissed;
    if (weatherDue)
    {
        setStep(Step::FetchWeather);
        bool fetched = online && fetchWeather(weatherData, WEATHER_LATITUDE, WEATHER_LONGITUDE);
        esp_task_wdt_reset();
        weatherFetchMissed = !fetched;
    }
    setStep(Step::Draw);
    drawWeather(display, fonts, weatherData, leftXpos, 180, rowHeight);

    if (online)
    {
        Serial.println("Bus information from API...");
        setStep(Step::FetchBus);
        fetchAndDrawDepartures(BUS_SITE_ID, busTransportType, BUS_TITLE, 65, busLineWidth, true);

        Serial.println("Train information from API...");
        setStep(Step::FetchTrain);
        fetchAndDrawDepartures(TRAIN_SITE_ID, trainTransportType, TRAIN_TITLE, 385, trainLineWidth, false);
    }
    else
    {
        drawOfflineMessage(display, fonts, BUS_TITLE, rightXpos, 65, rowHeight);
        drawOfflineMessage(display, fonts, TRAIN_TITLE, rightXpos, 385, rowHeight);
    }

    // Last, because the departures sections clear an area that reaches up to it
    drawResetNotice(display, fonts, bootInfo, E_INK_WIDTH - 25, 28);

    esp_task_wdt_reset();

    setStep(Step::DisplayUpdate);
#if MONOCHROME
    // For monochrome mode, always use partial display update to reduce flickering
    if (framesDrawn == 0)
    {
        display.display();
    }
    else
    {
        display.partialUpdate();
    }
#else
    // For grayscale mode, use normal display update since partial mode is not yet supported in grayscale
    display.display();
#endif

    framesDrawn = (framesDrawn + 1) % FULL_REFRESH_EVERY_N_UPDATES;

    setStep(Step::Waiting);
    Serial.printf("Waiting for next update in %d seconds...\n", REFRESH_INTERVAL_MS / 1000);
    esp_task_wdt_reset();
    delay(REFRESH_INTERVAL_MS);
    esp_task_wdt_reset();
}

// Fetches and draws one departures section, trying once more after a short pause
// if the API call fails. The watchdog is fed around every request: each one can
// take up to the HTTP timeouts in HttpUtils.cpp, all of them together could not.
static void fetchAndDrawDepartures(const char *siteId, const char *transportType, const char *title, int ypos, int lineBadgeWidth, bool useInformalTime)
{
    for (int attempt = 0; attempt < 2; attempt++)
    {
        if (attempt > 0)
            delay(1000);
        esp_task_wdt_reset();
        bool ok = fetchDepartures(departures, siteId, transportType);
        esp_task_wdt_reset();
        if (ok)
            break;
    }
    drawDepartures(display, fonts, departures, title, rightXpos, ypos, lineBadgeWidth, useInformalTime, rowHeight);
}

static void restart(const char *reason)
{
    Serial.println(reason);
    setStep(Step::PlannedRestart); // so the next boot doesn't report it as a problem
    delay(1000);
    ESP.restart();
}

// Restarts the board once a day to clear long-term heap fragmentation, and as a
// fallback when that can't work or the board can't get back online by itself.
static void restartIfDue()
{
    unsigned long uptime = millis();

    // Any time within the restart hour counts, so a slow loop can't step over it.
    // The uptime check keeps the board from restarting again in the same hour
    // once it is back up.
    struct tm timeinfo;
    if (uptime > MIN_UPTIME_BEFORE_RESTART_MS && getLocalTime(&timeinfo, 0) &&
        timeinfo.tm_hour == DAILY_RESTART_HOUR)
        restart("Performing scheduled daily maintenance restart...");

    // Without a clock (NTP never synced) the daily restart never happens
    if (uptime > MAX_UPTIME_MS)
        restart("Uptime limit reached, restarting...");

    // A restart runs WiFiManager again, which also reopens the setup portal
    if (uptime - lastOnlineMs > MAX_OFFLINE_MS)
        restart("Offline for too long, restarting...");
}

static void drawStartupMessage(Inkplate &display, const FontCollection &fonts, int xpos, int ypos)
{
    display.setTextColor(_BLACK, _WHITE);

    display.setCursor(xpos, ypos);
    display.setFont(&fonts.boldTextFont);
    display.println("Ansluter till WiFi...");
    ypos += rowHeight;

    display.setCursor(xpos, ypos);
    display.setFont(&fonts.normalTextFont);
    int wrappedLines = drawWrappedText(display, fonts.normalTextFont, "Vid problem, st\xe4ll in WiFi via det tillf\xe4lliga n\xe4tverket:", xpos, ypos, E_INK_WIDTH - xpos - 25);
    ypos += rowHeight * wrappedLines;

    display.setCursor(xpos, ypos);
    display.setFont(&fonts.boldTextFont);
    display.println(networkName);
}
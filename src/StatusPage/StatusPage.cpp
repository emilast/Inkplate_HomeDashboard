#include "StatusPage.h"
#include <Arduino.h>
#include <stdarg.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <esp_heap_caps.h>
#include <freertos/semphr.h>

static constexpr int serverPort = 80;
static constexpr uint32_t taskStackSize = 6144;
static constexpr int pageSize = 3072;

static WebServer server(serverPort);
static SemaphoreHandle_t statusLock = nullptr;
static DashboardStatus publishedStatus = {}; // guarded by statusLock
static char page[pageSize];                  // only used by the server task

static void formatTime(time_t t, char *buf, size_t size)
{
    if (t == 0)
    {
        snprintf(buf, size, "-");
        return;
    }
    struct tm local;
    localtime_r(&t, &local);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", &local);
}

static void formatDuration(unsigned long seconds, char *buf, size_t size)
{
    unsigned long days = seconds / 86400;
    seconds %= 86400;
    snprintf(buf, size, "%lu d %02lu:%02lu:%02lu", days, seconds / 3600, (seconds / 60) % 60, seconds % 60);
}

// Appends to page at *len, never past its end
static void append(int *len, const char *format, ...)
{
    if (*len >= pageSize - 1)
        return;
    va_list args;
    va_start(args, format);
    int written = vsnprintf(page + *len, pageSize - *len, format, args);
    va_end(args);
    if (written > 0)
        *len += written;
    if (*len > pageSize - 1)
        *len = pageSize - 1;
}

// Escapes &, < and > so API error texts can't break the page
static void appendEscaped(int *len, const char *text)
{
    for (const char *c = text; *c != '\0'; c++)
    {
        switch (*c)
        {
        case '&':
            append(len, "&amp;");
            break;
        case '<':
            append(len, "&lt;");
            break;
        case '>':
            append(len, "&gt;");
            break;
        default:
            append(len, "%c", *c);
        }
    }
}

static void appendSource(int *len, const char *name, const SourceStatus &source)
{
    char attempt[24], success[24];
    formatTime(source.lastAttempt, attempt, sizeof(attempt));
    formatTime(source.lastSuccess, success, sizeof(success));
    append(len, "<tr><th>%s</th><td>", name);
    if (!source.attempted)
        append(len, "not fetched yet");
    else if (source.lastOk)
        append(len, "OK %s", attempt);
    else
    {
        append(len, "<b>error</b> %s: ", attempt);
        appendEscaped(len, source.lastError);
        append(len, "<br>last OK %s", success);
    }
    append(len, "</td></tr>");
}

static void handleRoot()
{
    DashboardStatus status;
    xSemaphoreTake(statusLock, portMAX_DELAY);
    status = publishedStatus;
    xSemaphoreGive(statusLock);

    char uptime[32], stepFor[32], now[24], stepTime[24];
    unsigned long nowMs = millis();
    formatDuration(nowMs / 1000, uptime, sizeof(uptime));
    formatDuration((nowMs - currentStepStartedMs()) / 1000, stepFor, sizeof(stepFor));
    formatTime(time(nullptr) > 1600000000 ? time(nullptr) : 0, now, sizeof(now));

    int len = 0;
    append(&len, "<!doctype html><html><head><meta charset=\"iso-8859-1\">"
                 "<meta name=\"viewport\" content=\"width=device-width\">"
                 "<meta http-equiv=\"refresh\" content=\"10\"><title>Inkplate status</title>"
                 "<style>body{font-family:sans-serif;margin:16px}th{text-align:left;padding-right:16px;vertical-align:top}"
                 "td,th{padding-bottom:6px}</style></head><body><h1>Inkplate Home Dashboard</h1><table>");
    append(&len, "<tr><th>Time</th><td>%s</td></tr>", now);
    append(&len, "<tr><th>Uptime</th><td>%s</td></tr>", uptime);
    append(&len, "<tr><th>Current step</th><td>%s, for %s</td></tr>", stepName(currentStep()), stepFor);
    append(&len, "<tr><th>Wi-Fi</th><td>%s, %d dBm, %s</td></tr>",
           WiFi.SSID().c_str(), WiFi.RSSI(), WiFi.localIP().toString().c_str());
    append(&len, "<tr><th>Memory</th><td>internal %u kB free (largest block %u kB, lowest %u kB), PSRAM %u kB free</td></tr>",
           (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
           (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024),
           (unsigned)(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / 1024),
           (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
    appendSource(&len, "Weather", status.weather);
    appendSource(&len, "SL bus", status.bus);
    appendSource(&len, "SL train", status.train);

    append(&len, "<tr><th>Last reset</th><td>%s, boot %u since power-on",
           resetReasonName(status.boot.resetReason), (unsigned)status.boot.bootCount);
    if (status.boot.hasPreviousStep)
    {
        char when[24];
        formatTime(status.boot.previousStepTime, when, sizeof(when));
        append(&len, "<br>last step before it: %s (started %s)", stepName(status.boot.previousStep), when);
    }
    append(&len, "</td></tr><tr><th>Firmware</th><td>built %s %s</td></tr></table></body></html>", __DATE__, __TIME__);

    // ISO-8859-1 because the API error texts are stored that way for the display fonts
    server.send(200, "text/html; charset=iso-8859-1", page);
}

static void serverTask(void *)
{
    for (;;)
    {
        server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void startStatusPage(const char *hostname)
{
    statusLock = xSemaphoreCreateMutex();
    server.on("/", handleRoot);
    server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
    server.begin();
    if (!MDNS.begin(hostname))
        Serial.println("mDNS failed to start, use the IP address for the status page");
    MDNS.addService("http", "tcp", serverPort);

    // Core 0, next to the Wi-Fi stack, so it keeps answering while loop() on core 1 is busy or stuck
    xTaskCreatePinnedToCore(serverTask, "statusPage", taskStackSize, nullptr, 1, nullptr, 0);
    Serial.printf("Status page: http://%s.local/ or http://%s/\n", hostname, WiFi.localIP().toString().c_str());
}

void publishStatus(const DashboardStatus &status)
{
    if (statusLock == nullptr)
        return;
    xSemaphoreTake(statusLock, portMAX_DELAY);
    publishedStatus = status;
    xSemaphoreGive(statusLock);
}

void recordFetch(SourceStatus &source, bool ok, const char *error)
{
    time_t now = time(nullptr);
    if (now < 1600000000)
        now = 0;
    source.attempted = true;
    source.lastAttempt = now;
    source.lastOk = ok;
    if (ok)
    {
        source.lastSuccess = now;
        source.lastError[0] = '\0';
    }
    else
    {
        snprintf(source.lastError, sizeof(source.lastError), "%s", error ? error : "failed");
    }
}

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

// Memory history for the diagrams: one sample a minute for the last 4 hours, in kB.
// Sampled by the server task, so it keeps going while loop() is stuck.
static constexpr int historyLength = 240;
static constexpr unsigned long sampleIntervalMs = 60UL * 1000UL;

struct MemorySample
{
    uint16_t internalFree;
    uint16_t internalLargest;
    uint16_t psramFree;
};

static MemorySample history[historyLength]; // ring buffer, only used by the server task
static int historyCount = 0;
static int historyNext = 0;
static unsigned long lastSampleMs = 0;

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

// Sends what is in page as one chunk of the response
static void flushPage(int *len)
{
    if (*len > 0)
        server.sendContent(page, *len);
    *len = 0;
}

// Appends to page at *len, sending the page so far when it is full, so the
// response can be larger than the buffer
static void append(int *len, const char *format, ...)
{
    for (int attempt = 0; attempt < 2; attempt++)
    {
        va_list args;
        va_start(args, format);
        int written = vsnprintf(page + *len, pageSize - *len, format, args);
        va_end(args);
        if (written < 0)
            return;
        if (*len + written < pageSize)
        {
            *len += written;
            return;
        }
        if (*len == 0)
        {
            *len = pageSize - 1; // longer than the whole buffer: truncated
            return;
        }
        page[*len] = '\0'; // drop the partial write and retry in an empty buffer
        flushPage(len);
    }
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

static uint16_t toKb(size_t bytes)
{
    size_t kb = bytes / 1024;
    return kb > 0xffff ? 0xffff : (uint16_t)kb;
}

static void sampleMemory()
{
    MemorySample &sample = history[historyNext];
    sample.internalFree = toKb(heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    sample.internalLargest = toKb(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    sample.psramFree = toKb(heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    historyNext = (historyNext + 1) % historyLength;
    if (historyCount < historyLength)
        historyCount++;
    lastSampleMs = millis();
}

// The i-th sample, oldest first
static const MemorySample &historySample(int i)
{
    int oldest = (historyNext - historyCount + historyLength) % historyLength;
    return history[(oldest + i) % historyLength];
}

struct ChartSeries
{
    const char *label;
    const char *color;
    uint16_t MemorySample::*field;
};

// Chart geometry in SVG units; the SVG scales to the page width
static constexpr int chartWidth = 600;
static constexpr int chartHeight = 192;
static constexpr int plotLeft = 70;
static constexpr int plotRight = 590;
static constexpr int plotTop = 10;
static constexpr int plotBottom = 162;

static int chartX(int ageMinutes)
{
    return plotRight - (plotRight - plotLeft) * ageMinutes / (historyLength - 1);
}

static int chartY(int valueKb, int maxKb)
{
    return plotBottom - (plotBottom - plotTop) * valueKb / maxKb;
}

// A line chart of the history, newest on the right. lowestKb draws a dashed
// reference line when >= 0.
static void appendChart(int *len, const char *title, const ChartSeries *series, int seriesCount, int lowestKb, const char *lowestLabel)
{
    int maxKb = lowestKb > 0 ? lowestKb : 1;
    for (int i = 0; i < historyCount; i++)
        for (int s = 0; s < seriesCount; s++)
            maxKb = max(maxKb, (int)(historySample(i).*series[s].field));
    // 10 % headroom, rounded up to a round number
    int step = maxKb > 1000 ? 100 : 10;
    maxKb = (maxKb * 11 / 10 / step + 1) * step;

    append(len, "<h2>%s</h2><svg viewBox=\"0 0 %d %d\" style=\"width:100%%;max-width:%dpx\" font-size=\"15\">",
           title, chartWidth, chartHeight, chartWidth);

    // Horizontal grid lines with kB labels
    for (int g = 0; g <= 2; g++)
    {
        int value = maxKb * g / 2;
        int y = chartY(value, maxKb);
        append(len, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"#ddd\"/>"
                    "<text x=\"%d\" y=\"%d\" text-anchor=\"end\">%d kB</text>",
               plotLeft, y, plotRight, y, plotLeft - 4, y + 4, value);
    }

    // Time labels every hour, as clock times when the clock is set
    time_t now = time(nullptr);
    bool clockSet = now > 1600000000;
    for (int hoursAgo = 4; hoursAgo >= 0; hoursAgo--)
    {
        char label[8];
        if (clockSet)
        {
            time_t t = now - hoursAgo * 3600;
            struct tm local;
            localtime_r(&t, &local);
            strftime(label, sizeof(label), "%H:%M", &local);
        }
        else
        {
            snprintf(label, sizeof(label), hoursAgo == 0 ? "now" : "-%d h", hoursAgo);
        }
        int ageMinutes = min(hoursAgo * 60, historyLength - 1);
        // The newest label ends at the right edge so it isn't cut off
        append(len, "<text x=\"%d\" y=\"%d\" text-anchor=\"%s\">%s</text>",
               chartX(ageMinutes), plotBottom + 20, hoursAgo == 0 ? "end" : "middle", label);
    }

    if (lowestKb >= 0)
    {
        int y = chartY(lowestKb, maxKb);
        append(len, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"#d62728\" stroke-dasharray=\"6 4\"/>",
               plotLeft, y, plotRight, y);
    }

    for (int s = 0; s < seriesCount; s++)
    {
        append(len, "<polyline fill=\"none\" stroke=\"%s\" stroke-width=\"2\" points=\"", series[s].color);
        for (int i = 0; i < historyCount; i++)
        {
            int ageMinutes = historyCount - 1 - i;
            append(len, "%d,%d ", chartX(ageMinutes), chartY(historySample(i).*series[s].field, maxKb));
        }
        append(len, "\"/>");
    }
    append(len, "</svg><div>");

    for (int s = 0; s < seriesCount; s++)
        append(len, "<span style=\"color:%s\">&#9632;</span> %s &nbsp; ", series[s].color, series[s].label);
    if (lowestKb >= 0)
        append(len, "<span style=\"color:#d62728\">- -</span> %s (%d kB)", lowestLabel, lowestKb);
    append(len, "</div>");
}

static void appendMemoryCharts(int *len)
{
    if (historyCount < 2)
        return;

    static const ChartSeries internalSeries[] = {
        {"free", "#1f77b4", &MemorySample::internalFree},
        {"largest free block", "#ff7f0e", &MemorySample::internalLargest},
    };
    static const ChartSeries psramSeries[] = {
        {"free", "#2ca02c", &MemorySample::psramFree},
    };

    if (historyCount < historyLength)
        append(len, "<p>Memory history covers the %d minutes since the status page started.</p>", historyCount);
    appendChart(len, "Internal RAM", internalSeries, 2, toKb(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)), "lowest free since boot");
    appendChart(len, "PSRAM", psramSeries, 1, -1, nullptr);
}

static void handleRoot()
{
    DashboardStatus status;
    xSemaphoreTake(statusLock, portMAX_DELAY);
    status = publishedStatus;
    xSemaphoreGive(statusLock);

    char uptime[32], stepFor[32], now[24];
    unsigned long nowMs = millis();
    formatDuration(nowMs / 1000, uptime, sizeof(uptime));
    formatDuration((nowMs - currentStepStartedMs()) / 1000, stepFor, sizeof(stepFor));
    formatTime(time(nullptr) > 1600000000 ? time(nullptr) : 0, now, sizeof(now));

    // Sent in chunks, since the page with the diagrams is larger than the buffer.
    // ISO-8859-1 because the API error texts are stored that way for the display fonts.
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html; charset=iso-8859-1", "");

    int len = 0;
    append(&len, "<!doctype html><html><head><meta charset=\"iso-8859-1\">"
                 "<meta name=\"viewport\" content=\"width=device-width\">"
                 "<meta http-equiv=\"refresh\" content=\"10\"><title>Inkplate status</title>"
                 "<style>body{font-family:sans-serif;margin:16px}th{text-align:left;padding-right:16px;vertical-align:top}"
                 "td,th{padding-bottom:6px}h2{font-size:1.1em;margin:20px 0 4px}</style></head><body><h1>Inkplate Home Dashboard</h1><table>");
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
    append(&len, "</td></tr><tr><th>Firmware</th><td>built %s %s</td></tr></table>", __DATE__, __TIME__);

    appendMemoryCharts(&len);
    append(&len, "</body></html>");
    flushPage(&len);
    server.sendContent("", 0); // ends the chunked response
}

static void serverTask(void *)
{
    for (;;)
    {
        if (historyCount == 0 || millis() - lastSampleMs >= sampleIntervalMs)
            sampleMemory();
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

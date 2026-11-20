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

// History for the diagrams: one sample a minute for the last 4 hours, with memory in
// kB and the number of failed requests in that minute. Sampled by the server task,
// so it keeps going while loop() is stuck.
static constexpr int historyLength = 240;
static constexpr unsigned long sampleIntervalMs = 60UL * 1000UL;

struct MemorySample
{
    uint16_t internalFree;
    uint16_t internalLargest;
    uint16_t psramFree;
    uint8_t failures[DATA_SOURCE_COUNT];
};

static MemorySample history[historyLength]; // ring buffer, only used by the server task
static int historyCount = 0;
static int historyNext = 0;
static unsigned long lastSampleMs = 0;

// Failed requests since the last sample, per source. Written by loop(), moved into
// the history by the server task; guarded by statusLock.
static uint8_t pendingFailures[DATA_SOURCE_COUNT] = {};

static const char *const sourceNames[DATA_SOURCE_COUNT] = {"weather", "SL bus", "SL train"};
static const char *const sourceColors[DATA_SOURCE_COUNT] = {"#17becf", "#9467bd", "#8c564b"};

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

// Number of signal bars (0-4) for an RSSI, using the usual Wi-Fi thresholds
static int signalBars(int rssi)
{
    if (rssi >= -55)
        return 4;
    if (rssi >= -67)
        return 3;
    if (rssi >= -75)
        return 2;
    if (rssi >= -85)
        return 1;
    return 0;
}

static const char *signalQuality(int bars)
{
    static const char *const names[] = {"very weak", "weak", "fair", "good", "excellent"};
    return names[bars];
}

// A small four-bar gauge, sized to sit on a line of text
static void appendSignalGauge(int *len, int rssi, bool connected)
{
    int bars = connected ? signalBars(rssi) : 0;
    append(len, "<svg width=\"20\" height=\"14\" viewBox=\"0 0 20 14\" style=\"vertical-align:-2px;margin-right:6px\">"
                "<title>%s</title>",
           connected ? signalQuality(bars) : "not connected");
    for (int i = 0; i < 4; i++)
    {
        int height = 5 + i * 3;
        append(len, "<rect x=\"%d\" y=\"%d\" width=\"3.5\" height=\"%d\" rx=\"1\" fill=\"%s\"/>",
               i * 5, 14 - height, height, i < bars ? "#333" : "#ccc");
    }
    append(len, "</svg>");
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
    {
        append(len, "OK %s", attempt);
        if (source.failedRequests > 0)
        {
            char failure[24];
            formatTime(source.lastFailure, failure, sizeof(failure));
            append(len, "<br>last failed request %s: ", failure);
            appendEscaped(len, source.lastFailureError);
        }
    }
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
    xSemaphoreTake(statusLock, portMAX_DELAY);
    for (int s = 0; s < DATA_SOURCE_COUNT; s++)
    {
        sample.failures[s] = pendingFailures[s];
        pendingFailures[s] = 0;
    }
    xSemaphoreGive(statusLock);
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
static constexpr int failureChartHeight = 132;
static constexpr int failurePlotBottom = 102; // same plotTop, plotLeft and plotRight

static int chartX(int ageMinutes)
{
    return plotRight - (plotRight - plotLeft) * ageMinutes / (historyLength - 1);
}

static int chartY(int valueKb, int maxKb)
{
    return plotBottom - (plotBottom - plotTop) * valueKb / maxKb;
}

static int sampleFailures(const MemorySample &sample)
{
    int total = 0;
    for (int s = 0; s < DATA_SOURCE_COUNT; s++)
        total += sample.failures[s];
    return total;
}

// Time labels every hour along the bottom of a chart, as clock times when the
// clock is set. Shared by all charts so they line up.
static void appendTimeAxis(int *len, int y)
{
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
               chartX(ageMinutes), y, hoursAgo == 0 ? "end" : "middle", label);
    }
}

// A line chart of the history, newest on the right. lowestKb draws a dashed
// reference line when >= 0. failureMarkers shades the minutes with failed requests.
static void appendChart(int *len, const char *title, const ChartSeries *series, int seriesCount, int lowestKb, const char *lowestLabel, bool failureMarkers)
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

    appendTimeAxis(len, plotBottom + 20);

    if (failureMarkers)
    {
        for (int i = 0; i < historyCount; i++)
        {
            if (sampleFailures(historySample(i)) == 0)
                continue;
            int x = chartX(historyCount - 1 - i);
            append(len, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"#f4c2c2\" stroke-width=\"2\"/>",
                   x, plotTop, x, plotBottom);
        }
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
        append(len, "<span style=\"color:#d62728\">- -</span> %s (%d kB) &nbsp; ", lowestLabel, lowestKb);
    if (failureMarkers)
        append(len, "<span style=\"color:#f4c2c2\">|</span> minutes with failed requests");
    append(len, "</div>");
}

// Failed requests per minute as stacked bars, on the same time axis as the memory
// charts so the two can be compared
static void appendFailureChart(int *len)
{
    int maxCount = 1;
    for (int i = 0; i < historyCount; i++)
        maxCount = max(maxCount, sampleFailures(historySample(i)));

    append(len, "<h2>Failed requests per minute</h2><svg viewBox=\"0 0 %d %d\" style=\"width:100%%;max-width:%dpx\" font-size=\"15\">",
           chartWidth, failureChartHeight, chartWidth);

    int plotHeight = failurePlotBottom - plotTop;
    for (int value : {0, maxCount})
    {
        int y = failurePlotBottom - plotHeight * value / maxCount;
        append(len, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"#ddd\"/>"
                    "<text x=\"%d\" y=\"%d\" text-anchor=\"end\">%d</text>",
               plotLeft, y, plotRight, y, plotLeft - 4, y + 4, value);
    }
    appendTimeAxis(len, failurePlotBottom + 20);

    for (int i = 0; i < historyCount; i++)
    {
        const MemorySample &sample = historySample(i);
        int x = chartX(historyCount - 1 - i) - 1;
        int y = failurePlotBottom;
        for (int s = 0; s < DATA_SOURCE_COUNT; s++)
        {
            if (sample.failures[s] == 0)
                continue;
            int height = plotHeight * sample.failures[s] / maxCount;
            y -= height;
            append(len, "<rect x=\"%d\" y=\"%d\" width=\"3\" height=\"%d\" fill=\"%s\"/>", x, y, height, sourceColors[s]);
        }
    }
    append(len, "</svg><div>");
    for (int s = 0; s < DATA_SOURCE_COUNT; s++)
        append(len, "<span style=\"color:%s\">&#9632;</span> %s &nbsp; ", sourceColors[s], sourceNames[s]);
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
        append(len, "<p>The history covers the %d minutes since the status page started.</p>", historyCount);
    appendChart(len, "Internal RAM", internalSeries, 2, toKb(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)), "lowest free since boot", true);
    appendFailureChart(len);
    appendChart(len, "PSRAM", psramSeries, 1, -1, nullptr, false);
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
    bool connected = WiFi.status() == WL_CONNECTED;
    int rssi = WiFi.RSSI();
    append(&len, "<tr><th>Wi-Fi</th><td>");
    appendSignalGauge(&len, rssi, connected);
    if (connected)
        append(&len, "%s, %d dBm (%s), %s</td></tr>",
               WiFi.SSID().c_str(), rssi, signalQuality(signalBars(rssi)), WiFi.localIP().toString().c_str());
    else
        append(&len, "not connected</td></tr>");
    append(&len, "<tr><th>Memory</th><td>internal %u kB free (largest block %u kB, lowest %u kB), PSRAM %u kB free</td></tr>",
           (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
           (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024),
           (unsigned)(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / 1024),
           (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
    appendSource(&len, "Weather", status.weather);
    appendSource(&len, "SL bus", status.bus);
    appendSource(&len, "SL train", status.train);

    const SourceStatus *sources[DATA_SOURCE_COUNT] = {&status.weather, &status.bus, &status.train};
    uint32_t totalFailed = 0;
    for (const SourceStatus *source : sources)
        totalFailed += source->failedRequests;
    int recentFailed = 0;
    for (int i = 0; i < historyCount; i++)
        recentFailed += sampleFailures(historySample(i));
    append(&len, "<tr><th>Failed requests</th><td>%u since boot, %d in the last %d min<br>",
           (unsigned)totalFailed, recentFailed, historyCount);
    for (int s = 0; s < DATA_SOURCE_COUNT; s++)
    {
        append(&len, "%s %u", sourceNames[s], (unsigned)sources[s]->failedRequests);
        if (sources[s]->recoveredByRetry > 0)
            append(&len, " (%u OK on retry)", (unsigned)sources[s]->recoveredByRetry);
        append(&len, s < DATA_SOURCE_COUNT - 1 ? ", " : "</td></tr>");
    }

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

static time_t currentTimeOrZero()
{
    time_t now = time(nullptr);
    return now > 1600000000 ? now : 0;
}

void recordFailedRequest(SourceStatus &source, DataSource which, const char *error)
{
    source.failedRequests++;
    source.lastFailure = currentTimeOrZero();
    snprintf(source.lastFailureError, sizeof(source.lastFailureError), "%s", error ? error : "failed");

    if (statusLock == nullptr)
        return; // status page disabled: no history to add to
    xSemaphoreTake(statusLock, portMAX_DELAY);
    uint8_t &pending = pendingFailures[(int)which];
    if (pending < 255)
        pending++;
    xSemaphoreGive(statusLock);
}

void recordFetch(SourceStatus &source, bool ok, const char *error)
{
    time_t now = currentTimeOrZero();
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

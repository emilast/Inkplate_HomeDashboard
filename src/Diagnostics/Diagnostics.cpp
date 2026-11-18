#include "Diagnostics.h"
#include <Arduino.h>
#include <esp_attr.h>

static constexpr uint32_t recordMagic = 0x44494147; // "DIAG"

struct StepRecord
{
    uint32_t magic;
    uint8_t step;
    time_t stepTime;
    uint32_t bootCount;
};

// Not cleared on reset, so it still holds the last step after a watchdog reset
RTC_NOINIT_ATTR static StepRecord record;

static time_t currentTimeOrZero()
{
    time_t now = time(nullptr);
    // Before NTP has synced the clock counts from 1970
    return now > 1600000000 ? now : 0;
}

BootInfo readBootInfo()
{
    BootInfo info = {};
    info.resetReason = esp_reset_reason();

    bool valid = record.magic == recordMagic && info.resetReason != ESP_RST_POWERON &&
                 record.step <= (uint8_t)Step::PlannedRestart;
    if (valid)
    {
        info.hasPreviousStep = true;
        info.previousStep = (Step)record.step;
        info.previousStepTime = record.stepTime;
        record.bootCount++;
    }
    else
    {
        record.magic = recordMagic;
        record.bootCount = 1;
    }
    info.bootCount = record.bootCount;
    return info;
}

void setStep(Step step)
{
    record.step = (uint8_t)step;
    record.stepTime = currentTimeOrZero();
}

bool isUnexpectedReset(const BootInfo &info)
{
    switch (info.resetReason)
    {
    case ESP_RST_POWERON:
    case ESP_RST_EXT:
    case ESP_RST_DEEPSLEEP:
        return false;
    case ESP_RST_SW:
        return !info.hasPreviousStep || info.previousStep != Step::PlannedRestart;
    default:
        return true;
    }
}

const char *stepName(Step step)
{
    switch (step)
    {
    case Step::Setup:
        return "setup";
    case Step::WifiSetup:
        return "Wi-Fi setup";
    case Step::WifiReconnect:
        return "Wi-Fi reconnect";
    case Step::FetchWeather:
        return "fetching weather";
    case Step::FetchBus:
        return "fetching SL bus";
    case Step::FetchTrain:
        return "fetching SL train";
    case Step::Draw:
        return "drawing";
    case Step::DisplayUpdate:
        return "display update";
    case Step::Waiting:
        return "waiting";
    case Step::PlannedRestart:
        return "planned restart";
    }
    return "unknown";
}

const char *resetReasonName(esp_reset_reason_t reason)
{
    switch (reason)
    {
    case ESP_RST_POWERON:
        return "power-on";
    case ESP_RST_EXT:
        return "external reset";
    case ESP_RST_SW:
        return "software restart";
    case ESP_RST_PANIC:
        return "panic";
    case ESP_RST_INT_WDT:
        return "interrupt watchdog";
    case ESP_RST_TASK_WDT:
        return "task watchdog";
    case ESP_RST_WDT:
        return "other watchdog";
    case ESP_RST_DEEPSLEEP:
        return "deep sleep";
    case ESP_RST_BROWNOUT:
        return "brownout";
    case ESP_RST_SDIO:
        return "SDIO";
    default:
        return "unknown";
    }
}

void logBootInfo(const BootInfo &info)
{
    Serial.printf("Reset reason: %s, boot %u since power-on\n", resetReasonName(info.resetReason), (unsigned)info.bootCount);
    if (!info.hasPreviousStep)
        return;

    char when[32] = "time unknown";
    if (info.previousStepTime != 0)
    {
        struct tm t;
        localtime_r(&info.previousStepTime, &t);
        strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", &t);
    }
    Serial.printf("Last step before reset: %s (started %s)\n", stepName(info.previousStep), when);
}

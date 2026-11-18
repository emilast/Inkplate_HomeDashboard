#pragma once

// Records what the board is doing so that, after a watchdog or panic reset, it can
// tell why it reset and where it was. The current step is kept in RTC memory,
// which survives every reset except power-on. That makes it the one piece of
// module state that can't live in main.ino.

#include <stdint.h>
#include <time.h>
#include <esp_system.h>

enum class Step : uint8_t
{
    Setup,
    WifiSetup,
    WifiReconnect,
    FetchWeather,
    FetchBus,
    FetchTrain,
    Draw,
    DisplayUpdate,
    Waiting,
    PlannedRestart,
};

struct BootInfo
{
    esp_reset_reason_t resetReason;
    bool hasPreviousStep;    // false after power-on, when RTC memory holds no record
    Step previousStep;       // the step the board was in when it reset
    time_t previousStepTime; // when that step started, 0 if the clock wasn't set
    uint32_t bootCount;      // boots since power-on, including this one
};

// Reads why the board reset and the step it was in. Call first in setup(), before
// setStep().
BootInfo readBootInfo();

// Records the current step. Cheap enough to call before every operation.
void setStep(Step step);

// True for resets that point to a problem: watchdog, panic, brownout, or a
// software restart the dashboard didn't plan.
bool isUnexpectedReset(const BootInfo &info);

const char *stepName(Step step);                    // English, for logs
const char *resetReasonName(esp_reset_reason_t r); // English, for logs

void logBootInfo(const BootInfo &info);

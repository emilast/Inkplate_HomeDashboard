#include "DiagnosticsDisplay.h"
#include "../Utils/DrawUtils.h"

static const char *resetReasonText(esp_reset_reason_t reason)
{
    switch (reason)
    {
    case ESP_RST_PANIC:
        return "krasch";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
        return "watchdog";
    case ESP_RST_BROWNOUT:
        return "sp\xe4nningsfall";
    case ESP_RST_SW:
        return "omstart";
    default:
        return "ok\xe4nd orsak";
    }
}

static const char *stepText(Step step)
{
    switch (step)
    {
    case Step::Setup:
        return "uppstart";
    case Step::WifiSetup:
        return "WiFi-anslutning";
    case Step::WifiReconnect:
        return "WiFi-\xe5teranslutning";
    case Step::FetchWeather:
        return "v\xe4" "der";
    case Step::FetchBus:
        return "SL buss";
    case Step::FetchTrain:
        return "SL t\xe5g";
    case Step::Draw:
        return "ritning";
    case Step::DisplayUpdate:
        return "sk\xe4rmuppdatering";
    case Step::Waiting:
        return "v\xe4ntan";
    case Step::PlannedRestart:
        return "planerad omstart";
    }
    return "?";
}

void drawResetNotice(Inkplate &display, const FontCollection &fonts, const BootInfo &info, int rightX, int ypos)
{
    if (!isUnexpectedReset(info))
        return;

    // e.g. "Omstart 03:12: watchdog (SL tåg)"
    char when[8] = "";
    if (info.hasPreviousStep && info.previousStepTime != 0)
    {
        struct tm t;
        localtime_r(&info.previousStepTime, &t);
        strftime(when, sizeof(when), " %H:%M", &t);
    }
    char text[96];
    if (info.hasPreviousStep)
        snprintf(text, sizeof(text), "Omstart%s: %s (%s)", when, resetReasonText(info.resetReason), stepText(info.previousStep));
    else
        snprintf(text, sizeof(text), "Omstart: %s", resetReasonText(info.resetReason));

    display.setTextColor(_BLACK, _WHITE);
    drawRightString(display, fonts.smallTextFont, text, rightX, ypos, false);
}

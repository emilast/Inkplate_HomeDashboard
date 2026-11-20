#include "DisplayPower.h"
#include <Wire.h>

static constexpr uint8_t pmicAddress = 0x48; // TPS65186 on the Inkplate 10 and 5 V2
static constexpr uint8_t int1Register = 0x07;
static constexpr uint8_t int2Register = 0x08;
static constexpr uint8_t powerGoodRegister = 0x0F;
static constexpr int retryDelayMs = 500;

static int readPmicRegister(uint8_t reg)
{
    Wire.beginTransmission(pmicAddress);
    Wire.write(reg);
    if (Wire.endTransmission() != 0)
        return -1;
    if (Wire.requestFrom(pmicAddress, (uint8_t)1) != 1)
        return -1;
    return Wire.read();
}

// einkOn() puts the power chip back to sleep when it fails, and it doesn't answer
// while asleep, so wake it briefly to read why. Reading the fault flags also
// clears them, which may let the retry succeed.
static void readPmicStatus(Inkplate &display, DisplayPowerCheck &check)
{
    display.digitalWriteInternal(IO_INT_ADDR, display.ioRegsInt, WAKEUP, HIGH);
    delay(5);
    check.powerGood = readPmicRegister(powerGoodRegister);
    check.int1 = readPmicRegister(int1Register);
    check.int2 = readPmicRegister(int2Register);
    display.digitalWriteInternal(IO_INT_ADDR, display.ioRegsInt, WAKEUP, LOW);
}

DisplayPowerCheck powerUpDisplay(Inkplate &display)
{
    DisplayPowerCheck check = {false, 0, -1, -1, -1};
    for (int attempt = 1; attempt <= 2; attempt++)
    {
        check.attempts = attempt;
        if (display.einkOn())
        {
            check.ok = true;
            return check;
        }
        readPmicStatus(display, check);
        Serial.printf("Display power failed (attempt %d): power good 0x%02X, faults 0x%02X 0x%02X (-1 = no answer)\n",
                      attempt, check.powerGood, check.int1, check.int2);
        if (attempt == 1)
            delay(retryDelayMs);
    }
    return check;
}

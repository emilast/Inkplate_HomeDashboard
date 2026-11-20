#pragma once

// Checks that the e-paper panel's power supply comes up before a screen update.
// Inkplate::display() quietly does nothing when it doesn't, which leaves an old
// image on screen while the board otherwise keeps running.

#include <Inkplate.h>

struct DisplayPowerCheck
{
    bool ok;       // the panel power came up
    int attempts;  // 1, or 2 when the first attempt failed
    // TPS65186 power chip registers read after a failed attempt, -1 if it didn't
    // answer (which points to the I2C bus or the I/O expander instead)
    int powerGood; // 0xFA when all rails are up
    int int1;      // latched fault flags, cleared by reading them
    int int2;
};

// Powers up the panel for an update, trying once more after a failure. When ok
// is true the panel is on and display() or partialUpdate() can follow.
DisplayPowerCheck powerUpDisplay(Inkplate &display);

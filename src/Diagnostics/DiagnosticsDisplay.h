#pragma once

#include <Inkplate.h>
#include "../Utils/FontCollection.h"
#include "Diagnostics.h"

// After an unexpected reset, draws a short note with the reason and the step the
// board was in, right-aligned at (rightX, ypos). Draws nothing otherwise.
void drawResetNotice(Inkplate &display, const FontCollection &fonts, const BootInfo &info, int rightX, int ypos);

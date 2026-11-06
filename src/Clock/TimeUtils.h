#pragma once
#include <Arduino.h>
#include "../Utils/FontCollection.h"

void printLocalTime(Inkplate &display, const FontCollection &fonts, int xpos, int ypos);

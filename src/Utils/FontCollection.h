#pragma once
#include <Inkplate.h>

// The four fonts the dashboard uses, picked per board in main.ino
struct FontCollection
{
    const GFXfont &normalTextFont;
    const GFXfont &boldTextFont;
    const GFXfont &largeTextFont;
    const GFXfont &smallTextFont;
};

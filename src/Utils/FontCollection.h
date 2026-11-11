#pragma once
#include <Inkplate.h>

// The fonts the dashboard uses, picked per board in main.ino
struct FontCollection
{
    const GFXfont &normalTextFont;
    const GFXfont &boldTextFont;
    const GFXfont &largeTextFont;
    const GFXfont &smallTextFont;
    const GFXfont &tinyTextFont; // only for short numeric labels such as the daily precipitation
};

#pragma once

#include "config.h" // MONOCHROME

#if MONOCHROME
    // Use the same definitions as in Inkplate library for black and white mode
    #define _BLACK BLACK
    #define _WHITE WHITE
#else
    // In 3-bit mode, define black as 0 and white as 7. 1-6 are shades of gray
    // with 1 being the darkest and 6 the lightest.
    #define _BLACK 0
    #define _GRAY1 1
    #define _GRAY2 2
    #define _GRAY3 3
    #define _GRAY4 4
    #define _GRAY5 5
    #define _GRAY6 6
    #define _WHITE 7
#endif


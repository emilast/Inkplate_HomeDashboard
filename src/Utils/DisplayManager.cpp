#include "DisplayManager.h"

void drawHeader(Inkplate &display, const GFXfont &font, String text, int xpos, int ypos)
{
    display.setCursor(xpos, ypos);
    display.setFont(&font);
    display.print(text);
}

int drawRightString(Inkplate &display, const GFXfont &font, const char *buf, int x, int y, bool strikethrough)
{
    display.setFont(&font);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(buf, x, y, &x1, &y1, &w, &h);
    display.setCursor(x - w, y);
    if (strikethrough)
    {
        display.fillRect(x - w, y - (h / 2), w, 3, _BLACK);
    }
    display.print(buf);
    return w;
}

void drawInvertedString(Inkplate &display, const GFXfont &font, const char *buf, int x, int y, int width)
{
    display.setFont(&font);
    int16_t x1, y1;
    uint16_t w, h;
    const int margin = 5;
    display.getTextBounds(buf, x, y, &x1, &y1, &w, &h);
    display.fillRoundRect(x, y - h - margin, width, h + 2 * margin, margin, _BLACK);
    display.setTextColor(_WHITE, _BLACK);
    display.setCursor(x + (width / 2) - (w / 2) - 2, y);
    display.print(buf);
    display.setCursor(x + width, y);
    display.setTextColor(_BLACK, _WHITE);
}

int drawWrappedText(Inkplate &display, const GFXfont &font, const String &text, int16_t x, int16_t y, int16_t maxWidth)
{
    display.setFont(&font);
    display.setTextWrap(false);
    int linesDrawn = 0;
    String line = "";
    int16_t cursorY = y;
    int i = 0;
    while (i < text.length())
    {
        // Handle whitespace and explicit newlines
        while (i < text.length() && (text[i] == ' ' || text[i] == '\t' || text[i] == '\r' || text[i] == '\n'))
        {
            if (text[i] == '\n' && line.length() > 0)
            {
                display.setCursor(x, cursorY);
                display.print(line);
                cursorY += font.yAdvance;
                line = "";
                linesDrawn++;
            }
            i++;
        }
        if (i >= text.length()) break;

        // Find end of current word
        int nextSpace = i;
        while (nextSpace < text.length() && text[nextSpace] != ' ' && text[nextSpace] != '\t' && text[nextSpace] != '\r' && text[nextSpace] != '\n')
        {
            nextSpace++;
        }
        String word = text.substring(i, nextSpace);
        String testLine = line + (line.length() > 0 ? " " : "") + word;
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(testLine.c_str(), 0, 0, &x1, &y1, &w, &h);
        if (w > maxWidth && line.length() > 0)
        {
            display.setCursor(x, cursorY);
            display.print(line);
            cursorY += font.yAdvance;
            line = word;
            linesDrawn++;
        }
        else
        {
            line = testLine;
        }
        i = nextSpace;
    }
    if (line.length() > 0)
    {
        display.setCursor(x, cursorY);
        display.print(line);
        linesDrawn++;
    }
    return linesDrawn;
}

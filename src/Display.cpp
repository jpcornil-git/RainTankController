/**
 * @file Display.cpp
 * @brief Implementation of the LCD display and UI driver.
 */

#include <ArduinoLog.h>
#include "charset.cpp"

#include "Display.h"
#include "Menu.h"
#include "../include/config.h"

/**
 * @brief Constructs a new Display instance and clears the text buffer.
 * 
 * @param backlightPin Pin controlling the LCD backlight brightness.
 * @param refreshRate Minimum milliseconds between display redraws.
 */
Display::Display(int backlightPin, unsigned long refreshRate)
    : _backlightPin(backlightPin), _refreshRate(refreshRate), _lastRefresh(0), _isBlink(false) {
    // Initialize the screen & blinking glyph buffers
    clear();
    memset(_glyphBlink, 0, sizeof(_glyphBlink));
}

/**
 * @brief Allocates the low-level UC1701 driver, turns on backlight, and clears LCD.
 * 
 * @param now Pointer to current timestamp in milliseconds.
 * @return ErrorCode SUCCESS if display initialized, ERROR if memory allocation failed.
 */
ErrorCode Display::begin(unsigned long *now) {
    _lcd = new UC1701();
    if (!_lcd) {
        Log.error("Error: Failed to allocate memory for UC1701 display.");
        return ERROR;
    }
    _menuItem = nullptr;
    // Turn on the backlight
    digitalWrite(_backlightPin, HIGH);
    pinMode(_backlightPin, OUTPUT);
    wake(*now, true);
    _lcd->begin();
    _lcd->clear();
    return SUCCESS;
}

/**
 * @brief Clears the internal software text buffers by filling with spaces.
 */
void Display::clear() {
    for (int r = 0; r < LCD_ROWS; r++) {
        for (int c = 0; c < LCD_COLS; c++) {
            _screenBuffer[r][c] = ' ';
            _oldScreenBuffer[r][c] = 'x'; // Ensure initial mismatch to force first redraw
        }
        _screenBuffer[r][LCD_COLS] = '\0';
        _oldScreenBuffer[r][LCD_COLS - 1] = '\0';
    }
}

/**
 * @brief Adjusts the LCD backlight brightness using analog PWM.
 * 
 * @param value Desired brightness from 0 to 100%.
 * @return ErrorCode SUCCESS if in bounds, ERROR otherwise.
 */
ErrorCode Display::setBacklight(int value) {
    if (value < 0 || value > 100) {
        Log.error("Error: Backlight value must be between 0 and 100.");
        return ERROR;
    }
    analogWrite(_backlightPin, map(value, 0, 100, 0, 255));
    return SUCCESS;
}

/**
 * @brief Configures a custom character slot from the font table for blink effects.
 * 
 * Extracts the 5x7 font bitmap corresponding to ASCII character `c`,
 * stores it in the specified slot (0 to 2), and sends it to the LCD as custom char (0x1F - id).
 * 
 * @param c Character to clone from the font set.
 * @param id Custom glyph slot (0, 1, or 2).
 */
void Display::setBlinkingChar(char c, unsigned id) {
    if (id >= 3) {
        Log.error("Error: Blinking character id must be between 0 and 2.\n");
        return;
    }
    memcpy(_glyphBlink[id], &charset[c - ' '], sizeof(_glyphBlink[id]));
    if (_isBlink) toggleGlyph(id);
    _lcd->createChar((unsigned char)(0x1f - id), _glyphBlink[id]);
}

/**
 * @brief Inverts the bitmap pixels of a stored blinking character.
 * 
 * @param id Custom character slot (0, 1, or 2).
 */
void Display::toggleGlyph(unsigned id) {
    for (unsigned char i = 0; i < sizeof(_glyphBlink[id]); ++i) {
        _glyphBlink[id][i] = ~_glyphBlink[id][i];
    }
}

/**
 * @brief Writes formatted text into a specific screen row.
 * 
 * Left-justifies text and pads with spaces up to LCD_COLS (20 characters plus null).
 * 
 * @param row Row index (0 to 7).
 * @param text Content string to display.
 * @return int Number of characters written.
 */
int Display::setLine(int row, const char* text) {
    if (row < 0 || row >= LCD_ROWS) return 0;
    return snprintf(_screenBuffer[row], LCD_COLS, "%-20s", text);
}

/**
 * @brief Renders a graphical progress bar gauge with percentage text.
 * 
 * Constructs a bitmap in RAM containing:
 * - A rectangular bordered box.
 * - Centered text showing percentage (e.g. " 75 %").
 * - Inverted pixel fill representing the level bar.
 * Flushes the resulting bitmap to the UC1701 controller via drawBitmap().
 * 
 * @param column Starting horizontal column pixel coordinate.
 * @param line Starting vertical page line (0 to 7).
 * @param width Width in pixels.
 * @param height Height in pages (1 page = 8 pixels).
 * @param levelProcent Level percentage (0.0 to 100.0).
 */
void Display::drawGauge(unsigned char column, unsigned char line, unsigned char width, unsigned char height, float levelProcent) {
    uint8_t gaugeBitmap[height * width];

    _lcd->setCursor(column, line);

    // Reset bitmap buffer
    memset(gaugeBitmap, 0, sizeof(gaugeBitmap));

    // Draw the gauge outline
    // Top and bottom horizontal lines
    for (int c = 0; c < width; c++) {
        gaugeBitmap[0 * width + c] |= 0x80; 
        gaugeBitmap[4 * width + c] |= 0x01; 
    }
    // Left and right vertical border lines
    for (int r = 1; r < height - 1; r++) {
        gaugeBitmap[r * width + 0] |= 0xFF;
        gaugeBitmap[r * width + width - 1] |= 0xFF;
    }

    // Embed percentage text centered within the gauge
    char lineBuffer[LCD_COLS + 1];
    unsigned textLenght = snprintf(lineBuffer, sizeof(lineBuffer), "%3d %%", (int)(levelProcent + 0.5));
    unsigned cursorX = (width - textLenght * 6) / 2;
    for (int i = 0; i < textLenght; i++, cursorX += 6) {
        char ch = lineBuffer[i];
        int fontIdx = ch - ' ';
        memcpy(&gaugeBitmap[2 * width + cursorX], charset[fontIdx], 5);
    }

    // Fill gauge bar proportionally to levelProcent
    unsigned lastColumn = (int)((levelProcent * (width - 2)) / 100.0 + 0.5);
    for (unsigned col = 1; col <= lastColumn; col++) {
        gaugeBitmap[1 * width + col] ^= 0xFF;
        gaugeBitmap[2 * width + col] ^= 0xFF;
        gaugeBitmap[3 * width + col] ^= 0xFF;
    }

    // Transfer the generated bitmap to the LCD controller
    _lcd->drawBitmap(gaugeBitmap, width, height);
}

/**
 * @brief Switches the active menu view.
 * 
 * Invokes leave() on the current item, clears the screen buffer, and invokes enter()
 * on the newly selected menu item.
 * 
 * @param menuItem Pointer to new MenuItem.
 */
void Display::setRenderer(MenuItem* menuItem) {
    if (_menuItem != nullptr) _menuItem->leave();
    _menuItem = menuItem;
    clear();
    if (_menuItem != nullptr) _menuItem->enter(this);
}

/**
 * @brief Manages backlight brightness timeouts based on user interaction.
 * 
 * - Activity (isWakeDisplay = true): Restores FULL brightness (100%).
 * - LCD_BACKLIGHT_MEDIUM_DELAY expiration: Dims to MEDIUM brightness (50%).
 * - LCD_BACKLIGHT_OFF_DELAY expiration: Turns backlight completely OFF.
 * 
 * @param now Current timestamp in milliseconds.
 * @param isWakeDisplay True if a button press or user event occurred.
 */
void Display::wake(unsigned long now, bool isWakeDisplay) {
    if (isWakeDisplay) {
        _wakeTimer = now + LCD_BACKLIGHT_MEDIUM_DELAY;
        _wakeState = FULL;
        setBacklight(100);
    } else if ((_wakeState != OFF) && (now > _wakeTimer)) {
        if (_wakeState == FULL) {
            _wakeTimer = now + (LCD_BACKLIGHT_OFF_DELAY - LCD_BACKLIGHT_MEDIUM_DELAY);
            _wakeState = MEDIUM;
            setBacklight(50);
        } else {
            _wakeState = OFF;
            setBacklight(0);
        }
    }
}

/**
 * @brief Updates display contents, toggles blinking elements, and flushes modified rows to LCD.
 * 
 * Performance optimizations:
 * - Rate limits updates to _refreshRate (unless forceRefresh is true).
 * - Computes a 1 Hz blink cycle and updates blinking custom characters when the cycle toggles.
 * - Compares each row of _screenBuffer against _oldScreenBuffer, only transmitting
 *   modified rows via SPI to minimize flicker and bus overhead.
 * 
 * @param now Current timestamp in milliseconds.
 * @param config Optional pointer to system configuration.
 * @param state Optional pointer to system state.
 * @param forceRefresh True to force immediate redraw.
 */
void Display::update(unsigned long now, Configuration *config, MachineState *state, bool forceRefresh) {
    // Refresh rate limiting: update the display at most every _refreshRate ms (e.g. 500 ms)
    if (((now - _lastRefresh) < _refreshRate) && !forceRefresh) return;
    _lastRefresh = now;

    // 1 Hz Blink effect calculation
    bool isBlink = ((now / 1000) % 2) == 0;
    bool isBlinkChanged = (isBlink != _isBlink);
    if (isBlinkChanged) {
        _isBlink = isBlink;
        for (unsigned char i = 0; i < 3; ++i) {
            toggleGlyph(i);
            _lcd->createChar((unsigned char)(0x1f - i), _glyphBlink[i]);
        }
    }

    // Render active menu item into the text buffer
    if (_menuItem != nullptr) _menuItem->render(this, config, state);

    // Flush dirty rows to the physical LCD
    for (int r = 0; r < LCD_ROWS; r++) {
        // Heartbeat indication (blinking '*' at top-right corner)
        if (r == 0) _screenBuffer[0][LCD_COLS - 1] = isBlink ? '*' : ' ';

        // Check if this row contains any blinking custom characters (< 0x20)
        bool hasBlinkingChar = false;
        for (int c = 0; _screenBuffer[r][c] != '\0'; ++c) {
            if (_screenBuffer[r][c] < ' ') {
                hasBlinkingChar = true;
                break;
            }
        }

        // Transmit row only if text changed or if row contains an inverted blinking glyph
        if ((strcmp(_screenBuffer[r], _oldScreenBuffer[r]) != 0) ||
            (isBlinkChanged && hasBlinkingChar)) {
            _lcd->setCursor(0, r);
            _lcd->print(_screenBuffer[r]);
            // Cache current row in old buffer
            strcpy(_oldScreenBuffer[r], _screenBuffer[r]);
        }
    }
}

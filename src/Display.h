#ifndef DISPLAY_H
#define DISPLAY_H

/**
 * @file Display.h
 * @brief LCD display driver and UI manager for Industruino's UC1701 graphic display.
 * 
 * Provides an 8-row by 21-column software text buffer with dirty-line caching,
 * automated 3-level backlight power management, blinking cursor/glyph generation,
 * and custom bitmap progress bar (gauge) rendering.
 */

#include "UC1701.h"
#include "../include/config.h"

class MenuItem;

// Software Frame Buffer dimensions for the 128x64 UC1701 LCD in text mode
#define LCD_ROWS 8   ///< Number of vertical text rows.
#define LCD_COLS 21  ///< Number of horizontal characters per row.

class Display {
public:
    /**
     * @brief Backlight power saving states.
     */
    enum WakeState {
        FULL,   ///< Backlight 100% brightness (active user interaction).
        MEDIUM, ///< Backlight 50% dimmed brightness (idle warning before shutoff).
        OFF     ///< Backlight turned completely off (power-saving sleep).
    };

    /**
     * @brief Construct a new Display controller.
     * 
     * @param backlightPin Microcontroller PWM pin controlling LCD backlight brightness.
     * @param refreshRate Minimum interval in milliseconds between display redraws.
     */
    Display(int backlightPin, unsigned long refreshRate);

    /**
     * @brief Default destructor.
     */
    ~Display() = default;

    /**
     * @brief Initialize the hardware display and turn on backlight.
     * 
     * @param now Pointer to the current system millisecond timestamp.
     * @return ErrorCode SUCCESS if initialization succeeded, ERROR otherwise.
     */
    ErrorCode begin(unsigned long *now);

    /**
     * @brief Clear the internal software text buffers.
     */
    void clear();

    /**
     * @brief Draw a graphical water level gauge bar with centered percentage text.
     * 
     * @param column Starting horizontal column pixel coordinate.
     * @param line Starting vertical text line (page) index (0 to 7).
     * @param width Width of the gauge in pixels.
     * @param height Height of the gauge in pages (1 page = 8 pixels).
     * @param levelProcent Current fill percentage (0.0 to 100.0%).
     */
    void drawGauge(unsigned char column, unsigned char line, unsigned char width, unsigned char height, float levelProcent);

    /**
     * @brief Set LCD backlight brightness percentage.
     * 
     * @param value Brightness percentage (0 to 100%).
     * @return ErrorCode SUCCESS if valid, ERROR if out of range.
     */
    ErrorCode setBacklight(int value);

    /**
     * @brief Register a blinking custom character glyph for text cursors or inverted icons.
     * 
     * @param c ASCII character to copy bitmap from standard font.
     * @param id Custom character slot (0 to 2, mapped to ASCII 0x1F, 0x1E, 0x1D).
     */
    void setBlinkingChar(char c, unsigned id = 0);

    /**
     * @brief Write a string into a specific row of the screen text buffer.
     * 
     * Pads the row with trailing spaces up to LCD_COLS to clear leftover characters.
     * 
     * @param row Row index (0 to LCD_ROWS - 1).
     * @param text String to write.
     * @return int Number of characters written.
     */
    int setLine(int row, const char* text);

    /**
     * @brief Set the active MenuItem to be drawn on the display.
     * 
     * Calls leave() on the previous menu item, clears the screen buffer,
     * and calls enter() on the new menu item.
     * 
     * @param menuList Pointer to the MenuItem to activate.
     */
    void setRenderer(MenuItem* menuList);

    /**
     * @brief Handle user activity wake-up and timer-based backlight dimming.
     * 
     * @param now Current system timestamp in milliseconds.
     * @param isWakeDisplay True if a user event occurred (resets backlight to FULL).
     */
    void wake(unsigned long now, bool isWakeDisplay);

    /**
     * @brief Periodic refresh handler: renders active menu and flushes dirty lines to LCD.
     * 
     * @param now Current system timestamp in milliseconds.
     * @param config Optional pointer to system configuration.
     * @param state Optional pointer to system state.
     * @param forceRefresh Force an immediate redraw bypassing refresh rate limits.
     */
    void update(unsigned long now, Configuration *config = nullptr, MachineState *state = nullptr, bool forceRefresh = false);

private:
    /**
     * @brief Invert the bitmap data of a custom blinking character.
     * 
     * @param id Custom character slot index (0 to 2).
     */
    void toggleGlyph(unsigned id);

    UC1701* _lcd;                               ///< Pointer to low-level UC1701 LCD driver.
    int _backlightPin;                          ///< GPIO pin controlling backlight PWM.
    unsigned long _refreshRate;                 ///< Minimum period between display flushes (ms).
    unsigned long _lastRefresh;                 ///< Timestamp of the last display refresh (ms).
    char _screenBuffer[LCD_ROWS][LCD_COLS + 1];    ///< Current software text frame buffer.
    char _oldScreenBuffer[LCD_ROWS][LCD_COLS + 1]; ///< Previous frame buffer for dirty-row caching.
    MenuItem* _menuItem;                        ///< Pointer to currently active menu item renderer.
    unsigned long _wakeTimer;                   ///< Timestamp when the next backlight dim/off step occurs.
    WakeState _wakeState;                       ///< Current backlight power saving state.
    unsigned char _glyphBlink[3][5];            ///< Bitmaps for up to 3 custom blinking 5x7 glyphs.
    bool _isBlink;                              ///< Current blink phase (true = inverted, false = normal).
};

#endif // DISPLAY_H

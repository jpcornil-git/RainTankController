/**
 * @file Menu.cpp
 * @brief Implementation of interactive menu screens, parameter editors, and dashboards.
 */

#include <ArduinoLog.h>

#include "../include/git_version.h"
#include "../include/config.h"
#include "Menu.h"
#include "Display.h"

/**
 * @brief Renders a standard two-line title header with a separator rule.
 * 
 * @param display Pointer to Display instance.
 * @param title Header title text.
 */
void MenuItem::title(Display* display, const char *title) {
    char lineBuffer[LCD_COLS + 1];

    display->setLine(0, title);
    memset(lineBuffer, '=', LCD_COLS);
    lineBuffer[LCD_COLS] = '\0';
    display->setLine(1, lineBuffer);
}

/**
 * @brief Renders the splash screen with hardware name, git version, and GitHub link.
 */
void SplashScreen::render(Display* display, Configuration* config, MachineState* state) {
    char lineBuffer[LCD_COLS + 1];
    display->setLine(0, "Industruino");
    display->setLine(1, "Rain tank controller");
    snprintf(lineBuffer, sizeof(lineBuffer), "FW: %s", GIT_VERSION);
    display->setLine(2, lineBuffer);
    // Line 3 is left blank
    display->setLine(4, "GitHub: jpcornil-git/");
    display->setLine(5, "RainTankCtrl.git");
}

OverviewMenuItem::OverviewMenuItem(const char* name) : MenuItem(name) {}

/**
 * @brief Renders operational summary: probe current, water level (cm and Liters), valve/rinse status, and alarms.
 */
void OverviewMenuItem::render(Display* display, Configuration* config, MachineState* state) {
    char lineBuffer[LCD_COLS + 1];

    title(display, _name);
    // Display 4-20mA sensor current
    snprintf(lineBuffer, sizeof(lineBuffer), "probe: %2d.%02d mA",
        (int) state->waterLevel_mA,
        (int)(state->waterLevel_mA * 100) % 100
    );
    display->setLine(2, lineBuffer);

    // Display level in cm and estimated volume in Liters (volume = height * surface / 1000)
    snprintf(lineBuffer, sizeof(lineBuffer), "level: %3d cm (%4dL)",
        (unsigned) (state->waterLevel_cm + 0.5),
        (unsigned) (state->waterLevel_cm * config->tankSurface_cm2 / 1000.0)
    );
    display->setLine(3, lineBuffer);

    // Display valve, rinsing, and alarm statuses
    snprintf(lineBuffer, sizeof(lineBuffer), "%s: %s", state->valve->getName(), state->valve->isActive() ? "on" : "off");
    display->setLine(4, lineBuffer);
    snprintf(lineBuffer, sizeof(lineBuffer), "rinsing: %s", state->isRinsing ? "on" : "off");
    display->setLine(5, lineBuffer);
    snprintf(lineBuffer, sizeof(lineBuffer), "%s: %s (code: %d)", state->alarm->getName(), state->alarm->isActive() ? "on" : "off", state->alarmCode);
    display->setLine(6, lineBuffer);
    snprintf(lineBuffer, sizeof(lineBuffer), "probe errors: %d", (int) ((state->probeErrorCnt + ALARM_PROBE_COST / 2) / ALARM_PROBE_COST));
    display->setLine(7, lineBuffer);    
}

ConfirmMenuItem::ConfirmMenuItem(const char* name, bool *pConfirm) : MenuItem(name) {
    _pConfirm = pConfirm;
}

/**
 * @brief Renders confirmation prompt and instructions.
 */
void ConfirmMenuItem::render(Display* display, Configuration* config, MachineState* state) {
    char lineBuffer[LCD_COLS + 1];

    title(display, _name);
    snprintf(lineBuffer, sizeof(lineBuffer), "Confirm ?");
    display->setLine(2, lineBuffer);
    display->setLine(7, "[<](long):OK");
}

/**
 * @brief Listens for a long-press on the Enter button to confirm the action.
 * 
 * @return bool True if action was confirmed and consumed.
 */
bool ConfirmMenuItem::update(Button::Event enterEvent, Button::Event upEvent, Button::Event downEvent) {
    bool capture = false;
    if (enterEvent == Button::LongPressed) {
        *_pConfirm = true;
        capture = true;
    }
    return capture;
}

ParamBaseMenuItem::ParamBaseMenuItem(const char* paramName)
    : MenuItem("Settings"), _paramName(paramName), _isEdit(false) {}

void ParamBaseMenuItem::renderHeader(Display* display) {
    char lineBuffer[LCD_COLS + 1];
    snprintf(lineBuffer, sizeof(lineBuffer), "%s %s", _name, _isEdit ? "[EDIT]" : "");
    title(display, lineBuffer);
}

void ParamBaseMenuItem::renderParamName(Display* display) {
    char lineBuffer[LCD_COLS + 1];
    snprintf(lineBuffer, sizeof(lineBuffer), "%s", _paramName);
    display->setLine(2, lineBuffer);
}

void ParamBaseMenuItem::renderSavePrompt(Display* display) {
    display->setLine(7, "[<](long):Save");
}

void ParamBaseMenuItem::leave() {
    _isEdit = false;
}

ParamBoolMenuItem::ParamBoolMenuItem(const char* name, bool *pVariable)
    : ParamBaseMenuItem(name), _pVariable(pVariable), _editValue(false) {
}

/**
 * @brief Renders the boolean setting state (Enabled / Disabled) and key prompts.
 */
void ParamBoolMenuItem::render(Display* display, Configuration* config, MachineState* state) {
    char lineBuffer[LCD_COLS + 1];

    renderHeader(display);
    renderParamName(display);
    bool value = _isEdit ? _editValue : *_pVariable;
    snprintf(lineBuffer, sizeof(lineBuffer), "%s", value ? "Enabled" : "Disabled");
    display->setLine(3, lineBuffer);
    display->setLine(6, "[^]|[v]:toggle");
    renderSavePrompt(display);
}

/**
 * @brief Handles toggling and saving boolean parameters.
 * 
 * - Long enter: enter edit mode / save value and exit edit mode.
 * - Up / Down short press: toggle true/false while in edit mode.
 */
bool ParamBoolMenuItem::update(Button::Event enterEvent, Button::Event upEvent, Button::Event downEvent) {
    bool capture = true;

    if (_isEdit) {
        if (enterEvent == Button::LongPressed) {
            _isEdit = false;
            *_pVariable = _editValue; // Save value
        } else {
            if ((upEvent == Button::ShortPressed) || (downEvent == Button::ShortPressed)) {
                _editValue = !_editValue; // Toggle value
            } else {
                capture = false;
            }
        }
    } else if (enterEvent == Button::LongPressed) {
        _isEdit = true;
        _editValue = *_pVariable; // Enter edit mode with current value
    } else {
        capture = false;
    }

    return capture;
}

ParamIntMenuItem::ParamIntMenuItem(const char* name, unsigned *pVariable, int nDigits, unsigned minValue, unsigned maxValue, const char* unit)
    : ParamBaseMenuItem(name),
      _pVariable(pVariable),
      _nDigits(nDigits),
      _min(minValue),
      _max(maxValue),
      _unit(unit),
      _currentDigitIdx(0),
      _editValue(0),
      _isDigitChanged(false) {}

/**
 * @brief Increments or decrements the currently selected digit with decade rollover.
 * 
 * @param isIncrement True to increment digit, false to decrement.
 */
void ParamIntMenuItem::_intIncDecVar(bool isIncrement) { 
    bool isNegative = (_editValue < 0) ? true : false;
    unsigned absValue = abs(_editValue); 

    int increment = (isIncrement) ? 1 : -1;
    unsigned currentDigit = absValue;
    for (unsigned i = 0; i < _currentDigitIdx; i++) {
        increment *= 10;
        currentDigit /= 10;
    }

    if ((currentDigit % 10) == ((isIncrement) ? 9 : 0)) {
        _editValue = absValue - 9 * increment;
    } else {
        _editValue = absValue + increment;
    }
    if (isNegative) _editValue = -_editValue;
}

/**
 * @brief Renders the integer parameter, highlighting the active digit with a blinking glyph in edit mode.
 */
void ParamIntMenuItem::render(Display* display, Configuration* config, MachineState* state) {
    char lineBuffer[LCD_COLS + 1];

    renderHeader(display);
    renderParamName(display);
    if (_isEdit) {
        snprintf(lineBuffer, sizeof(lineBuffer), "%0*d %s", _nDigits, _editValue, _unit ? _unit : "");
        if (_isDigitChanged) {
            _isDigitChanged = false;
            display->setBlinkingChar(lineBuffer[_nDigits - _currentDigitIdx - 1]);
        }
        lineBuffer[_nDigits - _currentDigitIdx - 1] = '\x1f'; // Inverted/blinking character slot
    } else {
        snprintf(lineBuffer, sizeof(lineBuffer), "%0*d %s", _nDigits, *_pVariable, _unit ? _unit : "");
    }

    display->setLine(3, lineBuffer);
    display->setLine(6, "[^]:- [v]:+ [<]:next");
    renderSavePrompt(display);
}

/**
 * @brief Handles navigation between digits, digit modification, and saving integer parameters.
 * 
 * - Short Enter: Advance cursor to next digit position.
 * - Long Enter: Enter edit mode / Validate bounds [_min, _max] and commit changes.
 * - Up / Down (short or repeat): Increment or decrement the active digit.
 */
bool ParamIntMenuItem::update(Button::Event enterEvent, Button::Event upEvent, Button::Event downEvent) {
    bool capture = true;
    
    if (_isEdit) {
        switch (enterEvent) {
            case Button::LongPressed:
                _isEdit = false;
                if ((_editValue >= _min) && (_editValue <= _max)) {
                    *_pVariable = _editValue; // Commit validated value
                }
                break;
            case Button::ShortPressed:
                // Move cursor to next digit
                _currentDigitIdx = (_currentDigitIdx + 1) % _nDigits;
                _isDigitChanged = true;
                break;
            default:
                if (upEvent != Button::None) {
                    _intIncDecVar(true);
                    _isDigitChanged = true;
                } else if (downEvent != Button::None) {
                    _intIncDecVar(false);
                    _isDigitChanged = true;
                } else {
                    capture = false;
                }
                break;
        }
    } else if (enterEvent == Button::LongPressed) {
        // Enter edit mode
        _isEdit = true;
        _editValue = *_pVariable;
        _currentDigitIdx = 0;
        _isDigitChanged = true;
    } else {
        capture = false;
    }

    return capture;
}

ParamIPMenuItem::ParamIPMenuItem(const char* name, IPAddress *pVariable, bool *isDhcp)
    : ParamBaseMenuItem(name),
      _pVariable(pVariable),
      _isDhcp(isDhcp),
      _editValue(),
      _currentOctet(0),
      _isOctetChanged(false) {}

/**
 * @brief Hides static IP settings when DHCP is enabled.
 * 
 * @return bool True if menu item is visible.
 */
bool ParamIPMenuItem::isVisible() {
    if ((_isDhcp != nullptr) && (*_isDhcp)) return false;
    return true;
}

/**
 * @brief Increments or decrements the selected IP octet with 0-255 wrapping.
 * 
 * @param isIncrement True to increment, false to decrement.
 */
void ParamIPMenuItem::_octetIncDec(bool isIncrement) {
    byte& octet = _editValue[_currentOctet];
    if (isIncrement) {
        octet = (octet == 255) ? 0 : octet + 1;
    } else {
        octet = (octet == 0) ? 255 : octet - 1;
    }
}

/**
 * @brief Renders the dotted-decimal IP address with blinking custom glyphs on the selected octet.
 */
void ParamIPMenuItem::render(Display* display, Configuration* config, MachineState* state) {
    char lineBuffer[LCD_COLS + 1];

    renderHeader(display);
    renderParamName(display);
    IPAddress value = _isEdit ? _editValue : *_pVariable;
    snprintf(lineBuffer, sizeof(lineBuffer), "%03u.%03u.%03u.%03u",
        (unsigned)value[0], (unsigned)value[1], (unsigned)value[2], (unsigned)value[3]);
    if (_isEdit) {
        if (_isOctetChanged) {
            unsigned selectedOctetStart = _currentOctet * 4;
            for (unsigned i = 0; i < 3; ++i) {
                display->setBlinkingChar(lineBuffer[selectedOctetStart + i], i);
                lineBuffer[selectedOctetStart + i] = (char)(0x1f - i);
            }
            _isOctetChanged = false;
        }
        unsigned selectedOctetStart = _currentOctet * 4;
        for (unsigned i = 0; i < 3; ++i) {
            lineBuffer[selectedOctetStart + i] = (char)(0x1f - i);
        }
    }

    display->setLine(3, lineBuffer);
    display->setLine(6, "[^]:- [v]:+ [<]:next");
    renderSavePrompt(display);
}

/**
 * @brief Handles octet navigation, octet adjustments, and saving IP addresses.
 * 
 * - Short Enter: Advance cursor to the next octet (0 to 3).
 * - Long Enter: Save IP address and exit edit mode.
 * - Up / Down: Increment or decrement the selected octet.
 */
bool ParamIPMenuItem::update(Button::Event enterEvent, Button::Event upEvent, Button::Event downEvent) {
    if (_isEdit) {
        if (enterEvent == Button::LongPressed) {
            _isEdit = false;
            *_pVariable = _editValue; // Commit edited IP address
        } else if (enterEvent == Button::ShortPressed) {
            _currentOctet = (_currentOctet + 1) % 4; // Advance to next octet
            _isOctetChanged = true;
        } else if (upEvent != Button::None) {
            _octetIncDec(true);
            _isOctetChanged = true;
        } else if (downEvent != Button::None) {
            _octetIncDec(false);
            _isOctetChanged = true;
        } else {
            return false;
        }
        return true;
    }

    if (enterEvent != Button::LongPressed) {
        return false;
    }

    _isEdit = true;
    _editValue = *_pVariable;
    _currentOctet = 0;
    _isOctetChanged = true;
    return true;
}

NetworkInfo::NetworkInfo(const char* name, Network **network) : MenuItem(name) {
    _network = network;
}

/**
 * @brief Renders active IP address, Gateway, Subnet mask, and DNS server.
 */
void NetworkInfo::render(Display* display, Configuration* config, MachineState* state) {
    char lineBuffer[LCD_COLS + 1];

    title(display, _name);
    Network::DhcpMode dhcpMode = (*_network)->getDhcpMode();
    snprintf(lineBuffer, sizeof(lineBuffer), "DHCP: %s",
        dhcpMode == Network::DhcpMode::DHCP ? "Success" :
        dhcpMode == Network::DhcpMode::STATIC ? "None (static)" :
        "Failed (link local)"
    );
    display->setLine(2, lineBuffer);    
    IPAddress ip = (*_network)->getIP();
    snprintf(lineBuffer, sizeof(lineBuffer), "IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    display->setLine(3, lineBuffer);
    IPAddress gateway = (*_network)->getGateway();
    snprintf(lineBuffer, sizeof(lineBuffer), "GW: %d.%d.%d.%d", gateway[0], gateway[1], gateway[2], gateway[3]);
    display->setLine(4, lineBuffer);
    IPAddress subnet = (*_network)->getSubnet();
    snprintf(lineBuffer, sizeof(lineBuffer), "SN: %d.%d.%d.%d", subnet[0], subnet[1], subnet[2], subnet[3]);
    display->setLine(5, lineBuffer);
    IPAddress dns = (*_network)->getDNS();
    snprintf(lineBuffer, sizeof(lineBuffer), "DNS: %d.%d.%d.%d", dns[0], dns[1], dns[2], dns[3]);
    display->setLine(6, lineBuffer);
}

/**
 * @brief Preloads custom blinking glyphs 'o' and 'n' for the relay state animation on the gauge screen.
 */
void TankLevelInfo::enter(Display* display) {
    display->setBlinkingChar('o', 0);
    display->setBlinkingChar('n', 1);
}

/**
 * @brief Renders the visual water level gauge bar and output relay statuses.
 */
void TankLevelInfo::render(Display* display, Configuration* config, MachineState* state) {
    char lineBuffer[LCD_COLS + 1];

    title(display, _name);
    // Calculate water level percentage clamped between 0 and 100%
    float level = max(0.0f, min(100.0f, 100.0f * state->waterLevel_cm / config->tankMaxHeight_cm));
    display->drawGauge(3, 2, 102, 5, level);

    // Display output states at bottom row
    snprintf(lineBuffer, sizeof(lineBuffer), "%s:%s, %s:%s",
        state->valve->getName(), state->valve->isActive() ? "\x1f\x1e " : "off",
        state->alarm->getName(), state->alarm->isActive() ? "\x1f\x1e " : "off");
    display->setLine(7, lineBuffer);
}

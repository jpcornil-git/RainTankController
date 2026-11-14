#ifndef MENU_H
#define MENU_H

/**
 * @file Menu.h
 * @brief On-screen interactive menu framework for LCD display and keypad navigation.
 * 
 * Provides an extensible MenuItem hierarchy supporting status dashboards, graphical gauges,
 * confirmation dialogs, and interactive editors for integers, booleans, and IP addresses.
 */

#include "Button.h"
#include "Network.h"

class Display;
struct MachineState;
struct Configuration;

/**
 * @brief Abstract base class representing a single view or screen in the menu hierarchy.
 */
class MenuItem {
public:
    /**
     * @brief Construct a new MenuItem.
     * 
     * @param name Descriptive title displayed on the header line.
     */
    MenuItem(const char* name = nullptr) : _name(name) {}

    /**
     * @brief Default virtual destructor.
     */
    virtual ~MenuItem() = default;

    /**
     * @brief Called when this menu item becomes active on the display.
     * 
     * @param display Pointer to the active Display driver.
     */
    virtual void enter(Display* display) {}

    /**
     * @brief Draws the menu contents into the display text buffer.
     * 
     * @param display Pointer to the Display driver.
     * @param config Pointer to current system configuration.
     * @param state Pointer to current machine operational state.
     */
    virtual void render(Display* display, Configuration* config = nullptr, MachineState* state = nullptr) {}

    /**
     * @brief Handles user input button events when this item is active.
     * 
     * @param enter Enter button event.
     * @param up Up button event.
     * @param down Down button event.
     * @return bool True if the event was consumed/captured by this item, false to allow global navigation.
     */
    virtual bool update(Button::Event enter, Button::Event up, Button::Event down) { return false; }

    /**
     * @brief Called when transitioning away from this menu item.
     */
    virtual void leave() {}

    /**
     * @brief Determines whether this menu item should be shown during carousel navigation.
     * 
     * @return bool True if visible, false to skip (e.g. static IP items when DHCP is active).
     */
    virtual bool isVisible() { return true; }

    /**
     * @brief Utility method to render a standardized header title and separator bar on lines 0 and 1.
     * 
     * @param display Pointer to Display driver.
     * @param title Title string to display.
     */
    void title(Display* display, const char *title);

protected:
    const char* _name; ///< Title of the menu item.
};

/**
 * @brief Splash screen displaying controller branding, git version, and repository URL.
 */
class SplashScreen : public MenuItem {
public:
    void render(Display* display, Configuration* config, MachineState* state) override;
};

/**
 * @brief Primary dashboard displaying a graphical tank level gauge and output relay statuses.
 */
class TankLevelInfo : public MenuItem {
public:
    TankLevelInfo(const char* name) : MenuItem(name), _lastLevelProcent(0) {}
    ~TankLevelInfo() = default;
    void enter(Display* display) override;
    void render(Display* display, Configuration* config, MachineState* state) override;

private:
    unsigned _lastLevelProcent;
};

/**
 * @brief Textual operational overview showing sensor currents, water volume in Liters, and valve/alarm states.
 */
class OverviewMenuItem : public MenuItem {
public:
    OverviewMenuItem(const char* name);
    ~OverviewMenuItem() = default;
    void render(Display* display, Configuration* config, MachineState* state) override;
};

/**
 * @brief Confirmation prompt for critical actions (e.g. system reboot, saving flash, clearing alarms).
 */
class ConfirmMenuItem : public MenuItem {
public:
    /**
     * @brief Construct a new ConfirmMenuItem.
     * 
     * @param name Action description shown on screen.
     * @param pConfirm Pointer to boolean flag set to true when confirmed via long press.
     */
    ConfirmMenuItem(const char* name, bool *pConfirm);
    ~ConfirmMenuItem() = default;
    void render(Display* display, Configuration* config, MachineState* state) override;
    bool update(Button::Event enterEvent, Button::Event upEvent, Button::Event downEvent) override;

private:
    bool *_pConfirm; ///< Pointer to target boolean flag.
};

/**
 * @brief Abstract base class for interactive configuration parameter editors.
 */
class ParamBaseMenuItem : public MenuItem {
public:
    void leave() override;

protected:
    explicit ParamBaseMenuItem(const char* paramName);
    void renderHeader(Display* display);
    void renderParamName(Display* display);
    void renderSavePrompt(Display* display);

    const char* _paramName; ///< Descriptive name of the parameter.
    bool _isEdit;           ///< True when actively editing the parameter.
};

/**
 * @brief Multi-digit integer parameter editor with per-digit editing and range clamping.
 */
class ParamIntMenuItem : public ParamBaseMenuItem {
public:
    /**
     * @brief Construct a new ParamIntMenuItem.
     * 
     * @param name Menu title.
     * @param pVariable Pointer to the target unsigned integer variable.
     * @param nDigits Total number of digits to display and edit.
     * @param minValue Minimum allowable value.
     * @param maxValue Maximum allowable value.
     * @param unit Unit suffix string (e.g. "cm", "cm2", "Min.").
     */
    ParamIntMenuItem(const char* name, unsigned *pVariable, int nDigits, unsigned minValue, unsigned maxValue, const char* unit = nullptr);
    ~ParamIntMenuItem() = default;
    void render(Display* display, Configuration* config, MachineState* state) override;
    bool update(Button::Event enterEvent, Button::Event upEvent, Button::Event downEvent) override;

private:
    void _intIncDecVar(bool isIncrement);

    unsigned *_pVariable;
    unsigned _nDigits;
    unsigned _min, _max;
    const char* _unit;
    unsigned _currentDigitIdx;
    unsigned _editValue;
    bool _isDigitChanged;
};

/**
 * @brief Boolean setting editor (e.g. Enabled / Disabled toggle).
 */
class ParamBoolMenuItem : public ParamBaseMenuItem {
public:
    ParamBoolMenuItem(const char* name, bool *pVariable);
    ~ParamBoolMenuItem() = default;
    void render(Display* display, Configuration* config, MachineState* state) override;
    bool update(Button::Event enterEvent, Button::Event upEvent, Button::Event downEvent) override;

private:
    bool *_pVariable;
    bool _editValue;
};

/**
 * @brief IPv4 address parameter editor with octet-by-octet modification.
 */
class ParamIPMenuItem : public ParamBaseMenuItem {
public:
    /**
     * @brief Construct a new ParamIPMenuItem.
     * 
     * @param name Parameter label.
     * @param pVariable Pointer to target IPAddress object.
     * @param isDhcp Optional pointer to DHCP enabled flag; hides this item if DHCP is active.
     */
    ParamIPMenuItem(const char* name, IPAddress *pVariable, bool *isDhcp = nullptr);
    ~ParamIPMenuItem() = default;
    void render(Display* display, Configuration* config, MachineState* state) override;
    bool update(Button::Event enterEvent, Button::Event upEvent, Button::Event downEvent) override;
    bool isVisible() override;

private:
    void _octetIncDec(bool isIncrement);

    IPAddress *_pVariable;
    IPAddress _editValue;
    unsigned _currentOctet;
    bool _isOctetChanged;
    bool *_isDhcp;
};

/**
 * @brief Read-only network status screen displaying IP, Gateway, Subnet, and DNS.
 */
class NetworkInfo : public MenuItem {
public:
    NetworkInfo(const char* name, Network **network);
    ~NetworkInfo() = default;
    void render(Display* display, Configuration* config, MachineState* state) override;
    void leave() override {}

private:
    Network **_network;
};

#endif // MENU_H

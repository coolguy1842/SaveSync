#ifndef __KEYBOARD_HPP__
#define __KEYBOARD_HPP__

#include <3ds.h>
#include <functional>
#include <map>
#include <optional>
#include <string>

// filter out (disable) the use of the following
struct KeyboardValidationFilterOptions {
    std::optional<bool> digits;
    std::optional<bool> at;        /* @ */
    std::optional<bool> percent;   /* % */
    std::optional<bool> backslash; /* \ */
    std::optional<bool> profanity;

    std::optional<std::function<SwkbdCallbackResult(const char** errorMessage, const char* text, size_t size)>> callback;
};

struct KeyboardValidationOptions {
    std::optional<SwkbdValidInput> accepted;
    KeyboardValidationFilterOptions filter;
    std::optional<bool> maxDigits;
};

struct KeyboardFeatureOptions {
    std::optional<bool> parental; // parental pin mode
    std::optional<bool> darkenTopScreen;
    std::optional<bool> predictiveInput; // required for kanji on jpn consoles
    std::optional<bool> multiline;
    std::optional<bool> fixedWidth;
    std::optional<bool> allowHome;
    std::optional<bool> allowReset;
    std::optional<bool> allowPower;
    std::optional<bool> defaultQWERTY; // default to the qwerty page
};

struct KeyboardButton {
    const char* text;
    bool submit;
};

struct KeyboardButtonOptions {
    std::optional<KeyboardButton> left;
    std::optional<KeyboardButton> middle;
    std::optional<KeyboardButton> right;
};

struct KeyboardDictEntry {
    const char* input;
    const char* output;
};

struct KeyboardOptions {
    std::optional<SwkbdType> type;
    std::optional<int> numpadLeft;
    std::optional<int> numpadRight;

    std::optional<int> inputLen;
    std::optional<SwkbdPasswordMode> passwordMode;

    std::optional<std::vector<KeyboardDictEntry>> dictionary;

    KeyboardButtonOptions buttons;
    KeyboardFeatureOptions features;
    KeyboardValidationOptions validation;
};

class Keyboard {
public:
    Keyboard();
    Keyboard(const KeyboardOptions& options);
    ~Keyboard();

    void setOptions(const KeyboardOptions& options);
    // if maxInputLen hasnt been set yet error result returned
    Result show(std::string initialText = "");

    std::string output() const;
    // SWKBD_BUTTON_NONE returned if it hasnt been shown
    SwkbdButton button() const;

private:
    static SwkbdCallbackResult onFilterCallback(void* user, const char** err, const char* text, size_t size);

    KeyboardOptions m_options;

    std::string m_out;
    SwkbdButton m_pressedButton = SWKBD_BUTTON_NONE;
};

#endif
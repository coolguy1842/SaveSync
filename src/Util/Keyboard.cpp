#include <Util/Keyboard.hpp>
#include <cstring>
#include <stdio.h>
#include <string>

SwkbdCallbackResult Keyboard::onFilterCallback(void* user, const char** err, const char* text, size_t size) {
    Keyboard* keyboard = reinterpret_cast<Keyboard*>(user);

    if(keyboard->m_options.validation.filter.callback.value_or(nullptr) == nullptr) {
        return SWKBD_CALLBACK_OK;
    }

    return keyboard->m_options.validation.filter.callback.value()(err, text, size);
}

Keyboard::~Keyboard() {}
Keyboard::Keyboard() {}

Keyboard::Keyboard(const KeyboardOptions& options)
    : Keyboard() {
    setOptions(options);
}

void Keyboard::setOptions(const KeyboardOptions& options) {
#define CHECK_OPTION(name) \
    if(options.name.has_value()) m_options.name = options.name

    CHECK_OPTION(type);
    CHECK_OPTION(numpadLeft);
    CHECK_OPTION(numpadRight);

    CHECK_OPTION(inputLen);
    CHECK_OPTION(passwordMode);

    CHECK_OPTION(dictionary);

    {
        CHECK_OPTION(buttons.left);
        CHECK_OPTION(buttons.middle);
        CHECK_OPTION(buttons.right);
    }

    {
        CHECK_OPTION(validation.accepted);
        CHECK_OPTION(validation.maxDigits);
        {
            CHECK_OPTION(validation.filter.digits);
            CHECK_OPTION(validation.filter.at);
            CHECK_OPTION(validation.filter.percent);
            CHECK_OPTION(validation.filter.backslash);
            CHECK_OPTION(validation.filter.profanity);
            CHECK_OPTION(validation.filter.callback);
        }
    }

    {
        CHECK_OPTION(features.parental);
        CHECK_OPTION(features.darkenTopScreen);
        CHECK_OPTION(features.predictiveInput);
        CHECK_OPTION(features.multiline);
        CHECK_OPTION(features.fixedWidth);
        CHECK_OPTION(features.allowHome);
        CHECK_OPTION(features.allowReset);
        CHECK_OPTION(features.allowPower);
        CHECK_OPTION(features.defaultQWERTY);
    }
#undef CHECK_OPTION
}

u32 featureFlags(const KeyboardFeatureOptions& opts) {
    return (opts.parental.value_or(false) ? SWKBD_PARENTAL : 0) |
           (opts.darkenTopScreen.value_or(false) ? SWKBD_DARKEN_TOP_SCREEN : 0) |
           (opts.predictiveInput.value_or(false) ? SWKBD_PREDICTIVE_INPUT : 0) |
           (opts.multiline.value_or(false) ? SWKBD_MULTILINE : 0) |
           (opts.fixedWidth.value_or(false) ? SWKBD_FIXED_WIDTH : 0) |
           (opts.allowHome.value_or(false) ? SWKBD_ALLOW_HOME : 0) |
           (opts.allowReset.value_or(false) ? SWKBD_ALLOW_RESET : 0) |
           (opts.allowPower.value_or(false) ? SWKBD_ALLOW_POWER : 0) |
           (opts.defaultQWERTY.value_or(false) ? SWKBD_DEFAULT_QWERTY : 0);
}

u32 filterFlags(const KeyboardValidationFilterOptions& opts) {
    return (opts.digits.value_or(false) ? SWKBD_FILTER_DIGITS : 0) |
           (opts.at.value_or(false) ? SWKBD_FILTER_AT : 0) |
           (opts.percent.value_or(false) ? SWKBD_FILTER_PERCENT : 0) |
           (opts.backslash.value_or(false) ? SWKBD_FILTER_BACKSLASH : 0) |
           (opts.profanity.value_or(false) ? SWKBD_FILTER_PROFANITY : 0) |
           (opts.callback.has_value() && opts.callback.value() != nullptr ? SWKBD_FILTER_CALLBACK : 0);
}

std::string Keyboard::output() const { return m_out; }
SwkbdButton Keyboard::button() const { return m_pressedButton; }

Result Keyboard::show(std::string initialText) {
#define CHECK_BUTTON(name, id) \
    if(buttonOpts.name.has_value()) swkbdSetButton(&swkbd, id, buttonOpts.name->text, buttonOpts.name->submit)

    m_pressedButton = SWKBD_BUTTON_NONE;
    m_out.clear();

    const KeyboardButtonOptions& buttonOpts = m_options.buttons;
    int buttons                             = std::max(1, buttonOpts.left.has_value() + buttonOpts.middle.has_value() + buttonOpts.right.has_value());

    if(!m_options.inputLen.has_value() || m_options.inputLen.value() <= 0) {
        return MAKERESULT(RL_USAGE, RS_NOTFOUND, RM_APPLICATION, RD_NO_DATA);
    }

    SwkbdState swkbd;
    swkbdInit(&swkbd, m_options.type.value_or(SWKBD_TYPE_NORMAL), buttons, m_options.inputLen.value());
    swkbdSetPasswordMode(&swkbd, m_options.passwordMode.value_or(SWKBD_PASSWORD_NONE));
    swkbdSetFeatures(&swkbd, featureFlags(m_options.features));

    swkbdSetInitialText(&swkbd, initialText.c_str());

    std::vector<SwkbdDictWord> words;
    const std::vector<KeyboardDictEntry>& dictEntries = m_options.dictionary.value_or({});
    if(!dictEntries.empty()) {
        words.resize(dictEntries.size());
        for(size_t i = 0; i < dictEntries.size(); i++) {
            swkbdSetDictWord(&words[i], dictEntries[i].input, dictEntries[i].output);
        }

        swkbdSetDictionary(&swkbd, words.data(), static_cast<int>(words.size()));
    }

    CHECK_BUTTON(left, SWKBD_BUTTON_LEFT);
    CHECK_BUTTON(middle, SWKBD_BUTTON_MIDDLE);
    CHECK_BUTTON(right, SWKBD_BUTTON_RIGHT);

    if(swkbd.type == SWKBD_TYPE_NUMPAD) {
        swkbdSetNumpadKeys(&swkbd, m_options.numpadLeft.value_or(0), m_options.numpadRight.value_or(0));
    }

    swkbdSetValidation(&swkbd, m_options.validation.accepted.value_or(SWKBD_ANYTHING), filterFlags(m_options.validation.filter), m_options.validation.maxDigits.value_or(0));
    if(swkbd.filter_flags & SWKBD_FILTER_CALLBACK) {
        swkbdSetFilterCallback(&swkbd, &Keyboard::onFilterCallback, this);
    }

    m_out.resize(static_cast<size_t>(m_options.inputLen.value() + 1));
    m_pressedButton = swkbdInputText(&swkbd, m_out.data(), m_out.size());
    m_out.resize(strnlen(m_out.c_str(), m_out.size()));

    return RL_SUCCESS;
#undef CHECK_BUTTON
}
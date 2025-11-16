#ifndef __CONFIG_HPP__
#define __CONFIG_HPP__

#include <3ds.h>

#include <FS/Archive.hpp>
#include <FS/File.hpp>
#include <sigs.h>
#include <string>
#include <unordered_map>

enum Layout {
    GRID,
    LIST
};

template<typename T>
class Option {
public:
    Option(std::string key, T defaultValue)
        : m_key(key)
        , m_value(defaultValue) {}

    ~Option() {
        m_changedSignal.setBlocked(true);
        m_changedEmptySignal.setBlocked(true);
    }

    std::string key() const { return m_key; }
    const T& value() const { return m_value; }
    T value() { return m_value; }

    void setValue(T value) {
        if(value != m_value) {
            m_value = value;
            m_changedSignal(m_value);
            m_changedEmptySignal();
        }
    }

    [[nodiscard]] auto changedSignal() { return m_changedSignal.interface(); }
    [[nodiscard]] auto changedEmptySignal() { return m_changedEmptySignal.interface(); }

private:
    std::string m_key;
    T m_value;

    sigs::Signal<void(const T&)> m_changedSignal;
    sigs::Signal<void()> m_changedEmptySignal;
};

class Config {
public:
    Config();

    std::shared_ptr<Option<std::string>> serverURL();
    std::shared_ptr<Option<u16>> serverPort();
    std::shared_ptr<Option<Layout>> layout();

    void load();
    void save();

private:
    std::shared_ptr<File> openFile(u32 flags);

    std::shared_ptr<Option<std::string>> m_serverURL;
    std::shared_ptr<Option<u16>> m_serverPort;
    std::shared_ptr<Option<Layout>> m_layout;
};

#endif
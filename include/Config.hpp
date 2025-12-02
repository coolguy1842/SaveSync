#ifndef __CONFIG_HPP__
#define __CONFIG_HPP__

#include <3ds.h>

#include <FS/Archive.hpp>
#include <FS/File.hpp>
#include <rocket.hpp>
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
        changedSignal.clear();
        changedEmptySignal.clear();
    }

    std::string key() const { return m_key; }
    const T& value() const { return m_value; }
    T value() { return m_value; }

    void setValue(T value) {
        if(value != m_value) {
            m_value = value;
            changedSignal(m_value);
            changedEmptySignal();
        }
    }

    rocket::thread_safe_signal<void(const T&)> changedSignal;
    rocket::thread_safe_signal<void()> changedEmptySignal;

private:
    std::string m_key;
    T m_value;
};

class Config : rocket::trackable {
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
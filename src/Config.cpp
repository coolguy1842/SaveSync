#include <Config.hpp>
#include <Util/Logger.hpp>
#include <format>

const std::string defaultURL = "http://example.com";
const u16 defaultPort        = 8000;
const Layout defaultLayout   = GRID;

Config::Config()
    : m_serverURL(std::make_shared<Option<std::string>>("Server URL", defaultURL))
    , m_serverPort(std::make_shared<Option<u16>>("Server Port", defaultPort))
    , m_layout(std::make_shared<Option<Layout>>("Layout", defaultLayout)) {
    load();

    // m_serverURL->changedEmptySignal()->connect(this, &Config::save);
    // m_serverPort->changedEmptySignal()->connect(this, &Config::save);
    // m_layout->changedEmptySignal()->connect(this, &Config::save);
}

std::shared_ptr<Option<std::string>> Config::serverURL() { return m_serverURL; }
std::shared_ptr<Option<u16>> Config::serverPort() { return m_serverPort; }
std::shared_ptr<Option<Layout>> Config::layout() { return m_layout; }

void Config::load() {
    auto file = openFile(FS_OPEN_READ);
    if(file == nullptr || !file->valid()) {
        Logger::info("Config", "Config file not found");
        return;
    }

    std::string url       = file->readLine(0);
    std::string portStr   = file->readLine(url.size() + 1);
    std::string layoutStr = file->readLine(url.size() + portStr.size() + 2);

    if(url.empty() || portStr.empty() || layoutStr.empty()) {
        Logger::warn("Config", "Config file invalid entries");
        return;
    }

    m_serverURL->setValue(url);
    if(portStr.size() <= 4) {
        u16 port = 0;
        for(char& c : portStr) {
            if(!isdigit(c)) {
                goto skipPort;
            }

            port *= 10;
            port += c - '0';
        }

        m_serverPort->setValue(port);
    skipPort:
    }

    if(layoutStr.size() == 1 && isdigit(layoutStr[0])) {
        m_layout->setValue(static_cast<Layout>(layoutStr[0] - '0'));
    }
}

void Config::save() {
    auto file = openFile(FS_OPEN_WRITE | FS_OPEN_CREATE);
    if(file == nullptr || !file->valid()) {
        Logger::warn("Config", "Failed to create config file");
        return;
    }

    std::string writeBuf;

    writeBuf += m_serverURL->value() + "\n";
    writeBuf += std::format("{}", m_serverPort->value()) + "\n";
    writeBuf += std::format("{}", static_cast<int>(m_layout->value())) + "\n";
    if(writeBuf.empty()) {
        file->setSize(1);
        file->write({ '\n' }, 0, FS_WRITE_FLUSH);

        return;
    }

    if(!file->setSize(writeBuf.size())) {
        Logger::warn("Config", "Failed to set config file size");
        return;
    }

    u32 wrote = file->write(writeBuf.c_str(), writeBuf.size(), 0, FS_WRITE_FLUSH);
    if(wrote == 0 || wrote == UINT32_MAX) {
        Logger::warn("Config", "Failed to write config data");
    }
}

std::shared_ptr<File> Config::openFile(u32 flags) {
    std::shared_ptr<Archive> sdmc = Archive::open(ARCHIVE_SDMC, VarPath());
    if(sdmc == nullptr || !sdmc->valid() || !sdmc->mkdir(u"/3ds/SaveSync", 0, true)) {
        return nullptr;
    }

    return sdmc->openFile(u"/3ds/SaveSync/config", flags, 0);
}

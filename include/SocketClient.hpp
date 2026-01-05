#ifndef __SOCKET_CLIENT_HPP__
#define __SOCKET_CLIENT_HPP__

#include <3ds.h>
#include <string>

#define CLIENT_SERVER_VERSION 2

class SocketClient {
public:
    SocketClient();
    ~SocketClient();

    bool valid() const;
    Result lastResult() const;

    bool wifiEnabled();
    bool updateSocket();

private:
    bool m_valid;
    Result m_result;

    std::string m_url;

    // used to stop repeated debug logs
    std::string m_lastURL;
    int m_lastErrno;

    u16 m_port;
};

#endif
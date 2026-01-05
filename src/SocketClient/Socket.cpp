#include <Debug/Logger.hpp>
#include <SocketClient.hpp>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>

#ifdef __BIG_ENDIAN__
#define htonll(x) x
#define ntohll(x) x
#else
#define htonll(x) ((static_cast<uint64_t>(htonl(x & 0xFFFFFFFF)) << 32) | htonl(x >> 32))
#define ntohll(x) ((static_cast<uint64_t>(ntohl(x & 0xFFFFFFFF)) << 32) | ntohl(x >> 32))
#endif

bool SocketClient::updateSocket() {
    if(!m_valid || !wifiEnabled()) {
        return false;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) {
        Logger::warn("Client Update Socket", "Failed to create socket: {}", strerror(errno));
        return false;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(m_port);
    if(inet_pton(AF_INET, m_url.c_str(), &addr.sin_addr) <= 0) {
        if(m_url != m_lastURL) {
            Logger::warn("Client Update Socket", "Invalid URL");
            m_lastURL = m_url;
        }

        goto cleanup;
    }

    if(connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        if(errno != m_lastErrno) {
            Logger::warn("Client Update Socket", "Failed to connect: {}", strerror(errno));
            m_lastErrno = errno;
        }

    cleanup:
        close(sock);
        return false;
    }

    const u16 version          = htons(CLIENT_SERVER_VERSION);
    const std::vector<u64> ids = {
        0x00040000001B9000,
        0x0004000000053F00
    };

    printf("version write out: %d\n", write(sock, &version, sizeof(u16)));
    for(size_t i = 0; i < ids.size(); i++) {
        u64 id = ids[i];
        if(i < ids.size() - 1) {
            id |= static_cast<u64>(1) << 63;
        }

        id = htonll(id);
        write(sock, &id, sizeof(u64));
    }

    char buf[0x1000];
    _ssize_t numRead = read(sock, buf, sizeof(buf));
    printf("read %d bytes\n", numRead);

    if(numRead > 0) {
        buf[numRead - 1] = 0;
        printf("%s\n", buf);
    }

    Logger::info("Client Update Socket", "Created Socket");

    close(sock);
    return true;
}
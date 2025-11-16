#ifndef __CURL_EASY_HPP__
#define __CURL_EASY_HPP__

#include <curl/curl.h>

#include <functional>
#include <optional>
#include <string>

enum CURLEasyMethod {
    GET,
    HEAD,
    POST,
    PUT,
    DELETE,
    CONNECT,
    OPTIONS,
    TRACE,
    PATCH
};

struct ReadOptions {
    std::optional<long> bufferSize;
    long dataSize;

    // this function is called when it sends data to the server i.e uploading, params are: output buffer, max bytes to send (bufsize)
    std::function<size_t(char*, size_t)> callback;
};

struct WriteOptions {
    std::optional<long> bufferSize;

    // this function is called when it receives data from the server, params are: buf, bufsize (bytes)
    std::function<size_t(char*, size_t)> callback;
};

struct LowSpeedOptions {
    // limit 30, time 60 would be abort if lower than 30 bytes per second during 60 seconds
    long limit;
    long time;
};

struct CURLEasyOptions {
    // any set to nullopt will not change
    std::optional<std::string> url;
    std::optional<CURLEasyMethod> method;
    std::optional<std::string> contentType;

    std::optional<bool> followLocation   = true;
    std::optional<long> maximumRedirects = 5;

    std::optional<bool> noBody;
    std::optional<bool> trackProgress;
    // curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow
    std::optional<std::function<int(curl_off_t, curl_off_t, curl_off_t, curl_off_t)>> customProgressFunction;

    std::optional<long> timeout;
    std::optional<long> connectTimeout;

    std::optional<long> timeoutMS;
    std::optional<long> connectTimeoutMS;
    std::optional<long> acceptTimeoutMS;

    std::optional<LowSpeedOptions> lowSpeed;

    // set this if it should upload something
    std::optional<ReadOptions> read;

    // set this if it should download something
    std::optional<WriteOptions> write;
};

class CURLEasy {
public:
    CURLEasy();
    CURLEasy(const CURLEasyOptions& options);
    ~CURLEasy();

    void setHeader(std::string header, std::string value);
    void setOptions(const CURLEasyOptions& options);

    template<typename T>
    void getInfo(CURLINFO info, T value) { curl_easy_getinfo(m_curl, info, value); }

    CURLcode perform();
    long statusCode();

    CURL* getHandle();

    std::string escape(std::string str);

    // 0 to 100
    float getDownloadPercent() const;
    curl_off_t getDownloadCurrent() const;
    curl_off_t getDownloadMax() const;

    // 0 to 100
    float getUploadPercent() const;
    curl_off_t getUploadCurrent() const;
    curl_off_t getUploadMax() const;

private:
    int handleXFERInfo(curl_off_t downloadTotal, curl_off_t downloadNow, curl_off_t uploadTotal, curl_off_t uploadNow);

    static size_t on_read(char* ptr, size_t size, size_t nmemb, void* data);
    static size_t on_write(char* ptr, size_t size, size_t nmemb, void* data);
    static int on_xferinfo(void* data, curl_off_t downloadTotal, curl_off_t downloadNow, curl_off_t uploadTotal, curl_off_t uploadNow);

private:
    CURL* m_curl          = nullptr;
    curl_slist* m_headers = nullptr;

    std::string m_url;

    std::function<size_t(char*, size_t)> m_writeCallback;
    std::function<size_t(char*, size_t)> m_readCallback;
    std::function<int(curl_off_t, curl_off_t, curl_off_t, curl_off_t)> m_progressCallback;

    float m_downloadPercent      = 0.0f;
    curl_off_t m_downloadCurrent = 0;
    curl_off_t m_downloadMax     = 0;

    float m_uploadPercent      = 0.0f;
    curl_off_t m_uploadCurrent = 0;
    curl_off_t m_uploadMax     = 0;
};

#endif
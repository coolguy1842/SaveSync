#include <Util/CURLEasy.hpp>
#include <format>

const char* getMethodName(const CURLEasyMethod& method) {
    switch(method) {
    case GET:     return "GET";
    case HEAD:    return "HEAD";
    case POST:    return "POST";
    case PUT:     return "PUT";
    case DELETE:  return "DELETE";
    case CONNECT: return "CONNECT";
    case OPTIONS: return "OPTIONS";
    case TRACE:   return "TRACE";
    case PATCH:   return "PATCH";
    default:      return "HEAD";
    }
}

size_t CURLEasy::on_read(char* ptr, size_t size, size_t nmemb, void* data) {
    auto callback = reinterpret_cast<CURLEasy*>(data)->m_readCallback;
    if(callback == nullptr) {
        return (size_t)CURL_READFUNC_ABORT;
    }

    return callback(ptr, size * nmemb);
}

size_t CURLEasy::on_write(char* ptr, size_t size, size_t nmemb, void* data) {
    auto callback = reinterpret_cast<CURLEasy*>(data)->m_writeCallback;
    if(callback == nullptr) {
        return size * nmemb;
    }

    return callback(ptr, size * nmemb);
}

int CURLEasy::on_xferinfo(void* data, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    return reinterpret_cast<CURLEasy*>(data)->handleXFERInfo(dltotal, dlnow, ultotal, ulnow);
}

CURLEasy::CURLEasy()
    : m_curl(curl_easy_init()) {
    curl_easy_setopt(m_curl, CURLOPT_READFUNCTION, &CURLEasy::on_read);
    curl_easy_setopt(m_curl, CURLOPT_READDATA, this);

    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, &CURLEasy::on_write);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, this);

    curl_easy_setopt(m_curl, CURLOPT_XFERINFOFUNCTION, &CURLEasy::on_xferinfo);
    curl_easy_setopt(m_curl, CURLOPT_XFERINFODATA, this);
}

CURLEasy::CURLEasy(const CURLEasyOptions& options)
    : CURLEasy() {
    setOptions(options);
}

CURLEasy::~CURLEasy() {
    if(m_headers != nullptr) {
        curl_slist_free_all(m_headers);
    }

    if(m_curl != nullptr) {
        curl_easy_cleanup(m_curl);
    }
}

void CURLEasy::setHeader(std::string header, std::string value) {
    m_headers = curl_slist_append(m_headers, std::format("{}: {}", header, value).c_str());
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_headers);
}

void CURLEasy::setOptions(const CURLEasyOptions& options) {
    if(options.url.has_value()) {
        m_url = options.url.value();
        curl_easy_setopt(m_curl, CURLOPT_URL, m_url.c_str());
    }

    if(options.method.has_value()) curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, getMethodName(options.method.value()));
    if(options.contentType.has_value()) setHeader("Content-Type", options.contentType.value());

    if(options.followLocation.has_value()) curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, options.followLocation.value());
    if(options.maximumRedirects.has_value()) curl_easy_setopt(m_curl, CURLOPT_MAXREDIRS, options.maximumRedirects.value());

    if(options.noBody.has_value()) curl_easy_setopt(m_curl, CURLOPT_NOBODY, options.noBody.value());
    if(options.trackProgress.has_value()) curl_easy_setopt(m_curl, CURLOPT_NOPROGRESS, !options.trackProgress.value());
    if(options.customProgressFunction.has_value()) m_progressCallback = options.customProgressFunction.value();

    if(options.timeout.has_value()) curl_easy_setopt(m_curl, CURLOPT_TIMEOUT, options.timeout.value());
    if(options.connectTimeout.has_value()) curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT, options.connectTimeout.value());

    if(options.timeoutMS.has_value()) curl_easy_setopt(m_curl, CURLOPT_TIMEOUT_MS, options.timeoutMS.value());
    if(options.connectTimeoutMS.has_value()) curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT_MS, options.connectTimeoutMS.value());
    if(options.acceptTimeoutMS.has_value()) curl_easy_setopt(m_curl, CURLOPT_ACCEPTTIMEOUT_MS, options.acceptTimeoutMS.value());

    if(options.lowSpeed.has_value()) {
        curl_easy_setopt(m_curl, CURLOPT_LOW_SPEED_LIMIT, options.lowSpeed->limit);
        curl_easy_setopt(m_curl, CURLOPT_LOW_SPEED_TIME, options.lowSpeed->time);
    }

    if(options.write.has_value() && options.write->callback != nullptr) {
        m_writeCallback = options.write->callback;

        if(options.write->bufferSize.has_value()) {
            curl_easy_setopt(m_curl, CURLOPT_BUFFERSIZE, options.write->bufferSize.value());
        }
    }
    else {
        m_writeCallback = nullptr;
    }

    if(options.read.has_value() && options.read->callback != nullptr) {
        m_readCallback = options.read->callback;

        curl_easy_setopt(m_curl, CURLOPT_UPLOAD, true);
        curl_easy_setopt(m_curl, CURLOPT_INFILESIZE, options.read->dataSize);

        if(options.read->bufferSize.has_value()) {
            curl_easy_setopt(m_curl, CURLOPT_UPLOAD_BUFFERSIZE, options.read->bufferSize.value());
        }
    }
    else {
        m_readCallback = nullptr;

        curl_easy_setopt(m_curl, CURLOPT_UPLOAD, false);
        curl_easy_setopt(m_curl, CURLOPT_INFILESIZE, -1);
    }
}

CURLcode CURLEasy::perform() { return curl_easy_perform(m_curl); }
long CURLEasy::statusCode() {
    long code = 404;
    getInfo(CURLINFO_RESPONSE_CODE, &code);

    return code;
}

CURL* CURLEasy::getHandle() { return m_curl; }

// 0 to 100
float CURLEasy::getDownloadPercent() const { return m_downloadPercent; }
curl_off_t CURLEasy::getDownloadCurrent() const { return m_downloadCurrent; }
curl_off_t CURLEasy::getDownloadMax() const { return m_downloadMax; }

// 0 to 100
float CURLEasy::getUploadPercent() const { return m_uploadPercent; }
curl_off_t CURLEasy::getUploadCurrent() const { return m_uploadCurrent; }
curl_off_t CURLEasy::getUploadMax() const { return m_uploadMax; }

int CURLEasy::handleXFERInfo(curl_off_t downloadTotal, curl_off_t downloadNow, curl_off_t uploadTotal, curl_off_t uploadNow) {
    // stop divide by 0
    m_downloadCurrent = downloadNow;
    m_downloadMax     = downloadTotal;

    m_uploadCurrent = uploadNow;
    m_uploadMax     = uploadTotal;

    float newPercent = downloadTotal != 0 ? downloadNow / static_cast<float>(downloadTotal) : 0;
    if(m_downloadPercent != newPercent) {
        m_downloadPercent = newPercent;
    }

    newPercent = uploadTotal != 0 ? uploadNow / static_cast<float>(uploadTotal) : 0;
    if(m_uploadPercent != newPercent) {
        m_uploadPercent = newPercent;
    }

    if(m_progressCallback != nullptr) {
        return m_progressCallback(downloadTotal, downloadNow, uploadTotal, uploadNow);
    }

    return 0;
}

std::string CURLEasy::escape(std::string str) {
    char* escaped = curl_easy_escape(m_curl, str.c_str(), static_cast<int>(str.size()));
    std::string out(escaped);
    curl_free(escaped);

    return out;
}
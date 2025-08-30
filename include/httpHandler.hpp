#ifndef __HTTP_HANDLER_HPP__
#define __HTTP_HANDLER_HPP__

#include <3ds.h>

#include <functional>
#include <string>

#define CHECK_RESULT(result) \
    res = result;            \
    if(!R_SUCCEEDED(res)) {  \
        return res;          \
    }

typedef std::function<Result(httpcContext& context, const std::string& url, void* extraData)> SetupContextCallback;

static const char* USER_AGENT = "SaveSync/1.0.0";

static const SetupContextCallback GetMethod = [](httpcContext& context, const std::string& url, void* extraData) {
    Result res;

    CHECK_RESULT(httpcOpenContext(&context, HTTPC_METHOD_GET, url.c_str(), 0));
    CHECK_RESULT(httpcSetSSLOpt(&context, SSLCOPT_DisableVerify));
    CHECK_RESULT(httpcSetKeepAlive(&context, HTTPC_KEEPALIVE_ENABLED));
    CHECK_RESULT(httpcAddRequestHeaderField(&context, "User-Agent", USER_AGENT));

    return res;
};

// add in extra callback to add post data
static const SetupContextCallback PostMethod = [](httpcContext& context, const std::string& url, void* extraData) {
    Result res;

    CHECK_RESULT(httpcOpenContext(&context, HTTPC_METHOD_POST, url.c_str(), 0));
    CHECK_RESULT(httpcSetSSLOpt(&context, SSLCOPT_DisableVerify));
    CHECK_RESULT(httpcSetKeepAlive(&context, HTTPC_KEEPALIVE_ENABLED));
    CHECK_RESULT(httpcAddRequestHeaderField(&context, "User-Agent", USER_AGENT));

    return res;
};

static const SetupContextCallback PutMethod = [](httpcContext& context, const std::string& url, void* extraData) {
    Result res;

    CHECK_RESULT(httpcOpenContext(&context, HTTPC_METHOD_PUT, url.c_str(), 0));
    CHECK_RESULT(httpcSetSSLOpt(&context, SSLCOPT_DisableVerify));
    CHECK_RESULT(httpcSetKeepAlive(&context, HTTPC_KEEPALIVE_ENABLED));
    CHECK_RESULT(httpcAddRequestHeaderField(&context, "User-Agent", USER_AGENT));

    return res;
};

#undef CHECK_RESULT

class HttpHandler {
public:
    ~HttpHandler();

    static HttpHandler* instance();
    static void close();

    // handles redirects etc
    Result sendRequest(const std::string& url, std::string* response = nullptr, const SetupContextCallback& setupContextFunc = GetMethod, void* extraData = nullptr);

private:
    HttpHandler();

    bool isRedirect(u32 statusCode);
};

#endif
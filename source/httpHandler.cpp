#include <httpHandler.hpp>
#include <vector>

HttpHandler::HttpHandler() {
    httpcInit(0x400000);  // Buffer size when POST/PUT.
}

HttpHandler::~HttpHandler() {
    httpcExit();
}

bool HttpHandler::isRedirect(u32 statusCode) {
    return (statusCode >= 301 && statusCode <= 303) || (statusCode >= 307 && statusCode <= 308);
}

Result HttpHandler::sendRequest(const std::string& url, std::string* response, const SetupContextCallback& setupContextFunc, void* extraData) {
#define CHECK_RESULT(result)         \
    res = result;                    \
    if(!R_SUCCEEDED(res)) {          \
        httpcCloseContext(&context); \
        return res;                  \
    }

    if(response != nullptr) {
        response->clear();
    }

    httpcContext context;

    Result res;
    std::string currentURL = url;
    u32 statusCode         = 0;

    do {
        CHECK_RESULT(setupContextFunc(context, currentURL, extraData));
        CHECK_RESULT(httpcBeginRequest(&context));
        CHECK_RESULT(httpcGetResponseStatusCode(&context, &statusCode));

        if(isRedirect(statusCode)) {
            std::string newURL;
            newURL.resize(0x1000);

            CHECK_RESULT(httpcGetResponseHeader(&context, "Location", newURL.data(), 0x1000));
            if(newURL[0] == '/') {
                size_t protocolPos = currentURL.find("://");
                if(protocolPos == std::string::npos) {
                    CHECK_RESULT(MAKERESULT(RL_RESET, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_COMBINATION));
                }

                size_t pos = currentURL.find_first_of('/', protocolPos + 3);
                if(pos == std::string::npos) {
                    pos = currentURL.size();
                }

                newURL = currentURL.substr(0, pos) + newURL;
            }

            currentURL = newURL;
            httpcCloseContext(&context);
        }
    } while(isRedirect(statusCode));

    CHECK_RESULT(httpcGetResponseStatusCode(&context, &statusCode));

    if(!((statusCode >= 200 && statusCode <= 208) || statusCode == 226)) {
        httpcCloseContext(&context);
        return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }

    if(response != nullptr && (statusCode == 200 || statusCode == 203 || statusCode == 206 || statusCode == 207)) {
        u32 contentSize = 0, readSize = 0;
        CHECK_RESULT(httpcGetDownloadSizeState(&context, NULL, &contentSize));

        u8 buf[0x1000];
        do {
            CHECK_RESULT(httpcDownloadData(&context, buf, 0x1000, &readSize));
            response->append((char*)buf, readSize);
        } while(res == (s32)HTTPC_RESULTCODE_DOWNLOADPENDING);
    }

    return RL_SUCCESS;
#undef CHECK_RESULT
}

HttpHandler* handlerInstance = nullptr;
HttpHandler* HttpHandler::instance() {
    if(handlerInstance == nullptr) {
        handlerInstance = new HttpHandler();
    }

    return handlerInstance;
}

void HttpHandler::close() {
    if(handlerInstance == nullptr) {
        return;
    }

    delete handlerInstance;
}
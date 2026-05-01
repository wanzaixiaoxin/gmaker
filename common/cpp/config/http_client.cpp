#include "http_client.hpp"

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

#include <sstream>
#include <cstdio>

namespace gs {
namespace config {

bool HttpClient::ParseUrl(const std::string& url, std::string& host, std::string& path, int& port, bool& https) {
    // 简单解析 http://host:port/path
    https = false;
    port = 80;
    size_t pos = 0;

    if (url.compare(0, 8, "https://") == 0) {
        https = true;
        port = 443;
        pos = 8;
    } else if (url.compare(0, 7, "http://") == 0) {
        pos = 7;
    } else {
        pos = 0; // 无协议前缀
    }

    size_t slash = url.find('/', pos);
    if (slash == std::string::npos) {
        host = url.substr(pos);
        path = "/";
    } else {
        host = url.substr(pos, slash - pos);
        path = url.substr(slash);
    }

    // 解析端口
    size_t colon = host.find(':');
    if (colon != std::string::npos) {
        port = std::atoi(host.substr(colon + 1).c_str());
        host = host.substr(0, colon);
    }
    return true;
}

std::string HttpClient::Get(const std::string& url, long* last_error) {
#ifdef _WIN32
    std::string host, path;
    int port = 80;
    bool https = false;
    if (!ParseUrl(url, host, path, port, https)) {
        if (last_error) *last_error = -1;
        return "";
    }

    HINTERNET hSession = WinHttpOpen(L"gmaker-config-watcher/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        if (last_error) *last_error = GetLastError();
        return "";
    }

    // 宽字符转换
    int hostLen = MultiByteToWideChar(CP_UTF8, 0, host.c_str(), -1, nullptr, 0);
    std::vector<wchar_t> whost(hostLen);
    MultiByteToWideChar(CP_UTF8, 0, host.c_str(), -1, whost.data(), hostLen);

    HINTERNET hConnect = WinHttpConnect(hSession, whost.data(), (INTERNET_PORT)port, 0);
    if (!hConnect) {
        if (last_error) *last_error = GetLastError();
        WinHttpCloseHandle(hSession);
        return "";
    }

    int pathLen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    std::vector<wchar_t> wpath(pathLen);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wpath.data(), pathLen);

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wpath.data(),
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        https ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) {
        if (last_error) *last_error = GetLastError();
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    BOOL bResults = WinHttpSendRequest(hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!bResults) {
        if (last_error) *last_error = GetLastError();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    bResults = WinHttpReceiveResponse(hRequest, nullptr);
    if (!bResults) {
        if (last_error) *last_error = GetLastError();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    DWORD statusCode = 0;
    DWORD size = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &size, WINHTTP_NO_HEADER_INDEX);

    if (statusCode != 200) {
        if (last_error) *last_error = statusCode;
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    std::string response;
    DWORD downloaded = 0;
    do {
        DWORD avail = 0;
        WinHttpQueryDataAvailable(hRequest, &avail);
        if (avail == 0) break;
        std::vector<char> buf(avail + 1);
        WinHttpReadData(hRequest, buf.data(), avail, &downloaded);
        buf[downloaded] = '\0';
        response.append(buf.data(), downloaded);
    } while (downloaded > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (last_error) *last_error = 0;
    return response;
#else
    // Linux/macOS fallback: 使用简单 socket 实现（预留）
    (void)url;
    if (last_error) *last_error = -2;
    return "";
#endif
}

} // namespace config
} // namespace gs

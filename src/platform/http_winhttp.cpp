// Windows: WinHTTP, with the system's proxy settings and certificate store.

#include "platform/http.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>

#include <cwctype>
#include <memory>

namespace glideslope::platform {

namespace {

struct InternetHandle {
    void operator()(HINTERNET h) const {
        if (h != nullptr) {
            WinHttpCloseHandle(h);
        }
    }
};
using Handle = std::unique_ptr<void, InternetHandle>;

std::wstring widen(const std::string& s) {
    if (s.empty()) {
        return {};
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
                                      nullptr, 0);
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(),
                        n);
    return out;
}

std::string narrow(const std::wstring& s) {
    if (s.empty()) {
        return {};
    }
    const int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
                                      nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n,
                        nullptr, nullptr);
    return out;
}

[[noreturn]] void fail(const std::string& url, const char* what) {
    const DWORD code = GetLastError();
    throw HttpError(url + ": " + what + " (WinHTTP error " + std::to_string(code) +
                    ")");
}

} // namespace

std::string http_client() {
    return "WinHTTP";
}

HttpResponse http_get(const HttpRequest& request) {
    std::wstring url = widen(request.url);
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof parts;
    parts.dwSchemeLength = static_cast<DWORD>(-1);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts)) {
        fail(request.url, "not a URL WinHTTP can read");
    }
    const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.lpszExtraInfo != nullptr) {
        path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    }
    const bool secure = parts.nScheme == INTERNET_SCHEME_HTTPS;

    const Handle session(WinHttpOpen(
        widen(request.user_agent).c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) {
        fail(request.url, "no WinHTTP session");
    }
    // **WinHTTP undoes a compressed body, where it can.** Turning this on
    // once before made every Open-Meteo fetch fail on all three Windows jobs
    // and it was turned off again without the cause being found. The cause
    // was almost certainly the read loop below, which used
    // WinHttpQueryDataAvailable to decide the body had ended - which
    // Microsoft's documentation says not to do, and which is not the
    // decompressed length when an encoding is being undone. That loop is
    // fixed, so this is on again.
    //
    // **Its result is checked**, unlike last time: an option that did not
    // take is an option that quietly does nothing, and the body would then
    // arrive compressed with nothing to say so. Where it cannot be set -
    // it wants Windows 8.1 - the body is left as it came and
    // `content-encoding` is left on it, which is what tells a caller the
    // bytes are not what they look like.
    DWORD decompress = WINHTTP_DECOMPRESSION_FLAG_ALL;
    const bool undoing =
        WinHttpSetOption(session.get(), WINHTTP_OPTION_DECOMPRESSION, &decompress,
                         sizeof decompress) == TRUE;
    const int connect_ms = request.connect_timeout_seconds * 1000;
    const int stall_ms = request.stall_timeout_seconds * 1000;
    WinHttpSetTimeouts(session.get(), connect_ms, connect_ms, stall_ms, stall_ms);
    const Handle connection(
        WinHttpConnect(session.get(), host.c_str(), parts.nPort, 0));
    if (!connection) {
        fail(request.url, "could not connect");
    }
    const Handle handle(WinHttpOpenRequest(
        connection.get(), L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0));
    if (!handle) {
        fail(request.url, "could not open the request");
    }
    // The request's own headers, as CRLF-separated "name: value" lines, which
    // is the form WinHttpSendRequest takes.
    std::wstring sent;
    for (const auto& [name, value] : request.headers) {
        sent += widen(name) + L": " + widen(value) + L"\r\n";
    }
    if (!WinHttpSendRequest(handle.get(),
                            sent.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS
                                         : sent.c_str(),
                            sent.empty() ? 0 : static_cast<DWORD>(-1),
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        fail(request.url, "could not send the request");
    }
    if (!WinHttpReceiveResponse(handle.get(), nullptr)) {
        fail(request.url, "no response");
    }

    HttpResponse response;
    DWORD status = 0;
    DWORD size = sizeof status;
    if (!WinHttpQueryHeaders(
            handle.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX)) {
        fail(request.url, "no status code");
    }
    response.status = static_cast<int>(status);

    size = 0;
    WinHttpQueryHeaders(handle.get(), WINHTTP_QUERY_RAW_HEADERS_CRLF,
                        WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER, &size,
                        WINHTTP_NO_HEADER_INDEX);
    if (size > 0) {
        std::wstring raw(size / sizeof(wchar_t), L'\0');
        if (WinHttpQueryHeaders(handle.get(), WINHTTP_QUERY_RAW_HEADERS_CRLF,
                                WINHTTP_HEADER_NAME_BY_INDEX, raw.data(), &size,
                                WINHTTP_NO_HEADER_INDEX)) {
            raw.resize(size / sizeof(wchar_t));
            std::size_t at = 0;
            while (at < raw.size()) {
                std::size_t end = raw.find(L"\r\n", at);
                if (end == std::wstring::npos) {
                    end = raw.size();
                }
                const std::wstring line = raw.substr(at, end - at);
                at = end + 2;
                const auto colon = line.find(L':');
                if (colon == std::wstring::npos) {
                    continue; // the status line
                }
                std::wstring name = line.substr(0, colon);
                for (wchar_t& ch : name) {
                    ch = static_cast<wchar_t>(std::towlower(ch));
                }
                std::size_t start = colon + 1;
                while (start < line.size() && line[start] == L' ') {
                    ++start;
                }
                response.headers[narrow(name)] = narrow(line.substr(start));
            }
        }
    }

    // **The end of a body is a read of no bytes, not a count of none.**
    // WinHttpQueryDataAvailable's answer must not be used to decide that a
    // response has ended - Microsoft says so plainly, because not all servers
    // terminate a response properly - and it is not the decompressed length
    // when WinHTTP is undoing an encoding. So the body is read in fixed
    // chunks until a read returns nothing, which is what its documentation
    // asks for and is right whether anything is being decompressed or not.
    constexpr DWORD chunk = 16 * 1024; // its advice is 8 KiB or more
    for (;;) {
        const std::size_t at = response.body.size();
        response.body.resize(at + chunk);
        DWORD read = 0;
        if (!WinHttpReadData(handle.get(), response.body.data() + at, chunk, &read)) {
            fail(request.url, "the transfer failed");
        }
        response.body.resize(at + read);
        if (read == 0) {
            break; // the end of the body, as a local file's end-of-file
        }
        if (response.body.size() > request.max_body) {
            throw HttpError(request.url + ": the body is more than " +
                            std::to_string(request.max_body) + " bytes");
        }
    }
    // WinHTTP undid the encoding, so that header describes the wire and not
    // the body - but only if the option took. See HttpResponse.
    if (undoing) {
        response.headers.erase("content-encoding");
    }
    response.headers["content-length"] = std::to_string(response.body.size());

    return response;
}

} // namespace glideslope::platform

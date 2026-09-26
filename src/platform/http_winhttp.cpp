// Windows: WinHTTP, with the system's proxy settings and certificate store.

#include "platform/http.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>

#include <chrono>
#include <condition_variable>
#include <cwctype>
#include <memory>
#include <mutex>
#include <thread>

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

// **Gives a request up when it is abandoned**, from a thread of its own.
//
// WinHTTP's synchronous calls block, and a body fed a byte at a time never
// trips a timeout. Microsoft documents closing the request's handle as the way
// to end a synchronous request in progress from elsewhere: the call blocked on
// it then fails. So this watches the request's flag, and closes the handle if
// it is raised; the handle then belongs to it, and is not closed again.
//
// **Once closed, the handle is not used again**: `perform` asks `given_up()`
// before every WinHTTP call that follows the response, and throws instead.
//
// **A race is left, and it is named rather than hidden.** The watcher can close
// the handle between that question and the call that follows it. The call
// then gets a handle value that is no longer this request's - and WinHTTP may
// already have handed the same value to another thread's new request, which
// the call would then act on. Holding the lock across the call would close
// the gap only for calls that never block, and ReadData is the one that
// blocks, which is the whole reason the handle is closed from elsewhere. The
// window is one call wide and opens only while a program is ending or a
// terrain is closing, when nothing else is being fetched that it would harm.
class Abandoner {
public:
    Abandoner(const HttpRequest& request, Handle& handle)
        : request_(request), handle_(handle) {
        if (request_.abandon != nullptr) {
            watcher_ = std::thread([this] { watch(); });
        }
    }
    ~Abandoner() {
        {
            const std::lock_guard lock(mutex_);
            finished_ = true;
        }
        wake_.notify_all();
        if (watcher_.joinable()) {
            watcher_.join();
        }
        if (closed_) {
            (void)handle_.release(); // closed already, by the watcher
        }
    }
    Abandoner(const Abandoner&) = delete;
    Abandoner& operator=(const Abandoner&) = delete;

    bool given_up() {
        const std::lock_guard lock(mutex_);
        return closed_;
    }

private:
    void watch() {
        std::unique_lock lock(mutex_);
        while (!finished_) {
            if (abandoned(request_)) {
                WinHttpCloseHandle(handle_.get());
                closed_ = true;
                return;
            }
            wake_.wait_for(lock, std::chrono::milliseconds(50));
        }
    }

    const HttpRequest& request_;
    Handle& handle_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::thread watcher_;
    bool finished_ = false;
    bool closed_ = false;
};

} // namespace

std::string http_client() {
    return "WinHTTP";
}

namespace {

// A GET, or a POST of `body` where there is one.
HttpResponse perform(const HttpRequest& request, const std::string* body) {
    refuse_unsafe_headers(request);
    if (abandoned(request)) {
        throw HttpError(request.url + ": given up before it began");
    }
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
    // **WinHTTP is not asked to undo a compressed body**, and the second
    // attempt to make it said something the first did not.
    //
    // Turning WINHTTP_OPTION_DECOMPRESSION on makes every Open-Meteo fetch
    // fail on all three Windows jobs. The first time, the error was recorded
    // as 2147500036, which is 0x80004004, E_ABORT - not a WinHTTP code at
    // all, and a red herring. The second time it was **12002 at
    // WinHttpSendRequest**: ERROR_WINHTTP_TIMEOUT, raised before a single
    // byte of the body is read.
    //
    // That rules out the guess it was tried on - that the read loop below,
    // which used WinHttpQueryDataAvailable to decide a body had ended, was
    // the cause. It was wrong for its own reasons and is fixed, and the
    // failure happens earlier than it runs.
    //
    // What is left is that asking for a compressed body from this host, from
    // these runners, times out the send. Why is still not known. Nothing
    // asks for a compressed body otherwise, since a provider's own
    // Accept-Encoding is not passed on, so this costs nothing until a server
    // compresses one unasked - which Cesium ion does. That is a tail in
    // COMPLETION_PLAN.md, and it is why Cesium ion is not yet known to work
    // on Windows.
    const int connect_ms = request.connect_timeout_seconds * 1000;
    const int stall_ms = request.stall_timeout_seconds * 1000;
    WinHttpSetTimeouts(session.get(), connect_ms, connect_ms, stall_ms, stall_ms);
    const Handle connection(
        WinHttpConnect(session.get(), host.c_str(), parts.nPort, 0));
    if (!connection) {
        fail(request.url, "could not connect");
    }
    Handle handle(WinHttpOpenRequest(
        connection.get(), body != nullptr ? L"POST" : L"GET", path.c_str(), nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0));
    if (!handle) {
        fail(request.url, "could not open the request");
    }
    // From here the request can be given up; each failure below then says so.
    Abandoner abandoner(request, handle);
    const auto fail_or_given_up = [&](const char* what) {
        if (abandoner.given_up()) {
            throw HttpError(request.url + ": given up, unfinished");
        }
        fail(request.url, what);
    };
    // A GET follows redirects; a POST does not (http.hpp says why).
    if (body != nullptr) {
        DWORD disable = WINHTTP_DISABLE_REDIRECTS;
        if (!WinHttpSetOption(handle.get(), WINHTTP_OPTION_DISABLE_FEATURE, &disable,
                              sizeof disable)) {
            fail_or_given_up("could not turn redirects off");
        }
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
                            body != nullptr ? const_cast<char*>(body->data())
                                            : WINHTTP_NO_REQUEST_DATA,
                            body != nullptr ? static_cast<DWORD>(body->size()) : 0,
                            body != nullptr ? static_cast<DWORD>(body->size()) : 0, 0)) {
        fail_or_given_up("could not send the request");
    }
    if (!WinHttpReceiveResponse(handle.get(), nullptr)) {
        fail_or_given_up("no response");
    }
    // Each call from here is on a handle the watcher may have closed: asked
    // first, so that a closed one is not used (see Abandoner).
    const auto unless_given_up = [&] {
        if (abandoner.given_up()) {
            throw HttpError(request.url + ": given up, unfinished");
        }
    };

    HttpResponse response;
    DWORD status = 0;
    DWORD size = sizeof status;
    unless_given_up();
    if (!WinHttpQueryHeaders(
            handle.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX)) {
        fail_or_given_up("no status code");
    }
    response.status = static_cast<int>(status);

    size = 0;
    unless_given_up();
    WinHttpQueryHeaders(handle.get(), WINHTTP_QUERY_RAW_HEADERS_CRLF,
                        WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER, &size,
                        WINHTTP_NO_HEADER_INDEX);
    if (size > 0) {
        std::wstring raw(size / sizeof(wchar_t), L'\0');
        unless_given_up();
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
        unless_given_up();
        if (!WinHttpReadData(handle.get(), response.body.data() + at, chunk, &read)) {
            fail_or_given_up("the transfer failed");
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
    // **`content-encoding` is left on the response**, because nothing here
    // undid one. A caller that sees it knows the bytes are not what they look
    // like. See HttpResponse.
    response.headers["content-length"] = std::to_string(response.body.size());

    return response;
}

} // namespace

HttpResponse http_get(const HttpRequest& request) {
    return perform(request, nullptr);
}

HttpResponse http_post(const HttpRequest& request, const std::string& body) {
    return perform(request, &body);
}

} // namespace glideslope::platform

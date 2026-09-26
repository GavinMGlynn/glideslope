// Linux: the system's libcurl, loaded at run time.
//
// Linked at build time it would be one more development package on every
// build machine, and one more shared library the package names and every
// machine must have in a compatible version. Loaded at run time, the program
// builds and starts without it, and asks only when a download is wanted - and
// libcurl.so.4's interface has not changed incompatibly since 2006. The
// options used are its oldest and most stable.

#include "platform/http.hpp"

#include <dlfcn.h>

#include <cctype>
#include <csignal>
#include <mutex>

namespace glideslope::platform {

namespace {

using Handle = void*;

// The option and information numbers from curl.h: fixed by libcurl's ABI.
enum : int {
    curlopt_writedata = 10001,
    curlopt_url = 10002,
    curlopt_errorbuffer = 10010,
    curlopt_postfields = 10015,
    curlopt_postfieldsize = 60,
    curlopt_useragent = 10018,
    curlopt_headerdata = 10029,
    curlopt_httpheader = 10023,
    curlopt_accept_encoding = 10102,
    curlopt_low_speed_limit = 19,
    curlopt_low_speed_time = 20,
    curlopt_followlocation = 52,
    curlopt_maxredirs = 68,
    curlopt_connecttimeout = 78,
    curlopt_nosignal = 99,
    curlopt_noprogress = 43,
    curlopt_xferinfodata = 10057,
    curlopt_xferinfofunction = 20219,
    curlopt_writefunction = 20011,
    curlopt_headerfunction = 20079,
    curlinfo_response_code = 0x200002,
    curl_global_default = 3,
};

struct Curl {
    void* library = nullptr;
    int (*global_init)(long) = nullptr;
    Handle (*easy_init)() = nullptr;
    int (*easy_setopt)(Handle, int, ...) = nullptr;
    int (*easy_perform)(Handle) = nullptr;
    int (*easy_getinfo)(Handle, int, ...) = nullptr;
    void (*easy_cleanup)(Handle) = nullptr;
    void* (*slist_append)(void*, const char*) = nullptr;
    void (*slist_free_all)(void*) = nullptr;
    const char* (*easy_strerror)(int) = nullptr;
    const char* (*version)() = nullptr;
    std::string error;
};

template <typename F> F symbol(void* library, const char* name) {
    // dlsym gives an object pointer; POSIX guarantees it converts to a
    // function pointer, which C++ can only say with reinterpret_cast.
    return reinterpret_cast<F>(dlsym(library, name));
}

const Curl& curl() {
    static Curl c;
    static std::once_flag once;
    std::call_once(once, [] {
        for (const char* name : {"libcurl.so.4", "libcurl-gnutls.so.4", "libcurl.so"}) {
            c.library = dlopen(name, RTLD_NOW | RTLD_LOCAL);
            if (c.library != nullptr) {
                break;
            }
        }
        if (c.library == nullptr) {
            c.error =
                "no libcurl on this system (libcurl.so.4); install libcurl to download";
            return;
        }
        c.global_init = symbol<int (*)(long)>(c.library, "curl_global_init");
        c.easy_init = symbol<Handle (*)()>(c.library, "curl_easy_init");
        c.easy_setopt =
            symbol<int (*)(Handle, int, ...)>(c.library, "curl_easy_setopt");
        c.easy_perform = symbol<int (*)(Handle)>(c.library, "curl_easy_perform");
        c.easy_getinfo =
            symbol<int (*)(Handle, int, ...)>(c.library, "curl_easy_getinfo");
        c.easy_cleanup = symbol<void (*)(Handle)>(c.library, "curl_easy_cleanup");
        c.easy_strerror = symbol<const char* (*)(int)>(c.library, "curl_easy_strerror");
        c.slist_append =
            symbol<void* (*)(void*, const char*)>(c.library, "curl_slist_append");
        c.slist_free_all = symbol<void (*)(void*)>(c.library, "curl_slist_free_all");
        c.version = symbol<const char* (*)()>(c.library, "curl_version");
        if (c.global_init == nullptr || c.easy_init == nullptr ||
            c.easy_setopt == nullptr || c.easy_perform == nullptr ||
            c.easy_getinfo == nullptr || c.easy_cleanup == nullptr ||
            c.easy_strerror == nullptr || c.version == nullptr ||
            c.slist_append == nullptr || c.slist_free_all == nullptr) {
            c.error = "the system's libcurl lacks the functions it should have";
            return;
        }
        if (c.global_init(curl_global_default) != 0) {
            c.error = "libcurl would not initialise";
        }
        // Every request is made with CURLOPT_NOSIGNAL, as threads must, and
        // then ignoring SIGPIPE is the program's to do, not libcurl's: TLS
        // writing to a connection the server has closed raises it, and by
        // default it ends the process.
        std::signal(SIGPIPE, SIG_IGN);
    });
    if (!c.error.empty()) {
        throw HttpError(c.error);
    }
    return c;
}

struct Transfer {
    HttpResponse response;
    std::uint64_t max_body = 0;
    bool too_big = false;
    const HttpRequest* request = nullptr;
    bool given_up = false;
};

// **Asked about often, even when nothing arrives**: libcurl calls this many
// times a second while bytes flow and about once a second while none do, so
// an abandoned transfer ends within a second whatever it is waiting on.
// Returning non-zero is what ends it.
int on_progress(void* user, std::int64_t, std::int64_t, std::int64_t, std::int64_t) {
    auto* t = static_cast<Transfer*>(user);
    if (abandoned(*t->request)) {
        t->given_up = true;
        return 1;
    }
    return 0;
}

std::size_t on_body(const char* data, std::size_t size, std::size_t count, void* user) {
    auto* t = static_cast<Transfer*>(user);
    const std::size_t n = size * count;
    if (t->response.body.size() + n > t->max_body) {
        t->too_big = true;
        return 0; // abandons the transfer
    }
    t->response.body.insert(t->response.body.end(), data, data + n);
    return n;
}

std::size_t on_header(const char* data, std::size_t size, std::size_t count,
                      void* user) {
    auto* t = static_cast<Transfer*>(user);
    const std::size_t n = size * count;
    std::string line(data, n);
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
        line.pop_back();
    }
    // Each response of a redirect chain starts with its status line; only the
    // last response's headers are kept.
    if (line.rfind("HTTP/", 0) == 0) {
        t->response.headers.clear();
        return n;
    }
    const auto colon = line.find(':');
    if (colon != std::string::npos) {
        std::string name = line.substr(0, colon);
        for (char& ch : name) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        std::size_t start = colon + 1;
        while (start < line.size() && line[start] == ' ') {
            ++start;
        }
        t->response.headers[name] = line.substr(start);
    }
    return n;
}

} // namespace

std::string http_client() {
    return std::string("libcurl ") + curl().version();
}

namespace {

// A GET, or a POST of `body` where there is one.
HttpResponse perform(const HttpRequest& request, const std::string* body) {
    refuse_unsafe_headers(request);
    // Before the handle is made, so that nothing is left to clean up.
    if (abandoned(request)) {
        throw HttpError(request.url + ": given up before it began");
    }
    const Curl& c = curl();
    const Handle handle = c.easy_init();
    if (handle == nullptr) {
        throw HttpError("libcurl would not make a handle");
    }
    Transfer transfer;
    transfer.max_body = request.max_body;
    transfer.request = &request;
    char error[256] = {};
    c.easy_setopt(handle, curlopt_url, request.url.c_str());
    c.easy_setopt(handle, curlopt_useragent, request.user_agent.c_str());
    // Every encoding libcurl can undo, undone by it: a body arrives plain
    // whatever the server chose to compress it with.
    c.easy_setopt(handle, curlopt_accept_encoding, "");
    // A GET follows redirects; a POST does not (http.hpp says why).
    c.easy_setopt(handle, curlopt_followlocation, body == nullptr ? 1L : 0L);
    c.easy_setopt(handle, curlopt_maxredirs, 10L);
    c.easy_setopt(handle, curlopt_nosignal, 1L);
    c.easy_setopt(handle, curlopt_connecttimeout,
                  static_cast<long>(request.connect_timeout_seconds));
    c.easy_setopt(handle, curlopt_low_speed_limit, 1L);
    c.easy_setopt(handle, curlopt_low_speed_time,
                  static_cast<long>(request.stall_timeout_seconds));
    c.easy_setopt(handle, curlopt_errorbuffer, error);
    c.easy_setopt(handle, curlopt_noprogress, 0L);
    c.easy_setopt(handle, curlopt_xferinfofunction, &on_progress);
    c.easy_setopt(handle, curlopt_xferinfodata, &transfer);
    c.easy_setopt(handle, curlopt_writefunction, &on_body);
    c.easy_setopt(handle, curlopt_writedata, &transfer);
    c.easy_setopt(handle, curlopt_headerfunction, &on_header);
    c.easy_setopt(handle, curlopt_headerdata, &transfer);
    // The request's own headers, if it has any. libcurl wants "name: value"
    // lines, and owns none of them: the list is freed after the transfer.
    void* sent = nullptr;
    for (const auto& [name, value] : request.headers) {
        const std::string line = name + ": " + value;
        void* grown = c.slist_append(sent, line.c_str());
        if (grown == nullptr) {
            c.slist_free_all(sent);
            c.easy_cleanup(handle);
            throw HttpError(request.url + ": libcurl would not take the header " +
                            name);
        }
        sent = grown;
    }
    if (sent != nullptr) {
        c.easy_setopt(handle, curlopt_httpheader, sent);
    }
    // A body to POST, which libcurl reads from here - not copied - during the
    // transfer; its size given, so that it may hold a zero byte.
    if (body != nullptr) {
        c.easy_setopt(handle, curlopt_postfieldsize, static_cast<long>(body->size()));
        c.easy_setopt(handle, curlopt_postfields, body->data());
    }
    const int result = c.easy_perform(handle);
    long status = 0;
    c.easy_getinfo(handle, curlinfo_response_code, &status);
    c.easy_cleanup(handle);
    c.slist_free_all(sent);
    if (transfer.given_up) {
        throw HttpError(request.url + ": given up, unfinished");
    }
    if (transfer.too_big) {
        throw HttpError(request.url + ": the body is more than " +
                        std::to_string(request.max_body) + " bytes");
    }
    if (result != 0) {
        throw HttpError(request.url + ": " +
                        (error[0] != '\0' ? error : c.easy_strerror(result)));
    }
    transfer.response.status = static_cast<int>(status);
    // libcurl undid whatever encoding it understood, so those two headers
    // would describe the wire and not the body. See HttpResponse.
    transfer.response.headers.erase("content-encoding");
    transfer.response.headers["content-length"] =
        std::to_string(transfer.response.body.size());
    return std::move(transfer.response);
}

} // namespace

HttpResponse http_get(const HttpRequest& request) {
    return perform(request, nullptr);
}

HttpResponse http_post(const HttpRequest& request, const std::string& body) {
    return perform(request, &body);
}

} // namespace glideslope::platform

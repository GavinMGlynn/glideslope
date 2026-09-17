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
#include <mutex>

namespace glideslope::platform {

namespace {

using Handle = void*;

// The option and information numbers from curl.h: fixed by libcurl's ABI.
enum : int {
    curlopt_writedata = 10001,
    curlopt_url = 10002,
    curlopt_errorbuffer = 10010,
    curlopt_useragent = 10018,
    curlopt_headerdata = 10029,
    curlopt_low_speed_limit = 19,
    curlopt_low_speed_time = 20,
    curlopt_followlocation = 52,
    curlopt_maxredirs = 68,
    curlopt_connecttimeout = 78,
    curlopt_nosignal = 99,
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
        c.version = symbol<const char* (*)()>(c.library, "curl_version");
        if (c.global_init == nullptr || c.easy_init == nullptr ||
            c.easy_setopt == nullptr || c.easy_perform == nullptr ||
            c.easy_getinfo == nullptr || c.easy_cleanup == nullptr ||
            c.easy_strerror == nullptr || c.version == nullptr) {
            c.error = "the system's libcurl lacks the functions it should have";
            return;
        }
        if (c.global_init(curl_global_default) != 0) {
            c.error = "libcurl would not initialise";
        }
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
};

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

HttpResponse http_get(const HttpRequest& request) {
    const Curl& c = curl();
    const Handle handle = c.easy_init();
    if (handle == nullptr) {
        throw HttpError("libcurl would not make a handle");
    }
    Transfer transfer;
    transfer.max_body = request.max_body;
    char error[256] = {};
    c.easy_setopt(handle, curlopt_url, request.url.c_str());
    c.easy_setopt(handle, curlopt_useragent, request.user_agent.c_str());
    c.easy_setopt(handle, curlopt_followlocation, 1L);
    c.easy_setopt(handle, curlopt_maxredirs, 10L);
    c.easy_setopt(handle, curlopt_nosignal, 1L);
    c.easy_setopt(handle, curlopt_connecttimeout,
                  static_cast<long>(request.connect_timeout_seconds));
    c.easy_setopt(handle, curlopt_low_speed_limit, 1L);
    c.easy_setopt(handle, curlopt_low_speed_time,
                  static_cast<long>(request.stall_timeout_seconds));
    c.easy_setopt(handle, curlopt_errorbuffer, error);
    c.easy_setopt(handle, curlopt_writefunction, &on_body);
    c.easy_setopt(handle, curlopt_writedata, &transfer);
    c.easy_setopt(handle, curlopt_headerfunction, &on_header);
    c.easy_setopt(handle, curlopt_headerdata, &transfer);
    const int result = c.easy_perform(handle);
    long status = 0;
    c.easy_getinfo(handle, curlinfo_response_code, &status);
    c.easy_cleanup(handle);
    if (transfer.too_big) {
        throw HttpError(request.url + ": the body is more than " +
                        std::to_string(request.max_body) + " bytes");
    }
    if (result != 0) {
        throw HttpError(request.url + ": " +
                        (error[0] != '\0' ? error : c.easy_strerror(result)));
    }
    transfer.response.status = static_cast<int>(status);
    return std::move(transfer.response);
}

} // namespace glideslope::platform

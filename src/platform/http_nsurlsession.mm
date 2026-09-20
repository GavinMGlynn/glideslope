// macOS: NSURLSession, with the system's proxy settings and keychain.
//
// Nothing is thrown inside the autorelease pool: ARC code is not built to be
// unwound through, so failures are carried out of it and thrown after.

#include "platform/http.hpp"

#import <Foundation/Foundation.h>

#include <cctype>

namespace glideslope::platform {

std::string http_client() {
    return "NSURLSession";
}

HttpResponse http_get(const HttpRequest& request) {
    HttpResponse response;
    std::string failure;
    @autoreleasepool {
        NSURL* url =
            [NSURL URLWithString:[NSString stringWithUTF8String:request.url.c_str()]];
        if (url == nil) {
            failure = "not a URL";
        } else {
            NSMutableURLRequest* r = [NSMutableURLRequest
                 requestWithURL:url
                    cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
                timeoutInterval:request.stall_timeout_seconds];
            [r setValue:[NSString stringWithUTF8String:request.user_agent.c_str()]
                forHTTPHeaderField:@"User-Agent"];
            // The request's own headers, if it has any.
            for (const auto& header : request.headers) {
                [r setValue:[NSString stringWithUTF8String:header.second.c_str()]
                    forHTTPHeaderField:[NSString
                                           stringWithUTF8String:header.first.c_str()]];
            }
            NSURLSessionConfiguration* configuration =
                [NSURLSessionConfiguration ephemeralSessionConfiguration];
            configuration.timeoutIntervalForRequest = request.stall_timeout_seconds;
            NSURLSession* session =
                [NSURLSession sessionWithConfiguration:configuration];

            dispatch_semaphore_t done = dispatch_semaphore_create(0);
            __block NSData* data = nil;
            __block NSURLResponse* answer = nil;
            __block NSError* error = nil;
            NSURLSessionDataTask* task = [session
                dataTaskWithRequest:r
                  completionHandler:^(NSData* d, NSURLResponse* a, NSError* e) {
                    data = d;
                    answer = a;
                    error = e;
                    dispatch_semaphore_signal(done);
                  }];
            [task resume];
            dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
            [session finishTasksAndInvalidate];

            if (error != nil) {
                failure = error.localizedDescription.UTF8String;
            } else if (![answer isKindOfClass:[NSHTTPURLResponse class]]) {
                failure = "not an HTTP response";
            } else if (data.length > request.max_body) {
                failure = "the body is more than " + std::to_string(request.max_body) +
                          " bytes";
            } else {
                const auto* http = static_cast<NSHTTPURLResponse*>(answer);
                response.status = static_cast<int>(http.statusCode);
                for (NSString* name in http.allHeaderFields) {
                    std::string key = name.UTF8String;
                    for (char& ch : key) {
                        ch = static_cast<char>(
                            std::tolower(static_cast<unsigned char>(ch)));
                    }
                    response.headers[key] = [http.allHeaderFields[name] UTF8String];
                }
                const auto* bytes = static_cast<const std::uint8_t*>(data.bytes);
                response.body.assign(bytes, bytes + data.length);
            }
        }
    }
    if (!failure.empty()) {
        throw HttpError(request.url + ": " + failure);
    }
    return response;
}

} // namespace glideslope::platform

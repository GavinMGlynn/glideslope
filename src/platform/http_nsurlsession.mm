// macOS: NSURLSession, with the system's proxy settings and keychain.
//
// Nothing is thrown inside the autorelease pool: ARC code is not built to be
// unwound through, so failures are carried out of it and thrown after.

#include "platform/http.hpp"

#import <Foundation/Foundation.h>

#include <cctype>

// **A POST follows no redirect** (http.hpp says why): asked whether to follow
// one, this says no, and the redirect is the response.
@interface GlideslopeNoRedirect : NSObject <NSURLSessionTaskDelegate>
@end

@implementation GlideslopeNoRedirect
- (void)URLSession:(NSURLSession*)session
                          task:(NSURLSessionTask*)task
    willPerformHTTPRedirection:(NSHTTPURLResponse*)response
                    newRequest:(NSURLRequest*)request
             completionHandler:(void (^)(NSURLRequest*))completionHandler {
    completionHandler(nil);
}
@end

namespace glideslope::platform {

std::string http_client() {
    return "NSURLSession";
}

namespace {

// A GET, or a POST of `body` where there is one.
HttpResponse perform(const HttpRequest& request, const std::string* body) {
    refuse_unsafe_headers(request);
    if (abandoned(request)) {
        throw HttpError(request.url + ": given up before it began");
    }
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
            if (body != nullptr) {
                r.HTTPMethod = @"POST";
                r.HTTPBody = [NSData dataWithBytes:body->data() length:body->size()];
            }
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
                body != nullptr
                    ? [NSURLSession sessionWithConfiguration:configuration
                                                    delegate:[[GlideslopeNoRedirect alloc] init]
                                               delegateQueue:nil]
                    : [NSURLSession sessionWithConfiguration:configuration];

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
            // Waited on a tenth of a second at a time, so that an abandoned
            // request is cancelled: its completion then comes, as an error.
            bool given_up = false;
            while (dispatch_semaphore_wait(
                       done, dispatch_time(DISPATCH_TIME_NOW,
                                           static_cast<std::int64_t>(100 * NSEC_PER_MSEC))) != 0) {
                if (!given_up && abandoned(request)) {
                    given_up = true;
                    [task cancel];
                }
            }
            [session finishTasksAndInvalidate];

            if (given_up) {
                failure = "given up, unfinished";
            } else if (error != nil) {
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
    // NSURLSession undoes the encoding itself, so a content-encoding would
    // name what is already gone and a content-length would count the wire.
    // The names above are already in lower case. See HttpResponse.
    response.headers.erase("content-encoding");
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

#include "world/json.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace glideslope::world {

namespace {

void write_string(const std::string& s, std::string& out) {
    out += '"';
    for (const char c : s) {
        const auto u = static_cast<unsigned char>(c);
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (u < 0x20) {
                char escaped[8];
                std::snprintf(escaped, sizeof escaped, "\\u%04x", static_cast<unsigned>(u));
                out += escaped;
            } else {
                out += c;
            }
        }
    }
    out += '"';
}

void write_value(const Json& v, std::string& out) {
    switch (v.kind()) {
    case Json::Kind::null:
        out += "null";
        break;
    case Json::Kind::boolean:
        out += v.boolean() ? "true" : "false";
        break;
    case Json::Kind::number: {
        const double n = v.number();
        if (!std::isfinite(n)) {
            throw JsonError("JSON cannot hold a number that is not finite");
        }
        char text[32];
        // A whole number as one; any other with the fewest digits that read
        // back to the same double, so that 0.1 is written 0.1.
        if (n == std::floor(n) && std::abs(n) < 9007199254740992.0) {
            std::snprintf(text, sizeof text, "%.0f", n);
        } else {
            for (int digits = 15; digits <= 17; ++digits) {
                std::snprintf(text, sizeof text, "%.*g", digits, n);
                if (std::strtod(text, nullptr) == n) {
                    break;
                }
            }
        }
        out += text;
        break;
    }
    case Json::Kind::string:
        write_string(v.string(), out);
        break;
    case Json::Kind::array: {
        out += '[';
        bool first = true;
        for (const Json& e : v.array()) {
            if (!first) {
                out += ',';
            }
            first = false;
            write_value(e, out);
        }
        out += ']';
        break;
    }
    case Json::Kind::object: {
        out += '{';
        bool first = true;
        for (const auto& [key, e] : v.object()) {
            if (!first) {
                out += ',';
            }
            first = false;
            write_string(key, out);
            out += ':';
            write_value(e, out);
        }
        out += '}';
        break;
    }
    }
}

} // namespace

std::string write_json(const Json& value) {
    std::string out;
    write_value(value, out);
    return out;
}

namespace {

constexpr int max_depth = 128;

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    Json document() {
        skip_whitespace();
        Json value = parse_value(0);
        skip_whitespace();
        if (at_ != text_.size()) {
            fail("more after the value");
        }
        return value;
    }

private:
    [[noreturn]] void fail(const std::string& what) const {
        throw JsonError("JSON at byte " + std::to_string(at_) + ": " + what);
    }

    bool done() const {
        return at_ >= text_.size();
    }
    char peek() const {
        return done() ? '\0' : text_[at_];
    }

    void skip_whitespace() {
        while (!done() &&
               (peek() == ' ' || peek() == '\t' || peek() == '\n' || peek() == '\r')) {
            ++at_;
        }
    }

    void expect(std::string_view word) {
        if (text_.substr(at_, word.size()) != word) {
            fail("expected " + std::string(word));
        }
        at_ += word.size();
    }

    Json parse_value(int depth) {
        if (depth > max_depth) {
            fail("nested deeper than " + std::to_string(max_depth));
        }
        switch (peek()) {
        case '{': return parse_object(depth);
        case '[': return parse_array(depth);
        case '"': return Json::make_string(parse_string());
        case 't': expect("true"); return Json::make_boolean(true);
        case 'f': expect("false"); return Json::make_boolean(false);
        case 'n': expect("null"); return Json::make_null();
        default:
            if (peek() == '-' || (peek() >= '0' && peek() <= '9')) {
                return Json::make_number(parse_number());
            }
            fail(done() ? "the document ends where a value should be" : "not a value");
        }
    }

    Json parse_object(int depth) {
        ++at_; // {
        std::vector<std::pair<std::string, Json>> members;
        skip_whitespace();
        if (peek() == '}') {
            ++at_;
            return Json::make_object(std::move(members));
        }
        for (;;) {
            skip_whitespace();
            if (peek() != '"') {
                fail("an object's key must be a string");
            }
            std::string key = parse_string();
            skip_whitespace();
            if (peek() != ':') {
                fail("expected ':' after a key");
            }
            ++at_;
            skip_whitespace();
            Json value = parse_value(depth + 1);
            bool replaced = false;
            for (auto& [k, v] : members) {
                if (k == key) {
                    v = std::move(value);
                    replaced = true;
                    break;
                }
            }
            if (!replaced) {
                members.emplace_back(std::move(key), std::move(value));
            }
            skip_whitespace();
            if (peek() == ',') {
                ++at_;
                continue;
            }
            if (peek() == '}') {
                ++at_;
                return Json::make_object(std::move(members));
            }
            fail("expected ',' or '}' in an object");
        }
    }

    Json parse_array(int depth) {
        ++at_; // [
        std::vector<Json> items;
        skip_whitespace();
        if (peek() == ']') {
            ++at_;
            return Json::make_array(std::move(items));
        }
        for (;;) {
            skip_whitespace();
            items.push_back(parse_value(depth + 1));
            skip_whitespace();
            if (peek() == ',') {
                ++at_;
                continue;
            }
            if (peek() == ']') {
                ++at_;
                return Json::make_array(std::move(items));
            }
            fail("expected ',' or ']' in an array");
        }
    }

    unsigned hex4() {
        if (text_.size() - at_ < 4) {
            fail("a \\u escape needs four hex digits");
        }
        unsigned v = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = text_[at_++];
            v <<= 4;
            if (c >= '0' && c <= '9') {
                v |= static_cast<unsigned>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                v |= static_cast<unsigned>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                v |= static_cast<unsigned>(c - 'A' + 10);
            } else {
                fail("a \\u escape needs four hex digits");
            }
        }
        return v;
    }

    static void append_utf8(std::string& out, unsigned code) {
        if (code < 0x80) {
            out.push_back(static_cast<char>(code));
        } else if (code < 0x800) {
            out.push_back(static_cast<char>(0xc0 | (code >> 6)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
        } else if (code < 0x10000) {
            out.push_back(static_cast<char>(0xe0 | (code >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
        } else {
            out.push_back(static_cast<char>(0xf0 | (code >> 18)));
            out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3f)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
        }
    }

    std::string parse_string() {
        ++at_; // "
        std::string out;
        for (;;) {
            if (done()) {
                fail("a string is not closed");
            }
            const auto c = static_cast<unsigned char>(text_[at_++]);
            if (c == '"') {
                return out;
            }
            if (c < 0x20) {
                fail("a control character in a string");
            }
            if (c != '\\') {
                out.push_back(static_cast<char>(c));
                continue;
            }
            if (done()) {
                fail("a string ends in an escape");
            }
            const char e = text_[at_++];
            switch (e) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                unsigned code = hex4();
                if (code >= 0xd800 && code <= 0xdbff) {
                    if (text_.substr(at_, 2) != "\\u") {
                        fail("a high surrogate without its low surrogate");
                    }
                    at_ += 2;
                    const unsigned low = hex4();
                    if (low < 0xdc00 || low > 0xdfff) {
                        fail("a high surrogate without its low surrogate");
                    }
                    code = 0x10000 + ((code - 0xd800) << 10) + (low - 0xdc00);
                } else if (code >= 0xdc00 && code <= 0xdfff) {
                    fail("a low surrogate on its own");
                }
                append_utf8(out, code);
                break;
            }
            default: fail(std::string("no escape \\") + e);
            }
        }
    }

    double parse_number() {
        const std::size_t start = at_;
        if (peek() == '-') {
            ++at_;
        }
        if (peek() == '0') {
            ++at_;
            if (peek() >= '0' && peek() <= '9') {
                fail("a number with a leading zero");
            }
        } else if (peek() >= '1' && peek() <= '9') {
            while (peek() >= '0' && peek() <= '9') {
                ++at_;
            }
        } else {
            fail("a number needs a digit");
        }
        if (peek() == '.') {
            ++at_;
            if (!(peek() >= '0' && peek() <= '9')) {
                fail("a fraction needs a digit");
            }
            while (peek() >= '0' && peek() <= '9') {
                ++at_;
            }
        }
        if (peek() == 'e' || peek() == 'E') {
            ++at_;
            if (peek() == '+' || peek() == '-') {
                ++at_;
            }
            if (!(peek() >= '0' && peek() <= '9')) {
                fail("an exponent needs a digit");
            }
            while (peek() >= '0' && peek() <= '9') {
                ++at_;
            }
        }
        // The grammar has been checked, so strtod reads exactly this - and the
        // program never sets a locale, so its decimal point is '.'.
        const std::string digits(text_.substr(start, at_ - start));
        const double v = std::strtod(digits.c_str(), nullptr);
        if (!std::isfinite(v)) {
            fail("a number out of range");
        }
        return v;
    }

    std::string_view text_;
    std::size_t at_ = 0;
};

[[noreturn]] void wrong_kind(const char* wanted) {
    throw JsonError(std::string("a JSON value is not ") + wanted);
}

} // namespace

bool Json::boolean() const {
    if (kind_ != Kind::boolean) {
        wrong_kind("a boolean");
    }
    return boolean_;
}

double Json::number() const {
    if (kind_ != Kind::number) {
        wrong_kind("a number");
    }
    return number_;
}

const std::string& Json::string() const {
    if (kind_ != Kind::string) {
        wrong_kind("a string");
    }
    return string_;
}

const std::vector<Json>& Json::array() const {
    if (kind_ != Kind::array) {
        wrong_kind("an array");
    }
    return array_;
}

const std::vector<std::pair<std::string, Json>>& Json::object() const {
    if (kind_ != Kind::object) {
        wrong_kind("an object");
    }
    return object_;
}

const Json* Json::find(const std::string& key) const {
    for (const auto& [k, v] : object()) {
        if (k == key) {
            return &v;
        }
    }
    return nullptr;
}

const Json& Json::at(const std::string& key) const {
    const Json* v = find(key);
    if (v == nullptr) {
        throw JsonError("a JSON object has no \"" + key + "\"");
    }
    return *v;
}

Json Json::make_null() {
    return {};
}

Json Json::make_boolean(bool v) {
    Json j;
    j.kind_ = Kind::boolean;
    j.boolean_ = v;
    return j;
}

Json Json::make_number(double v) {
    Json j;
    j.kind_ = Kind::number;
    j.number_ = v;
    return j;
}

Json Json::make_string(std::string v) {
    Json j;
    j.kind_ = Kind::string;
    j.string_ = std::move(v);
    return j;
}

Json Json::make_array(std::vector<Json> v) {
    Json j;
    j.kind_ = Kind::array;
    j.array_ = std::move(v);
    return j;
}

Json Json::make_object(std::vector<std::pair<std::string, Json>> v) {
    Json j;
    j.kind_ = Kind::object;
    j.object_ = std::move(v);
    return j;
}

Json parse_json(std::string_view text) {
    return Parser(text).document();
}

} // namespace glideslope::world

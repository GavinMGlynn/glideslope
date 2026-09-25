#pragma once

// JSON (RFC 8259), read into a tree - for the weather services' responses and
// a language model's - and written from one, for what a language model's API
// is asked.
//
// Written here rather than linked, as DEFLATE was: JSON is short and
// completely specified, and what is needed is small documents, not streams. Strict: what RFC 8259 does not allow - trailing
// commas, comments, leading zeros, unescaped control characters, lone
// surrogates - is refused, as is nesting deeper than 128.

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace glideslope::world {

struct JsonError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class Json {
public:
    enum class Kind { null, boolean, number, string, array, object };

    Json() = default;

    Kind kind() const {
        return kind_;
    }
    bool is_null() const {
        return kind_ == Kind::null;
    }

    // Each throws JsonError if the value is another kind.
    bool boolean() const;
    double number() const;
    const std::string& string() const;
    const std::vector<Json>& array() const;
    // Members in the order the document gives them; a key given twice keeps
    // its last value.
    const std::vector<std::pair<std::string, Json>>& object() const;

    // A member of an object, or null if there is none. Throws JsonError if this
    // is not an object.
    const Json* find(const std::string& key) const;
    // A member that must be there.
    const Json& at(const std::string& key) const;

    static Json make_null();
    static Json make_boolean(bool v);
    static Json make_number(double v);
    static Json make_string(std::string v);
    static Json make_array(std::vector<Json> v);
    static Json make_object(std::vector<std::pair<std::string, Json>> v);

private:
    Kind kind_ = Kind::null;
    bool boolean_ = false;
    double number_ = 0.0;
    std::string string_;
    std::vector<Json> array_;
    // Not a map: a standard container of an incomplete type is only promised
    // for vector.
    std::vector<std::pair<std::string, Json>> object_;
};

// **Writes `value` as a document**: compact, members in their order, strings
// escaped as RFC 8259 requires - the quote, the backslash and every control
// character - and otherwise as the UTF-8 they hold. A number is written so
// that it reads back to the same double; one that is not finite, which JSON
// cannot hold, throws JsonError.
std::string write_json(const Json& value);

// Parses a whole document. Throws JsonError, with the byte offset, for anything
// that is not exactly one JSON value, surrounded by whitespace at most.
Json parse_json(std::string_view text);

} // namespace glideslope::world

#include "harness.hpp"

#include "world/json.hpp"

#include <cmath>
#include <limits>
#include <string>

using glideslope::test::check;
using glideslope::test::fail;
using glideslope::world::Json;
using glideslope::world::JsonError;
using glideslope::world::parse_json;

GLIDESLOPE_TEST(json_documents_rfc_8259_allows_are_read_into_their_values) {
    const Json doc = parse_json(R"( {
        "station": "YSSY", "elevation": 6, "report": {"wind": [170, 12, 22.5], "calm": false},
        "missing": null, "empty": {}, "none": [],
        "numbers": [0, -0, 1.5e-5, -2.25E+2, 1e308, 0.1, 123456789012],
        "escapes": "\"\\\/\b\f\n\r\t\u0041\u00e9\u20ac\ud83d\ude00",
        "twice": 1, "twice": 2
    } )");
    check(doc.at("station").string() == "YSSY", "a string");
    check(doc.at("elevation").number() == 6.0, "a whole number");
    const auto& wind = doc.at("report").at("wind").array();
    check(wind.size() == 3 && wind[2].number() == 22.5,
          "an array in an object in an object");
    check(doc.at("report").at("calm").boolean() == false, "false");
    check(doc.at("missing").is_null() && doc.at("empty").object().empty() &&
              doc.at("none").array().empty(),
          "null, and an empty object and array");
    const auto& n = doc.at("numbers").array();
    check(n[0].number() == 0.0 && std::signbit(n[1].number()) &&
              n[2].number() == 1.5e-5 && n[3].number() == -225.0 &&
              n[4].number() == 1e308 && n[5].number() == 0.1 &&
              n[6].number() == 123456789012.0,
          "numbers, with signs, fractions and exponents, exactly as strtod reads them");
    check(doc.at("escapes").string() ==
              std::string("\"\\/\b\f\n\r\tA\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80"),
          "every escape, and \\u to UTF-8 in one, two, three and four bytes");
    check(doc.at("twice").number() == 2.0 && doc.object().size() == 9,
          "a key given twice keeps its last value, once");
    check(doc.find("absent") == nullptr, "a key that is not there is not found");
    check(parse_json(" \t\r\n42 \n").number() == 42.0,
          "a bare value, with whitespace around it");

    std::string deep;
    for (int i = 0; i < 128; ++i) {
        deep += '[';
    }
    for (int i = 0; i < 128; ++i) {
        deep += ']';
    }
    parse_json(deep); // 128 deep is allowed
}

GLIDESLOPE_TEST(json_documents_rfc_8259_does_not_allow_are_refused) {
    const auto refused = [](const std::string& text, const std::string& says,
                            std::source_location where =
                                std::source_location::current()) {
        try {
            parse_json(text);
        } catch (const JsonError& e) {
            if (std::string(e.what()).find(says) == std::string::npos) {
                fail("\"" + text + "\" refused with \"" + e.what() + "\", not \"" +
                         says + "\"",
                     where);
            }
            return;
        }
        fail("\"" + text + "\" was accepted", where);
    };
    refused("", "the document ends where a value should be");
    refused("[1, 2,]", "not a value");
    refused("{\"a\": 1,}", "an object's key must be a string");
    refused("{a: 1}", "an object's key must be a string");
    refused("{\"a\" 1}", "expected ':'");
    refused("[1 2]", "expected ',' or ']'");
    refused("01", "a leading zero");
    refused("1.", "a fraction needs a digit");
    refused("1e", "an exponent needs a digit");
    refused("-", "a number needs a digit");
    refused("1e999", "out of range");
    refused("+1", "not a value");
    refused(".5", "not a value");
    refused("\"tab\there\"", "a control character");
    refused("\"open", "not closed");
    refused("\"\\x\"", "no escape");
    refused("\"\\u12\"", "four hex digits");
    refused("\"\\ud83d\"", "high surrogate without its low");
    refused("\"\\ude00\"", "a low surrogate on its own");
    refused("tru", "expected true");
    refused("nul", "expected null");
    refused("// comment\n1", "not a value");
    refused("1 2", "more after the value");
    refused("{} []", "more after the value");
    std::string deep;
    for (int i = 0; i < 130; ++i) {
        deep += '[';
    }
    refused(deep, "nested deeper than 128");

    try {
        parse_json("[1]").at("key");
        fail("an array was used as an object");
    } catch (const JsonError& e) {
        check(std::string(e.what()).find("not an object") != std::string::npos,
              "asking an array for a key says it is not an object");
    }
}

GLIDESLOPE_TEST(a_json_value_is_written_as_rfc_8259_has_it_and_reads_back_the_same) {

    using glideslope::world::write_json;
    // What a language model's API is asked with, in miniature: every kind of
    // value, nested, and a string with every character that must be escaped.
    std::string awkward = "a \"quoted\" back\\slash, a\nnew line, a\ttab, a\rreturn, ";
    awkward += std::string(1, '\x01');
    awkward += " and \xC3\xA9 as it is";
    const Json value = Json::make_object(
        {{"model", Json::make_string("a model")},
         {"input", Json::make_string(awkward)},
         {"temperature", Json::make_number(0.0)},
         {"figures", Json::make_array({Json::make_number(3000), Json::make_number(-33.8688),
                                       Json::make_number(0.1), Json::make_number(1e300),
                                       Json::make_number(0.1 + 0.2)})},
         {"store", Json::make_boolean(false)},
         {"nothing", Json::make_null()},
         {"empty", Json::make_object({})}});
    const std::string text = write_json(value);
    check(text ==
              "{\"model\":\"a model\",\"input\":\"a \\\"quoted\\\" back\\\\slash, a\\nnew line, "
              "a\\ttab, a\\rreturn, \\u0001 and \xC3\xA9 as it is\",\"temperature\":0,"
              "\"figures\":[3000,-33.8688,0.1,1e+300,0.30000000000000004],"
              "\"store\":false,\"nothing\":null,\"empty\":{}}",
          "written exactly: " + text);
    // And it reads back to the same values.
    const Json back = glideslope::world::parse_json(text);
    check(back.at("input").string() == awkward, "the string reads back the same");
    const auto& figures = back.at("figures").array();
    check(figures.size() == 5 && figures[0].number() == 3000.0 &&
              figures[1].number() == -33.8688 && figures[2].number() == 0.1 &&
              figures[3].number() == 1e300 && figures[4].number() == 0.1 + 0.2,
          "every number reads back to the same double");
    check(write_json(back) == text, "and writes out the same again");

    std::size_t refused = 0;
    for (const double n : {std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::infinity(),
                           -std::numeric_limits<double>::infinity()}) {
        try {
            (void)write_json(Json::make_array({Json::make_number(n)}));
        } catch (const glideslope::world::JsonError&) {
            ++refused;
        }
    }
    check(refused == 3, "a NaN and both infinities refused, which JSON cannot hold");
}

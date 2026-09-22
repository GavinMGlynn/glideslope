#include "harness.hpp"

#include "net/keys.hpp"
#include "platform/store.hpp"

#include <filesystem>
#include <random>
#include <string>
#include <system_error>
#include <vector>

using glideslope::platform::Store;
using glideslope::platform::StoreError;
using glideslope::test::check;

// What a server keeps between runs.
//
// The rule these pin is `REQUIREMENTS.md` 6.6: `--key HEX (server secret,
// minted once and stored if not given)`. "Minted once" is the whole claim,
// and it is only true if what was written comes back after the thing that
// wrote it has gone.

namespace {

// A file of our own, removed when we are done, so that two tests running at
// once do not meet in one store.
class Scratch {
public:
    explicit Scratch(const char* what) {
        static std::random_device entropy;
        path_ = std::filesystem::temp_directory_path() /
                ("glideslope-store-" + std::string(what) + "-" +
                 std::to_string(entropy()) + ".sqlite");
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }
    ~Scratch() {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }
    Scratch(const Scratch&) = delete;
    Scratch& operator=(const Scratch&) = delete;
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

} // namespace

// **A store gives back what was put in it**, and says nothing for a name
// nobody has used.
GLIDESLOPE_TEST(a_store_gives_back_what_was_put_in_it) {
    Scratch scratch("round-trip");
    Store store(scratch.path());
    check(!store.get("nothing-is-here").has_value(),
          "a name nobody has used holds nothing");
    store.set("a-name", "a value");
    const auto back = store.get("a-name");
    check(back.has_value() && *back == "a value", "and a name that was set holds it");

    store.set("a-name", "another value");
    check(*store.get("a-name") == "another value", "setting it again replaces it");
    check(store.get("A-NAME").has_value() == false, "and names are not folded in case");
}

// **What a store keeps outlives the store that kept it.** This is the one
// that matters: a server restarting is a new process opening the same file.
GLIDESLOPE_TEST(what_a_store_keeps_outlives_the_store_that_kept_it) {
    Scratch scratch("outlives");
    {
        Store writing(scratch.path());
        writing.set("a-name", "a value");
    }
    check(std::filesystem::exists(scratch.path()), "the file is there afterwards");
    Store reading(scratch.path());
    const auto back = reading.get("a-name");
    check(back.has_value() && *back == "a value", "and still holds what was put in it");
}

// **A store keeps awkward values unchanged**, because a value is not always
// hexadecimal: a session name is whatever somebody typed.
GLIDESLOPE_TEST(a_store_keeps_awkward_values_unchanged) {
    Scratch scratch("awkward");
    Store store(scratch.path());
    // The whole space here is "what a std::string can hold", which cannot be
    // walked; these are the shapes that break a store written by pasting text
    // into SQL, plus the two that break a length-blind one.
    const std::vector<std::string> values = {
        "",                              // empty
        "a value with spaces",           // whitespace
        "'quoted'; DROP TABLE kept; --", // bound, not pasted
        "a\nnewline",                    // not a line-oriented format
        "\xE2\x9C\x93 a tick in UTF-8",  // not ASCII
        std::string(4096, 'x'),          // longer than any key
    };
    std::size_t checked = 0;
    for (const auto& value : values) {
        store.set("a-name", value);
        const auto back = store.get("a-name");
        check(back.has_value() && *back == value,
              "a value of " + std::to_string(value.size()) + " bytes comes back whole");
        ++checked;
    }
    check(checked == values.size(), "and all six shapes were tried");
    check(checked == 6, "six is what the list above holds");
    // The table survived the one that tried to drop it.
    store.set("after", "still here");
    check(store.get("after").has_value(), "and the table is still there");
}

// **A store that cannot be opened says so** rather than carrying on with
// nowhere to write. A directory is not a database.
GLIDESLOPE_TEST(a_store_that_cannot_be_opened_says_so_rather_than_carrying_on) {
    bool threw = false;
    try {
        Store store(std::filesystem::temp_directory_path());
        store.set("a-name", "a value");
    } catch (const StoreError&) {
        threw = true;
    }
    check(threw, "opening a directory as a store throws");
}

// **A secret kept in a store reads back as the same key**, which is the only
// reason the store exists: the public half a client was given still matches
// after the server has restarted.
GLIDESLOPE_TEST(a_secret_kept_in_a_store_reads_back_as_the_same_key) {
    Scratch scratch("secret");
    const auto minted = glideslope::net::mint_key_pair();
    {
        Store writing(scratch.path());
        writing.set("server-secret", glideslope::net::secret_for_keeping(minted.secret));
    }
    Store reading(scratch.path());
    const auto kept = reading.get("server-secret");
    check(kept.has_value(), "the secret is in the store");
    check(kept->size() == glideslope::net::key_hex_digits,
          "written as 64 hexadecimal digits");
    const auto secret = glideslope::net::secret_from_text(*kept);
    check(secret.has_value() && *secret == minted.secret, "and reads back as itself");
    check(glideslope::net::public_from_secret(*secret) == minted.publik,
          "so the public half a client was given still matches");
}

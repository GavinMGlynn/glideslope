#include "harness.hpp"

#include "frontend/machine_stalls.hpp"

#include <cstddef>
#include <optional>
#include <string>

using glideslope::frontend::MachineStalls;
using glideslope::test::check;

// **A frame is a machine's stall only when the machine took most of it, and
// a quarter of a second or more** (frontend::MachineStalls): every case of
// the two - short or long, the client's own or the machine's - and none
// before the session's clock is known.
GLIDESLOPE_TEST(a_frame_is_a_machines_stall_only_when_the_machine_took_most_of_it_and_a_quarter_second_or_more) {
    struct Case {
        double between_s, own_s;
        bool clock_known, stall;
    };
    const Case cases[] = {
        {0.016, 0.016, true, false}, // an ordinary frame
        {0.2, 0.0, true, false},     // the machine's, but shorter than 0.25 s
        {0.25, 0.0, true, true},     // the machine's, 0.25 s
        {3.5, 0.7, true, true},      // a held frame inside a machine's stall
        {3.5, 3.4, true, false},     // a busy wait of its own
        {0.8, 0.5, true, false},     // the machine's 0.3 s, but not most of it
        {3.5, 0.0, false, false},    // before the session's clock is known
    };
    std::size_t covered = 0;
    for (const Case& c : cases) {
        MachineStalls stalls;
        stalls.frame(10.0, c.between_s, c.own_s,
                     c.clock_known ? std::optional<double>(5.0) : std::nullopt);
        check(stalls.stalls().size() == (c.stall ? 1u : 0u),
              "a frame of " + std::to_string(c.between_s) + " s, " + std::to_string(c.own_s) +
                  " s its own, is " + (c.stall ? "" : "not ") + "a stall");
        ++covered;
    }
    check(covered == 7, "all seven cases covered");
}

// **Each stall keeps its own window**: a second stall inside the first's 60
// words opens its own; a word in both is counted and judged by each, and
// counted apart once; the first's window closes at its own 60th word and the
// second's runs on to its own. A word over a metre is the last over in each
// window it falls in.
GLIDESLOPE_TEST(overlapping_machine_stalls_each_keep_their_own_window_and_a_word_in_both_is_counted_apart_once) {
    MachineStalls stalls;
    // Stall 1: the session at 10 s at the frame after it. Words every 0.04 s.
    stalls.frame(20.0, 3.0, 0.0, 10.0);
    double about = 9.0;
    // Ten words of its catching up.
    for (int i = 0; i < 10; ++i, about += 0.04) {
        check(stalls.word(about), "a word of the catching up is across");
        stalls.compared(0.1);
    }
    about = 10.0;
    // Twenty words after.
    for (int i = 0; i < 20; ++i, about += 0.04) {
        check(stalls.word(about), "a word after the first stall is across");
        stalls.compared(0.1);
    }
    // Stall 2, its session clock where the words now are.
    stalls.frame(25.0, 1.0, 0.0, about);
    // Forty words more: the first's window closes at its 60th word; every one
    // is in the second's.
    for (int i = 0; i < 40; ++i, about += 0.04) {
        check(stalls.word(about), "a word in the second's window is across");
        stalls.compared(i == 5 ? 2.0 : 0.1);
    }
    const auto& s = stalls.stalls();
    check(s.size() == 2, "two stalls");
    check(s[0].words == 60, "the first counted its own 60 words after");
    check(s[1].words == 40, "the second counted the 40 heard since it");
    check(s[0].over_until_word == 26 && s[1].over_until_word == 6,
          "the word over a metre is the 26th after the first and the 6th after the second");
    check(stalls.apart_compared() == 70, "70 words, each counted apart once");
    // Twenty more close the second's window; then a word is not across.
    for (int i = 0; i < 20; ++i, about += 0.04) {
        check(stalls.word(about), "the second's window runs on");
        stalls.compared(0.1);
    }
    check(!stalls.word(about), "a word after both windows is not across");
    check(s[1].words == 60, "the second counted its own 60");
}

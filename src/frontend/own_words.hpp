#pragma once

// **The server's words on a client's own aircraft, every one held against
// where the client had flown it** - for both clients that predict their own:
// the client with the window and glideslope_cli.
//
// A client reads every word that has come before it puts its prediction
// right, and puts it right once, from the newest: the server's newest word
// supersedes the older, and the client is put back to it and flies its
// inputs since again (Gambetta, *Client-Side Prediction and Server
// Reconciliation*; Valve, *Source Multiplayer Networking*). Put right from
// every word in turn, each replaying the inputs since, a client too slow to
// fly them as fast as the words came read them ever later and never caught up
// (docs/status, 2026-10-23).
//
// **Every word read is still a measurement of the prediction**, superseded or
// not: placed on this client's clock by the clocks' difference as it is read
// (sim::Prediction::hear_clock), it is about one of the steps this client
// flies, and is held against where that step left it. Compared from the
// newest alone, a client reading several words a frame measured its
// prediction on fewer words than came - the slower its frames, the fewer.
//
// **A word placed past the steps flown is held until they are**: it is about
// a moment this client's ticks had not reached when it was read - a step or
// two, about one word in twelve with frames held 0.3 s (docs/status,
// 2026-10-23). Put right from as it was read, the aircraft was put at the
// server's word with nothing to fly again, ahead of the steps it had
// counted; compared then, there was no step to hold it against. Held, it is
// compared once its step is flown, and put right from then if it is still
// the newest. A word whose step is never flown - the prediction let go of at
// a switch, or started again - is dropped, and counted by why; so is one held
// longer than `most_held_s`, which a prediction flying on never reaches.

#include "sim/fixed_step.hpp"
#include "sim/prediction.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <utility>

namespace glideslope::frontend {

// The server's step a word is about, from its simulation time.
inline std::uint64_t server_steps_of(double simulation_time_s) {
    return static_cast<std::uint64_t>(
        std::llround(simulation_time_s * static_cast<double>(sim::steps_per_second)));
}

// **Where the client had flown by the step a word is placed at**: the step
// flown from `at_step - 1`, which left the aircraft where the server's word
// is held against it. `Path` is any sequence of the client's own records, each
// with its `step` (sim::Prediction::steps() before it); nothing if the word
// was not placed, or its step is not kept.
template <class Path>
const typename Path::value_type* flown_at(const Path& path, std::optional<std::uint64_t> at_step) {
    if (!at_step || *at_step == 0) {
        return nullptr;
    }
    for (const auto& p : path) {
        if (p.step + 1 == *at_step) {
            return &p;
        }
    }
    return nullptr;
}

// How far apart, metres, where the client had flown and where the server said.
inline double apart_m(const std::array<double, 3>& where, const sim::Motion& server) {
    return std::hypot(where[0] - server.location_ecef_m[0], where[1] - server.location_ecef_m[1],
                      where[2] - server.location_ecef_m[2]);
}

// What holding a word against where the client had flown came to.
enum class Held {
    compared,
    // Its step flown, and no longer kept.
    not_kept,
    // The word not finite, which measures no prediction (sim::first_not_finite).
    not_finite,
};

// **The words on its own aircraft a client reads, each compared at its own
// step, and the one to put it right from.** `Word` is what the client keeps
// of a word to compare it and put itself right from it. Each word is placed
// as it is read (`read`, with what sim::Prediction::hear_clock said), and
// compared at once if its step is flown, or held until it is. Once a pass's
// words are read, `settle` compares what is held and now flown and gives the
// newest word compared - or read before any could be placed - to put the
// prediction right from, once (sim::Prediction::reconcile_placed).
template <class Word>
class OwnWords {
public:
    struct Placed {
        Word word;
        std::optional<std::uint64_t> at_step;
        double read_s = 0.0;
        // Its place among every word read, oldest first.
        std::size_t index = 0;
    };

    // Held longer than this, a word's step is not coming: dropped.
    static constexpr double most_held_s = 1.0;

    // **A word read**, placed at `at_step` (nothing before the clocks'
    // difference can place any), with `flown` steps flown
    // (sim::Prediction::steps). `compare(const Placed&)` holds it against
    // where the client had flown, and says what that came to (`Held`).
    template <class Compare>
    void read(Word word, std::optional<std::uint64_t> at_step, std::uint64_t flown,
              double local_s, Compare&& compare) {
        release(flown, compare);
        if (at_step) {
            last_placed_ = at_step;
        }
        Placed placed{std::move(word), at_step, local_s, read_};
        ++read_;
        last_read_ = placed.index;
        if (!at_step) {
            ++unplaced_;
            // Before the first placed, always: the clocks' difference, once
            // it has a sample, places every word after.
            unplaced_after_placed_ += any_placed_ ? 1 : 0;
            offer(std::move(placed));
        } else if (*at_step <= flown) {
            any_placed_ = true;
            judge(placed, compare);
            offer(std::move(placed));
        } else {
            any_placed_ = true;
            ++held_ever_;
            most_steps_past_ = std::max(most_steps_past_, *at_step - flown);
            held_.push_back(std::move(placed));
        }
    }

    // **The pass's words read**: those held now flown compared, those held
    // too long dropped, and the newest compared - or unplaced - given to put
    // right from, once; nothing if none was.
    template <class Compare>
    std::optional<Placed> settle(std::uint64_t flown, double local_s, Compare&& compare) {
        release(flown, compare);
        for (auto it = held_.begin(); it != held_.end();) {
            if (local_s - it->read_s > most_held_s) {
                it = held_.erase(it);
                ++dropped_["never flown"];
            } else {
                ++it;
            }
        }
        std::optional<Placed> out = std::move(newest_);
        newest_.reset();
        if (out) {
            ++put_right_from_;
            // **Which word it was**, counted, not assumed: the newest read
            // whose step was flown (`ready_`), and the newest read of all
            // unless newer ones are held for their steps.
            if (!ready_ || out->index != *ready_) {
                ++not_newest_;
            } else if (out->index == last_read_) {
                ++newest_read_;
            } else {
                ++newer_held_;
            }
        }
        return out;
    }

    // **The prediction let go of** - at a switch, a take-back, a loss, a
    // start again - and with it every word held for it, counted by `why`;
    // and the word read for it not yet put right from.
    void drop(const std::string& why) {
        // A prediction begun again has no clocks' difference until a word
        // gives it one (sim::Prediction::hear_clock).
        any_placed_ = false;
        if (!held_.empty()) {
            dropped_[why] += held_.size();
            held_.clear();
        }
        if (newest_) {
            ++let_go_;
            newest_.reset();
        }
    }

    std::size_t read() const { return read_; }
    std::size_t compared() const { return compared_; }
    std::size_t unplaced() const { return unplaced_; }
    // The step the newest word placed was placed at, if any has been.
    std::optional<std::uint64_t> last_placed() const { return last_placed_; }
    std::size_t unplaced_after_placed() const { return unplaced_after_placed_; }
    // Placed at a step flown and no longer kept: in nothing else, and a
    // test that counts the words read finds it missing.
    std::size_t not_kept() const { return not_kept_; }
    std::size_t not_finite() const { return not_finite_; }
    std::size_t held_ever() const { return held_ever_; }
    std::uint64_t most_steps_past() const { return most_steps_past_; }
    std::size_t held_now() const { return held_.size(); }
    const std::map<std::string, std::size_t>& dropped() const { return dropped_; }
    std::size_t dropped_all() const {
        std::size_t all = 0;
        for (const auto& [why, n] : dropped_) {
            all += n;
        }
        return all;
    }
    // Compared, and let go of before it could be put right from: counted
    // as compared, and this says how many puttings right were not made.
    std::size_t let_go() const { return let_go_; }
    std::size_t put_right_from() const { return put_right_from_; }
    // Of those: the newest word read in its pass; the newest whose step was
    // flown, newer ones held; and any other, which is a fault.
    std::size_t put_right_from_newest_read() const { return newest_read_; }
    std::size_t put_right_from_newer_held() const { return newer_held_; }
    std::size_t put_right_from_not_newest() const { return not_newest_; }

private:
    template <class Compare>
    void release(std::uint64_t flown, Compare& compare) {
        // **Every word held whose step is now flown**, in the order of their
        // steps - not the order read, nor only those in front: a word placed
        // earlier than one read before it (the clocks' difference moved) is
        // flown first, and compared after the other it found its step
        // forgotten by that one's comparison (Linux debug, under 20 busy
        // cores: 4 in 268, held up to 39 steps).
        for (;;) {
            auto first = held_.end();
            for (auto it = held_.begin(); it != held_.end(); ++it) {
                if (*it->at_step <= flown && (first == held_.end() || *it->at_step < *first->at_step)) {
                    first = it;
                }
            }
            if (first == held_.end()) {
                break;
            }
            Placed placed = std::move(*first);
            held_.erase(first);
            judge(placed, compare);
            offer(std::move(placed));
        }
    }
    // A word ready to put right from - compared, or never placeable - is
    // the one to put right from if it is the newest read of those ready.
    void offer(Placed placed) {
        ready_ = std::max(ready_.value_or(0), placed.index);
        if (!newest_ || placed.index > newest_->index) {
            newest_ = std::move(placed);
        }
    }
    template <class Compare>
    void judge(const Placed& placed, Compare& compare) {
        switch (compare(placed)) {
        case Held::compared:
            ++compared_;
            break;
        case Held::not_kept:
            ++not_kept_;
            break;
        case Held::not_finite:
            ++not_finite_;
            break;
        }
    }

    std::deque<Placed> held_;
    std::optional<Placed> newest_;
    std::size_t read_ = 0;
    std::size_t compared_ = 0;
    std::size_t unplaced_ = 0;
    std::size_t not_kept_ = 0;
    std::size_t not_finite_ = 0;
    std::size_t held_ever_ = 0;
    std::uint64_t most_steps_past_ = 0;
    std::size_t let_go_ = 0;
    std::size_t put_right_from_ = 0;
    std::size_t newest_read_ = 0;
    std::size_t newer_held_ = 0;
    std::size_t not_newest_ = 0;
    std::size_t unplaced_after_placed_ = 0;
    bool any_placed_ = false;
    std::optional<std::uint64_t> last_placed_;
    std::size_t last_read_ = 0;
    std::optional<std::size_t> ready_;
    std::map<std::string, std::size_t> dropped_;
};

// **What a client says of its words**, for a test to read - the same line
// from both clients: every word read is in exactly one of compared, before
// its clocks' difference could place it, not finite, placed at a step no
// longer kept, dropped (by why), or still held at the end.
template <class Word>
std::string said(const OwnWords<Word>& words, std::size_t most_in_a_pass) {
    std::string dropped;
    for (const auto& [why, n] : words.dropped()) {
        dropped += (dropped.empty() ? "" : ", ") + why + " " + std::to_string(n);
    }
    return "own words: " + std::to_string(words.read()) + " read, " +
           std::to_string(words.compared()) + " compared at their own step, " +
           std::to_string(words.held_ever()) + " held until it was flown (at most " +
           std::to_string(words.most_steps_past()) + " steps past); " +
           std::to_string(words.unplaced()) + " before its clocks' difference could place them (" +
           std::to_string(words.unplaced_after_placed()) + " after one was placed), " +
           std::to_string(words.not_finite()) + " not finite, " +
           std::to_string(words.not_kept()) + " at a step no longer kept, " +
           std::to_string(words.dropped_all()) + " dropped (" +
           (dropped.empty() ? std::string("none") : dropped) + "), " +
           std::to_string(words.held_now()) + " still held; " +
           std::to_string(words.put_right_from()) + " put right from (" +
           std::to_string(words.put_right_from_newest_read()) + " the newest read, " +
           std::to_string(words.put_right_from_newer_held()) + " the newest flown with newer held, " +
           std::to_string(words.put_right_from_not_newest()) + " neither), at most " +
           std::to_string(most_in_a_pass) + " read in a pass";
}

} // namespace glideslope::frontend

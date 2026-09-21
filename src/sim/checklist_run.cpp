#include "sim/checklist_run.hpp"

#include <stdexcept>

namespace glideslope::sim {

ChecklistRun::ChecklistRun(AircraftChecklists lists) : lists_(std::move(lists)) {
    progress_.reserve(all_phases().size());
    for (const Phase phase : all_phases()) {
        progress_.emplace_back(lists_.at(phase).items.size());
    }
}

std::size_t ChecklistRun::at(Phase phase) const {
    const auto& phases = all_phases();
    for (std::size_t i = 0; i < phases.size(); ++i) {
        if (phases[i] == phase) {
            return i;
        }
    }
    throw ChecklistError("no phase " + phase_name(phase));
}

void ChecklistRun::show(Phase phase) {
    showing_ = phase;
}

const std::vector<ItemProgress>& ChecklistRun::progress() const {
    return progress_[at(showing_)];
}

void ChecklistRun::update(const Aircraft& aircraft, std::int64_t tick) {
    const Checklist& list = lists_.at(showing_);
    std::vector<ItemProgress>& theirs = progress_[at(showing_)];
    for (std::size_t i = 0; i < list.items.size() && i < theirs.size(); ++i) {
        const ChecklistItem& item = list.items[i];
        if (theirs[i].ticked || item.pilots()) {
            continue;
        }
        double value = 0.0;
        try {
            value = aircraft.property(item.property);
        } catch (const std::out_of_range&) {
            continue; // not this aircraft's to show; a test catches that
        }
        if (item.done(value)) {
            theirs[i].ticked = true;
            theirs[i].at_tick = tick;
        }
    }
}

void ChecklistRun::confirm(std::size_t item, std::int64_t tick) {
    const Checklist& list = lists_.at(showing_);
    std::vector<ItemProgress>& theirs = progress_[at(showing_)];
    if (item >= list.items.size() || item >= theirs.size()) {
        return;
    }
    if (!list.items[item].pilots() || theirs[item].ticked) {
        return;
    }
    theirs[item].ticked = true;
    theirs[item].at_tick = tick;
}

std::vector<std::size_t> ChecklistRun::outstanding() const {
    std::vector<std::size_t> out;
    const std::vector<ItemProgress>& theirs = progress_[at(showing_)];
    for (std::size_t i = 0; i < theirs.size(); ++i) {
        if (!theirs[i].ticked) {
            out.push_back(i);
        }
    }
    return out;
}

} // namespace glideslope::sim

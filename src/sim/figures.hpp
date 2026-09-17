#pragma once

// Published figures: what an aircraft's handbook says it does, and flights that
// check the flight model does it.
//
// An aircraft's figures file (assets/figures/<model>.xml) names each figure, the
// range its measurement must land in, where the number comes from, and the
// conditions it was measured in. Each figure's name picks a flight in
// figures.cpp that flies those conditions with a simple test pilot and measures
// the one number.

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "sim/aircraft.hpp"

namespace glideslope::sim {

struct FigureSpec {
    std::string name;
    std::string unit;
    std::string source;     // the file's words for where the number is from
    double published = 0.0; // the handbook's number, or the middle of its range
    double low = 0.0;       // the measurement must land in [low, high]
    double high = 0.0;
    std::map<std::string, double> conditions; // speeds, flap settings, altitudes
};

struct PublishedFigures {
    std::string model;
    std::string source;
    double total_lbs = 0.0;
    Loading loading;
    std::vector<FigureSpec> figures;
};

struct FigureResult {
    const FigureSpec* spec = nullptr;
    double measured = 0.0;

    bool passed() const {
        return measured >= spec->low && measured <= spec->high;
    }
};

// Reads an aircraft's figures file. Throws std::runtime_error if it cannot be
// read, names a figure no flight exists for, or leaves out a range.
PublishedFigures read_published_figures(const std::filesystem::path& file);

// The name of every figure a flight exists for.
std::vector<std::string> known_figures();

// Flies the flight for `spec` with the aircraft loaded as `figures` says, and
// measures its number. `jsbsim_root` is where the model's files are. Throws
// std::runtime_error if the flight could not be flown as specified - the loading
// does not come to the stated weight, say, or the throttle ran out.
FigureResult fly_figure(const std::filesystem::path& jsbsim_root,
                        const PublishedFigures& figures, const FigureSpec& spec);

} // namespace glideslope::sim

// poisoned_heap - every aircraft, loaded where fresh memory is NaN, reports
// only finite numbers.
//
//   glideslope_poisoned_heap DATA
//
// DATA is the data directory, which holds aircraft/ and jsbsim/.
//
// **Why the heap is poisoned**: JSBSim's FGLGear never sets FCoeff, which it
// reports as gear/unit[N]/side_friction_coeff, until that unit first touches
// the ground; a unit that never does - the A320's nose tip - reports whatever
// the heap gave it. That is usually zero, and once was a NaN: windows-clang,
// CI run 45972866440, "a320 went non-finite at step 1 (1 of 730 numbers),
// first: gear/unit[8]/side_friction_coeff". A test that relies on what the
// heap happens to hold only sometimes tests its rule, so here every
// allocation through operator new is filled with 0xFF bytes, a NaN in every
// double, and anything JSBSim leaves unset reads as one.
//
// Exit status 0 when every aircraft reports only finite numbers, 1 when one
// does not, 2 when it could not be run.

#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <new>
#include <string>
#include <vector>

namespace {

void* poisoned(std::size_t size) noexcept {
    void* p = std::malloc(size == 0 ? 1 : size);
    if (p != nullptr) {
        std::memset(p, 0xFF, size);
    }
    return p;
}

} // namespace

// The replaceable allocation functions, all four plain forms; the aligned
// ones are left as they are, and keep their own deallocation.
void* operator new(std::size_t size) {
    if (void* p = poisoned(size)) {
        return p;
    }
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) {
    if (void* p = poisoned(size)) {
        return p;
    }
    throw std::bad_alloc();
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    return poisoned(size);
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    return poisoned(size);
}
// GCC's release builds warn that free() is handed memory from operator new
// (-Wmismatched-new-delete) once it inlines these: true of any operator new
// replaced over malloc, so the warning is wrong here and is off for these alone.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#endif
void operator delete(void* p) noexcept {
    std::free(p);
}
void operator delete[](void* p) noexcept {
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept {
    std::free(p);
}
void operator delete[](void* p, std::size_t) noexcept {
    std::free(p);
}
void operator delete(void* p, const std::nothrow_t&) noexcept {
    std::free(p);
}
void operator delete[](void* p, const std::nothrow_t&) noexcept {
    std::free(p);
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: glideslope_poisoned_heap DATA\n");
        return 2;
    }
    const std::filesystem::path data = argv[1];
    // **The poison is on**: a fresh double from operator new is a NaN, or
    // this test tests nothing.
    {
        // Straight from operator new, read a byte at a time through
        // volatile: memory nobody wrote, read as a double, is a value the
        // compiler may fold either way. Kept, never deleted: a static that
        // holds it is no leak.
        static void* const probe = ::operator new(4 * sizeof(double));
        const volatile unsigned char* const bytes =
            static_cast<const volatile unsigned char*>(probe);
        std::uint64_t bits = 0;
        for (std::size_t i = 0; i < sizeof bits; ++i) {
            bits = (bits << 8U) | bytes[2 * sizeof(double) + i];
        }
        if (bits != ~std::uint64_t{0}) {
            std::fprintf(stderr, "the heap is not poisoned: a fresh double is %016llx\n",
                         static_cast<unsigned long long>(bits));
            return 2;
        }
    }
    try {
        const auto roster = glideslope::sim::read_catalogue(data);
        std::vector<std::string> wrong;
        std::size_t loaded = 0;
        std::size_t flown = 0;
        // **Every wheel's side friction is walked**: the names the tree
        // holds, against the bogeys her model file declares - 61 in the
        // catalogue's 16 (the A320's 12, the A380's and the S.23's 5, the
        // others' 3).
        constexpr std::size_t catalogue_bogeys = 61;
        std::size_t bogeys_walked = 0;
        for (const glideslope::sim::CatalogueEntry& e : roster) {
            glideslope::sim::Aircraft aircraft(data / "jsbsim", e.model);
            const std::size_t numbers = aircraft.numbers().size();
            ++loaded;
            // In the air, every gear unit clear of the ground, one step on.
            // Not as loaded: JSBSim's orbital/ numbers are first worked out
            // by its first run, which initialize does.
            glideslope::sim::InitialConditions ic;
            ic.latitude_deg = 47.0;
            ic.longitude_deg = 8.0;
            ic.altitude_ft = 5000.0;
            ic.terrain_elevation_ft = 0.0;
            ic.airspeed_kts = 150.0;
            aircraft.initialize(ic);
            aircraft.step();
            for (const std::string& n : aircraft.not_finite_numbers(numbers)) {
                wrong.push_back(e.id + " a step into the air: " + n);
            }
            std::size_t declared = 0;
            {
                std::ifstream in(data / "jsbsim" / "aircraft" / e.model / (e.model + ".xml"));
                const std::string text((std::istreambuf_iterator<char>(in)),
                                       std::istreambuf_iterator<char>());
                for (std::size_t at = text.find("type=\"BOGEY\""); at != std::string::npos;
                     at = text.find("type=\"BOGEY\"", at + 1)) {
                    ++declared;
                }
            }
            const std::string suffix = "/side_friction_coeff";
            std::size_t named = 0;
            for (const std::string& n : aircraft.number_names()) {
                if (n.rfind("gear/unit", 0) == 0 && n.size() > suffix.size() &&
                    n.compare(n.size() - suffix.size(), suffix.size(), suffix) == 0) {
                    ++named;
                }
            }
            if (named != declared || declared == 0) {
                wrong.push_back(e.id + ": " + std::to_string(named) +
                                " side frictions walked, " + std::to_string(declared) +
                                " bogeys declared");
            }
            bogeys_walked += named;
            ++flown;
            std::printf("  %-13s %zu numbers, loaded and a step flown\n", e.id.c_str(),
                        numbers);
        }
        for (const std::string& w : wrong) {
            std::printf("  WRONG: %s\n", w.c_str());
        }
        // Every aircraft in the catalogue, none left out.
        if (roster.empty() || loaded != roster.size() || flown != roster.size()) {
            std::printf("  %zu in the catalogue, %zu loaded, %zu flown\n", roster.size(), loaded,
                        flown);
            return 1;
        }
        std::printf("  %zu of %zu aircraft loaded and flown on a poisoned heap; %zu of %zu "
                    "bogeys' side friction walked\n",
                    flown, roster.size(), bogeys_walked, catalogue_bogeys);
        if (bogeys_walked != catalogue_bogeys) {
            return 1;
        }
        return wrong.empty() ? 0 : 1;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "glideslope_poisoned_heap: %s\n", ex.what());
        return 2;
    }
}

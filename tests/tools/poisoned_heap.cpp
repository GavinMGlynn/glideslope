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

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: glideslope_poisoned_heap DATA\n");
        return 2;
    }
    const std::filesystem::path data = argv[1];
    // **The poison is on**: a fresh double from operator new is a NaN, or
    // this test tests nothing.
    {
        // Kept, never deleted: GCC sees a delete inlined into free here as
        // a mismatch, and a static that holds it is no leak.
        static double* const probe = new double[4];
        std::uint64_t bits = 0;
        std::memcpy(&bits, &probe[2], sizeof bits);
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
        std::printf("  %zu of %zu aircraft loaded and flown on a poisoned heap\n", flown,
                    roster.size());
        return wrong.empty() ? 0 : 1;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "glideslope_poisoned_heap: %s\n", ex.what());
        return 2;
    }
}

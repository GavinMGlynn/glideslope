#include "harness.hpp"

#include "world/metar.hpp"
#include "world/sky.hpp"
#include "world/weather.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using glideslope::test::check;
using glideslope::world::Metar;
using glideslope::world::parse_metar;

namespace {

constexpr double metres_per_foot = 0.3048;

bool near(double a, double b, double tolerance) {
    return std::abs(a - b) <= tolerance;
}

} // namespace

GLIDESLOPE_TEST(a_reports_cloud_is_drawn_as_decks_at_its_heights_and_cover) {
    using glideslope::world::cloud_decks;
    // Albuquerque's thunderstorm, recorded: scattered cumulonimbus at 3,600 ft,
    // broken at 4,600 and overcast at 10,000, over a station 1,631 m up.
    const Metar abq = parse_metar("SPECI KABQ 180759Z COR 18027G37KT 3SM VCTS +RA BR "
                                  "SCT036CB BKN046 OVC100 18/16 "
                                  "A3039");
    const auto decks = cloud_decks(abq, 1631.0);
    check(decks.size() == 3, "three decks");
    check(near(decks[0].base_m, 1631.0 + 3600.0 * metres_per_foot, 1e-9) &&
              near(decks[0].top_m, decks[0].base_m + 6000.0, 1e-9) &&
              decks[0].cumulonimbus && near(decks[0].cover, 3.5 / 8.0, 1e-12),
          "the scattered cumulonimbus: its base the report's above the station, 6 km "
          "deep, 3.5 eighths");
    check(near(decks[1].base_m, 1631.0 + 4600.0 * metres_per_foot, 1e-9) &&
              near(decks[1].top_m - decks[1].base_m, 300.0, 1e-9) &&
              near(decks[1].cover, 6.0 / 8.0, 1e-12),
          "the broken layer: 300 m deep, six eighths");
    check(near(decks[2].cover, 1.0, 0.0), "the overcast: all of the sky");
    check(near(glideslope::world::haze_top_m(decks, 1631.0), 1631.0 + 1000.0, 1e-9),
          "the haze to 1,000 m above the station, under cloud higher than that");

    // Mount Washington in fog, the sky obscured 100 ft up: the haze stops there.
    const Metar mwn =
        parse_metar("METAR KMWN 180751Z 30035G44KT 1/16SM FG VV001 07/07");
    const auto fog = cloud_decks(mwn, 1910.0);
    check(fog.size() == 1 && near(fog[0].cover, 1.0, 0.0) &&
              near(fog[0].base_m, 1910.0 + 30.48, 1e-9) &&
              near(glideslope::world::haze_top_m(fog, 1910.0), 1910.0 + 30.48, 1e-9),
          "an obscured sky is a deck at its vertical visibility, and the haze stops "
          "under it");
    check(
        cloud_decks(parse_metar("METAR UUEE 171600Z 19004MPS CAVOK 18/09 Q1012"), 190.0)
            .empty(),
        "CAVOK has no cloud to draw");
}

GLIDESLOPE_TEST(a_decks_pattern_covers_as_much_of_the_sky_as_its_eighths_say) {
    using glideslope::world::CloudPattern;
    for (const double cover : {1.5 / 8.0, 3.5 / 8.0, 6.0 / 8.0}) {
        const CloudPattern pattern(0xc10d, 0, cover);
        // A grid of its own, finer and offset from the one the pattern's level
        // was found on.
        int cloudy = 0;
        int count = 0;
        bool bounded = true;
        for (double north = -49871.0; north < 50000.0; north += 237.0) {
            for (double east = -49903.0; east < 50000.0; east += 237.0) {
                const double d = pattern.density(east, north);
                bounded = bounded && d >= 0.0 && d <= 1.0;
                cloudy += d > 0.5 ? 1 : 0;
                ++count;
            }
        }
        const double share = static_cast<double>(cloudy) / count;
        check(bounded, "density is between 0 and 1");
        check(near(share, cover, 0.03), "a deck of " + std::to_string(cover * 8.0) +
                                            " eighths is cloud over " +
                                            std::to_string(share * 8.0) + " eighths");
    }
    const CloudPattern one(7, 0, 0.75);
    const CloudPattern again(7, 0, 0.75);
    const CloudPattern other_deck(7, 1, 0.75);
    int same = 0;
    int differs = 0;
    for (int i = 0; i < 1000; ++i) {
        const double e = i * 97.0 - 40000.0;
        const double n = i * 53.0 - 20000.0;
        same += one.density(e, n) == again.density(e, n) ? 1 : 0;
        differs +=
            (one.density(e, n) > 0.5) != (other_deck.density(e, n) > 0.5) ? 1 : 0;
    }
    check(same == 1000, "the same seed and deck draw the same cloud");
    check(differs > 100, "another deck's cloud is elsewhere: " +
                             std::to_string(differs) + " of 1000 places differ");
    check(CloudPattern(7, 0, 1.0).density(123.0, 456.0) == 1.0 &&
              CloudPattern(7, 0, 0.0).density(123.0, 456.0) == 0.0,
          "overcast is cloud everywhere, and no cover none");
}

GLIDESLOPE_TEST(a_reports_visibility_and_precipitation_are_drawn_as_it_gives_them) {
    using glideslope::world::drawn_visibility_m;
    using glideslope::world::Precipitation;
    using glideslope::world::precipitation_of;
    check(drawn_visibility_m(
              parse_metar("METAR EGPH 180820Z 24013G23KT 9999 FEW024 13/10 "
                          "Q1004")) == 40000.0,
          "10 km or more is drawn as 40 km");
    check(drawn_visibility_m(
              parse_metar("METAR ZBAA 171600Z 11003MPS 2800 BR BKN030 23/20 "
                          "Q1018")) == 2800.0,
          "2,800 m is 2,800 m");
    check(near(drawn_visibility_m(
                   parse_metar("METAR KMWN 180751Z 30035G44KT 1/16SM FG VV001 "
                               "07/07")),
               1609.344 / 16.0, 1e-9),
          "a sixteenth of a mile is 100.6 m");
    check(drawn_visibility_m(
              parse_metar("METAR KCQC 180853Z AUTO 26016G29KT SCT017 BKN028 "
                          "OVC110 14/12 A3048")) == 40000.0,
          "no visibility reported is drawn as 40 km");

    const auto abq = precipitation_of(parse_metar(
        "SPECI KABQ 180759Z COR 18027G37KT 3SM VCTS +RA BR SCT036CB 18/16 A3039"));
    check(abq.what == Precipitation::rain && abq.intensity == 1,
          "VCTS +RA BR: heavy rain here, the thunderstorm in the vicinity passed over");
    const auto jac = precipitation_of(parse_metar(
        "METAR KJAC 180856Z AUTO 30006KT 10SM -RA BKN075 OVC095 09/08 A3030"));
    check(jac.what == Precipitation::rain && jac.intensity == -1, "-RA: light rain");
    check(
        precipitation_of(parse_metar("METAR NZCH 171600Z AUTO 34002KT 0300 FZFG VV001 "
                                     "M01/M01 Q1004"))
                .what == Precipitation::none,
        "freezing fog falls as nothing");
    check(precipitation_of(
              parse_metar("METAR XXXX 181200Z 00000KT 2000 SHSN OVC010 M02/M03 "
                          "Q1000"))
                  .what == Precipitation::snow,
          "snow showers: snow");
    check(precipitation_of(
              parse_metar("METAR XXXX 181200Z 00000KT 4000 VCSH BKN020 10/08 "
                          "Q1010"))
                  .what == Precipitation::none,
          "showers in the vicinity fall elsewhere");
}

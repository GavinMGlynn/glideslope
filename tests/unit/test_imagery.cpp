#include "harness.hpp"

#include <CesiumImage/ImageAsset.h>
#include <CesiumImage/ImageManipulation.h>

#include <cstddef>

using glideslope::test::check;

// When an imagery tile cannot be had - a timeout, a server error - Cesium
// Native fills its place with its parent's pixels scaled up, through stb's
// resizer. The stb vcpkg pinned (stb_image_resize2 v2.10) wrote a float past
// its decode buffer scaling three-channel pixels, and read a field of a
// pointer it had just freed; the sanitized client stopped there whenever a
// tile timed out. cmake/ports/stb takes a newer stb, and this is the blit.
GLIDESLOPE_TEST(a_parent_imagery_tile_scaled_into_a_missing_childs_place_is_sound) {
    CesiumImage::ImageAsset parent;
    parent.width = 4;
    parent.height = 4;
    parent.channels = 3;
    parent.bytesPerChannel = 1;
    parent.pixelData.assign(4 * 4 * 3, std::byte{200});

    CesiumImage::ImageAsset child;
    child.width = 8;
    child.height = 8;
    child.channels = 3;
    child.bytesPerChannel = 1;
    child.pixelData.assign(8 * 8 * 3, std::byte{0});

    const bool blitted = CesiumImage::ImageManipulation::blitImage(
        child, {0, 0, 8, 8}, parent, {0, 0, 4, 4});
    check(blitted, "the parent's 4x4 pixels scale into the child's 8x8");
    bool uniform = true;
    for (const std::byte b : child.pixelData) {
        uniform = uniform && b == std::byte{200};
    }
    check(uniform, "a uniform parent scales to the same uniform child");
}

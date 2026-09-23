# vcpkg's own stb port, at a newer commit. The checkout cmake/Vcpkg.cmake pins
# has stb from 2024-07-29, whose stb_image_resize2 (v2.10) writes a float past
# its decode buffer when it scales three-channel pixels and reads a field of a
# pointer it has just freed; v2.11 and v2.12 fixed those. Cesium Native scales
# with it whenever it fills a missing imagery tile from its parent, and the
# sanitized clients on CI stopped there. The rest of the port is vcpkg's.
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO nothings/stb
    REF 2c980bb59875b0d32144a71867fbdebb2f77cd20 # committed on 2026-08-02
    SHA512 93db78589d836b5a8924ed0e980c6fd8d1dbc7e2d0489bc5a0fa4dedd73ed94332e5c814adc2976e89fab3f4cd96b5c46beae7b5dd55bc8374a91390d774b2b0
    HEAD_REF master
)

file(GLOB HEADER_FILES "${SOURCE_PATH}/*.h" "${SOURCE_PATH}/stb_vorbis.c")
file(COPY ${HEADER_FILES} DESTINATION "${CURRENT_PACKAGES_DIR}/include")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/FindStb.cmake" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/vcpkg-cmake-wrapper.cmake" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

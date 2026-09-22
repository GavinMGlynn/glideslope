# A glideslope server in a container.
#
#   docker build -f deploy/Dockerfile -t glideslope-server .
#   docker run --rm -p 47801:47801/udp -v glideslope:/var/lib/glideslope \
#              glideslope-server
#
# **It builds the server and nothing else.** `-DGLIDESLOPE_SERVER_ONLY=ON`
# leaves out SDL, Cesium Native, the shader compiler, the renderer and the
# client, and does not use vcpkg at all: the three libraries the server needs
# - libsodium for the handshake, SQLite for its key store, and libcurl for the
# terrain it downloads - come from the distribution. Building the renderer
# would add most of an hour to this image and nothing in it would ever run a
# renderer.
#
# **Two stages**, so the image that ships carries no compiler and no source.
#
# The build needs the submodules: clone with `--recurse-submodules`, or run
# `git submodule update --init --recursive` before building. `ext/jsbsim` is
# the flight model and the build fails without it.

# --- the build ---------------------------------------------------------------
FROM rockylinux:9 AS build

RUN dnf install -y --setopt=install_weak_deps=False \
        gcc-toolset-13 cmake ninja-build git \
        libsodium-devel sqlite-devel libcurl-devel \
 && dnf clean all

WORKDIR /src
COPY . .

# gcc-toolset puts a newer compiler somewhere the default PATH does not look.
RUN . /opt/rh/gcc-toolset-13/enable \
 && cmake -S . -B /build -G Ninja \
        -DGLIDESLOPE_SERVER_ONLY=ON \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/out \
 && cmake --build /build -j"$(nproc)" \
 && cmake --install /build --component glideslope

# --- what ships --------------------------------------------------------------
FROM rockylinux:9

RUN dnf install -y --setopt=install_weak_deps=False \
        libsodium sqlite-libs libcurl \
 && dnf clean all \
 && useradd --system --home-dir /var/lib/glideslope --create-home glideslope

COPY --from=build /out/ /opt/glideslope/

# **The store lives outside the image.** A server mints its key once and keeps
# it, and a client is given that key out of band - so an image that lost the
# key on every restart would lock out every client it had. Mount a volume here.
VOLUME /var/lib/glideslope
USER glideslope
WORKDIR /opt/glideslope

# UDP, and the port the server takes by default.
EXPOSE 47801/udp

ENTRYPOINT ["/opt/glideslope/glideslope_server"]
CMD ["--headless", "--data", "/opt/glideslope/data", \
     "--store", "/var/lib/glideslope/server.sqlite"]

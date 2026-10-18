#!/usr/bin/env bash
# Rootless Debian 13 cloud setup. Caches stay outside the project checkout.
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
tools=${TRANSLATOR_TOOLS:-/workspace/tooling}
deps=${TRANSLATOR_DEPS:-/workspace/deps}
mkdir -p "$tools" "$deps"
if [[ ! -x "$tools/root/usr/bin/arm-none-eabi-gcc" ]]; then
    [[ -r /usr/share/keyrings/debian-archive-keyring.gpg ]] || {
        echo 'Debian archive keyring required; see docs/development.md for other platforms.' >&2; exit 1;
    }
    mkdir -p "$tools/apt/lists/partial" "$tools/apt/cache/archives/partial" "$tools/apt/empty" "$tools/root"
    cat > "$tools/apt/apt.conf" <<APT
Dir::Etc::Parts "$tools/apt/empty";
Dir::Etc::main "-";
Dir::Etc::sourcelist "$tools/apt/sources.list";
Dir::Etc::sourceparts "-";
Dir::State::lists "$tools/apt/lists";
Dir::Cache "$tools/apt/cache";
APT::Sandbox::User "$(id -un)";
APT
    cat > "$tools/apt/sources.list" <<'APT'
deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://deb.debian.org/debian trixie main
APT
    APT_CONFIG="$tools/apt/apt.conf" /usr/bin/apt-get update
    (
        cd "$tools/apt/cache/archives"
        APT_CONFIG="$tools/apt/apt.conf" /usr/bin/apt-get download \
            gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi libnewlib-dev \
            libstdc++-arm-none-eabi-dev
        for pkg in ./*.deb; do dpkg-deb -x "$pkg" "$tools/root"; done
    )
fi
# Debian normally creates these with update-alternatives during installation.
# Recreate them inside the private extracted toolchain, not system directories.
[[ -e "$tools/root/usr/lib/arm-none-eabi/include" ]] || \
    ln -s ../../include/newlib "$tools/root/usr/lib/arm-none-eabi/include"
[[ -e "$tools/root/usr/lib/arm-none-eabi/lib" ]] || \
    ln -s newlib "$tools/root/usr/lib/arm-none-eabi/lib"
if [[ ! -x "$tools/python/cmake/data/bin/cmake" || ! -x "$tools/python/bin/ninja" ]]; then
    python3 -m pip install --no-cache-dir --target "$tools/python" cmake==3.31.6 ninja==1.11.1.4
fi
checkout() {
    local path=$1 url=$2 tag=$3 commit=$4
    if [[ ! -d "$path" ]]; then git clone --depth 1 --branch "$tag" "$url" "$path"; fi
    [[ $(git -C "$path" rev-parse HEAD) == "$commit" ]] || {
        echo "Unexpected dependency revision at $path; preserve it and choose a different TRANSLATOR_DEPS directory." >&2
        exit 1
    }
    [[ -z $(git -C "$path" status --porcelain --untracked-files=no) ]] || {
        echo "Dependency has local changes: $path; refusing to overwrite them." >&2; exit 1;
    }
}
checkout "$deps/pico-sdk" https://github.com/raspberrypi/pico-sdk.git 2.2.0 a1438dff1d38bd9c65dbd693f0e5db4b9ae91779
# Only the USB submodule is required by this firmware.
git -C "$deps/pico-sdk" submodule update --init lib/tinyusb
checkout "$deps/Pico-PIO-USB" https://github.com/sekigon-gonnoc/Pico-PIO-USB.git 0.7.2 3c1eec341a5232640e4c00628b889b641af34b28
checkout "$deps/picotool/picotool-src" https://github.com/raspberrypi/picotool.git 2.2.0 a7eb3988f0645239185fadb4e25d8279478c2dbb
# Activation belongs in a sourced script, since exports don't survive this process.
cat > "$tools/activate.sh" <<ACTIVATE
export PATH="$tools/root/usr/bin:$tools/python/cmake/data/bin:$tools/python/bin:\$PATH"
export PICO_TOOLCHAIN_PATH="$tools/root/usr"
export PICO_SDK_PATH="$deps/pico-sdk"
export PICO_PIO_USB_PATH="$deps/Pico-PIO-USB"
export PICOTOOL_FETCH_FROM_GIT_PATH="$deps/picotool"
ACTIVATE
source "$tools/activate.sh"
cmake -S "$repo" -B "$repo/build-host" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$repo/build-host" -j4
ctest --test-dir "$repo/build-host" --output-on-failure
cmake -S "$repo" -B "$repo/build-pico" -G Ninja -DBUILD_FIRMWARE=ON \
    -DPICO_BOARD=waveshare_rp2350_usb_a -DPICO_NO_PICOTOOL=0 -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_MAKE_PROGRAM="$tools/python/bin/ninja"
cmake --build "$repo/build-pico" -j4
[[ -s "$repo/build-pico/firmware/videowriter_usb.uf2" ]]
printf 'Setup and build complete. Source %s/activate.sh in new shells.\n' "$tools"

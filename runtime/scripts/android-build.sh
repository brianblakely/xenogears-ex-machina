#!/usr/bin/env bash
# Builds the Android APK (build/android/gradle/app/outputs/apk/debug/xem-debug.apk):
# libSDL3.so from the desktop's SDL release ($XEM_SDL3_SRC) with the NDK's CMake
# toolchain, libmain.so (xem-android) with cargo-ndk, then the APK with Gradle.
#
#   nix develop path:./nix/runtime#android -c runtime/scripts/android-build.sh
#
# XEM_ANDROID_ABIS (default "x86_64 arm64-v8a") picks the ABIs.
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
out=$root/build/android
abis=${XEM_ANDROID_ABIS:-x86_64 arm64-v8a}
api=29
: "${XEM_SDL3_SRC:?run inside nix develop path:./nix/runtime#android}"
: "${ANDROID_NDK_HOME:?run inside nix develop path:./nix/runtime#android}"

# Gradle, the debug keystore and Android tool state stay in ignored directories.
export GRADLE_USER_HOME=$root/.local/android/gradle
export ANDROID_USER_HOME=$root/.local/android/home
export CARGO_BUILD_JOBS=${CARGO_BUILD_JOBS:-6}

targets=()
for abi in $abis; do
    sdl=$out/sdl3/$abi
    cmake -S "$XEM_SDL3_SRC" -B "$sdl" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI="$abi" -DANDROID_PLATFORM="android-$api" \
        -DCMAKE_BUILD_TYPE=Release \
        -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF \
        -DSDL_EXAMPLES=OFF -DSDL_INSTALL=OFF -DSDL_ANDROID_JAR=OFF >/dev/null
    cmake --build "$sdl" --target SDL3-shared
    mkdir -p "$out/sdl3-libs/$abi" "$out/jniLibs/$abi"
    cp "$sdl/libSDL3.so" "$out/sdl3-libs/$abi/"
    cp "$sdl/libSDL3.so" "$out/jniLibs/$abi/"
    targets+=(-t "$abi")
done

(
    cd "$root/runtime"
    XEM_ANDROID_SDL3_LIBS=$out/sdl3-libs \
        cargo ndk "${targets[@]}" -P "$api" -o "$out/jniLibs" build -p xem-android --release
)

# The first run downloads the pinned Android Gradle Plugin into GRADLE_USER_HOME.
gradle -p "$root/runtime/android" --project-cache-dir "$out/gradle-project-cache" assembleDebug
echo "$out/gradle/app/outputs/apk/debug/xem-debug.apk"

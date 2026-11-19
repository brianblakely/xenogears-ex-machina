#!/usr/bin/env bash
# Builds the Android APK (build/android/gradle/app/outputs/apk/debug/xem-debug.apk):
# libSDL3.so from the desktop's SDL release ($XEM_SDL3_SRC) with the NDK's CMake
# toolchain, libmain.so (xem-android) with cargo-ndk, then the APK with Gradle.
#
#   nix develop path:./nix/runtime#android -c src/runtime/scripts/android-build.sh
#
# XEM_ANDROID_ABIS (default "x86_64 arm64-v8a") picks the ABIs.
set -euo pipefail

root=$(cd "$(dirname "$0")/../../.." && pwd)
out=$root/build/android
abis=${XEM_ANDROID_ABIS:-x86_64 arm64-v8a}
api=29
variant=${XEM_ANDROID_VARIANT:-Debug}
case $variant in Debug|Release) ;; *) echo 'XEM_ANDROID_VARIANT must be Debug or Release' >&2; exit 2;; esac
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
    cd "$root/src/runtime"
    XEM_ANDROID_SDL3_LIBS=$out/sdl3-libs \
        cargo ndk "${targets[@]}" -P "$api" -o "$out/jniLibs" build --locked -p xem-android --release
)

# The first run downloads the pinned Android Gradle Plugin and its
# dependencies into GRADLE_USER_HOME, from Google's Maven, Maven Central and
# the plugin portal (every release run: CI keeps no Gradle cache). Those
# repositories can briefly answer "not found" for files they have: the
# release run of 2026-11-19 stopped on Maven Central's
# protobuf-java-util-3.22.3.pom, which it served again minutes later. Gradle
# takes a not-found answer as final, so a run that failed to resolve
# dependencies goes again, asking the repositories afresh, three runs at
# most; any other failure ends the build at once.
log=$out/gradle.log
for attempt in 1 2 3; do
    refresh=()
    if [ "$attempt" -gt 1 ]; then refresh=(--refresh-dependencies); fi
    if gradle -p "$root/src/runtime/android" --project-cache-dir "$out/gradle-project-cache" "${refresh[@]}" "assemble$variant" 2>&1 | tee "$log"; then
        break
    fi
    if [ "$attempt" -eq 3 ] || ! grep -qE 'Could not (resolve|find|GET|HEAD) ' "$log"; then
        exit 1
    fi
    echo "android-build: Gradle could not resolve its dependencies (run $attempt of 3); trying again in $((attempt * 30)) s" >&2
    sleep $((attempt * 30))
done
echo "$out/gradle/app/outputs/apk/${variant,,}/xem-${variant,,}.apk"

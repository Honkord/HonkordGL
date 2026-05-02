# HonkordGL Java/Kotlin Bridge

This workspace provides a reusable Java/Kotlin binding layer that reuses the
native HonkordGL codebase without rewriting the engine per platform.

## Goals

- Keep native engine logic in C/C++ (single source of truth).
- Expose a stable C ABI bridge for JNI and other FFIs.
- Use one Java/Kotlin API across Android, Windows, Linux, and macOS.
- Keep the same bridge reusable by other hosts (game engines, scripting VMs).

## Layout

- `honkord-jvm/` Kotlin/Java desktop JVM API.
- `honkord-android/` Android library module (AAR) with `externalNativeBuild`.
- `native-bridge/` C/C++ bridge (`extern "C"`) over HonkordGL APIs.
- `bindings/spec.json` declarative API manifest for generated bindings.
- `scripts/generate_bindings.py` binding generator (Kotlin + JNI stubs).
- `scripts/package_native.sh` / `scripts/package_native.ps1` packaging tools.

## Architecture

1. Native HonkordGL remains unchanged.
2. `native-bridge` wraps selected APIs into a stable C ABI.
3. JNI exports in the bridge map Java/Kotlin calls to C ABI functions.
4. Java/Kotlin code only talks to JNI symbols, not C++ directly.
5. For Android, build the same bridge with NDK CMake.

## Build (JVM desktop)

From this folder:

```bash
./gradlew :honkord-jvm:build
```

Set a custom native library location at runtime:

```bash
java -Dhonkord.native.path=/abs/path/to/libhonkord_jni_bridge.so -jar app.jar
```

## Build (Android library module)

```bash
./gradlew :honkord-android:assembleRelease
```

The Android module uses:

- `externalNativeBuild.cmake` -> `native-bridge/CMakeLists.txt`
- ABI filters: `arm64-v8a`, `armeabi-v7a`, `x86_64`
- `HONKORDGL_LIB_DIR` hint for prebuilt `libHonkordGL.so`

Expected prebuilt location per ABI:

- `build/android/native-libs/<abi>/libHonkordGL.so`

The produced AAR can be used directly in Android game/app projects.

## Packaging native outputs per ABI/OS

```bash
./scripts/package_native.sh
```

or on PowerShell:

```powershell
./scripts/package_native.ps1
```

Bundles native outputs into:

- `android/dist/native/linux/x86_64/*`
- `android/dist/native/windows/x86_64/*`
- `android/dist/native/macos/universal/*`
- `android/dist/native/android/<abi>/*`

## Generated binding pattern (window/video/sprite)

1. Edit `bindings/spec.json`.
2. Run `python3 scripts/generate_bindings.py`.
3. Generated files appear in:
   - `honkord-jvm/src/main/kotlin/org/honkord/generated/*.kt`
   - `native-bridge/src/generated_bindings.cpp`

This gives a production-friendly pattern for scaling bindings without manual JNI
boilerplate.

## Reuse in other platforms/engines

Because the bridge has a plain C ABI, it can also be consumed by:

- Unity native plugin (C ABI entrypoints)
- Unreal plugins (through C wrapper)
- C#/Rust/Go/Python FFI layers
- iOS/macOS host apps (Objective-C++/Swift + C ABI)
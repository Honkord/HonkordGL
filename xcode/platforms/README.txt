Per-platform project fragments for the same logical Xcode groups (HonkordGL, works, tests).

Apple (Xcode .xcconfig)
  HonkordGL/*.xcconfig   — attach to the HonkordGL static / dynamic target (or merge into target Build Settings → “Based on Configuration File”).
  works/*.xcconfig       — #include the matching HonkordGL file; attach to each example target.
  tests/*.xcconfig       — same for test targets.

  Files: macos, ios, ios-simulator, tvos, watchos. Shared prefix: Apple-common.xcconfig

Non-Apple (*.configuration.txt)
  CMake / Ninja / Visual Studio / pkg-config hints. Replace <REPO> with your HonkordGL clone path.

CMake presets (repo root CMakePresets.json)
  linux-ninja-debug, linux-ninja-release
  windows-mingw64-ninja, windows-mingw32-ninja
  macos-xcode, macos-ninja-debug
  ios-xcode, ios-simulator-xcode
  android-ninja, emscripten-ninja

Committed pkg-config trees: misc/pkgconfig/{linux,macos,android,mingw-x86_64,mingw-i686,emscripten,ios}

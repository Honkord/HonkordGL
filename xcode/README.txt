HonkordGL — cross-platform project configs

CMake presets (all hosts: pick the preset that matches your machine)
  cmake --list-presets
  cmake --preset linux-ninja-debug
  cmake --build --preset linux-ninja-debug

  macOS + Xcode IDE:
    cmake --preset macos-xcode
    cmake --build --preset macos-xcode

Xcode navigator folders (see also cmake/HonkordGL.cmake when using -G Xcode on Apple)
  HonkordGL/   — library sources + misc/pkgconfig/macos/*.pc (reference members)
  works/       — examples + per-example .pc grouping
  tests/       — tests + Test*.pc grouping

Per-platform files (this directory)
  HonkordGL/
    *.xcconfig              — macOS, iOS, iOS Simulator, tvOS, watchOS
    *.configuration.txt     — Linux, Windows (MinGW / MSVC / MinGW-i686), Android, Emscripten, FreeBSD, NetBSD, Raspberry Pi OS
  works/   — same platform names; Apple files #include ../HonkordGL/<platform>.xcconfig
  tests/   — same pattern

  platforms/Apple-common.xcconfig — shared C++ / libc++ settings for Apple SDKs
  platforms/README.txt          — matrix summary

pkg-config paths by platform
  Linux:        misc/pkgconfig/linux
  macOS:        misc/pkgconfig/macos
  Android:      misc/pkgconfig/android
  MinGW x64:    misc/pkgconfig/mingw-x86_64
  MinGW i686:   misc/pkgconfig/mingw-i686
  iOS (stub):   misc/pkgconfig/ios
  Emscripten:   misc/pkgconfig/emscripten

Windows MSVC
  Use visualC/HonkordGL.vcxproj or visualC_gdk/HonkordGL.vcxproj; see HonkordGL/windows-msvc.configuration.txt

#!/usr/bin/env python3
"""Regenerate misc/pkgconfig/*.pc for each toolchain (HonkordGL + demo stubs)."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PKG = ROOT / "misc/pkgconfig"

HONKORDGL_PC = {
    "linux": '''# HonkordGL — pkg-config (Linux GNU)
honkord_prefix=/usr/local
libdir=${honkord_prefix}/lib
includedir=${honkord_prefix}/include

Name: HonkordGL
Description: HonkordGL windowing and rendering library (Linux: X11, Wayland, EGL, GL, ALSA)
Version: 0.1.0

Libs: -L${libdir} -lHonkordGL
Cflags: -I${includedir} -I${honkord_prefix}/third_party/HonkordGL_ImGui -I${honkord_prefix}/third_party/HonkordGL_Nuklear -I${honkord_prefix}/third_party/Nuklear

Libs.private: -lX11 -lXrandr -lXi -lXcursor -lwayland-client -lwayland-egl -lwayland-cursor -lEGL -lGL -lasound -ldl -lpthread -lm
''',
    "mingw-x86_64": '''# HonkordGL — pkg-config (MinGW-w64 x86_64)
honkord_prefix=/usr/local
libdir=${honkord_prefix}/lib
includedir=${honkord_prefix}/include

Name: HonkordGL
Description: HonkordGL windowing and rendering library (Windows via MinGW-w64 x86_64)
Version: 0.1.0

Libs: -L${libdir} -lHonkordGL -luser32 -lgdi32 -lwinmm -lole32 -lws2_32 -lopengl32 -ld3d11 -ldxgi -ld3dcompiler
Cflags: -I${includedir} -I${honkord_prefix}/third_party/HonkordGL_ImGui -I${honkord_prefix}/third_party/HonkordGL_Nuklear -I${honkord_prefix}/third_party/Nuklear
''',
    "mingw-i686": '''# HonkordGL — pkg-config (MinGW-w64 i686)
honkord_prefix=/usr/local
libdir=${honkord_prefix}/lib
includedir=${honkord_prefix}/include

Name: HonkordGL
Description: HonkordGL windowing and rendering library (Windows via MinGW-w64 i686)
Version: 0.1.0

Libs: -L${libdir} -lHonkordGL -luser32 -lgdi32 -lwinmm -lole32 -lws2_32 -lopengl32 -ld3d11 -ldxgi -ld3dcompiler
Cflags: -I${includedir} -I${honkord_prefix}/third_party/HonkordGL_ImGui -I${honkord_prefix}/third_party/HonkordGL_Nuklear -I${honkord_prefix}/third_party/Nuklear
''',
    "android": '''# HonkordGL — pkg-config (Android NDK, prebuilt libHonkordGL.so)
honkord_prefix=/usr/local
abi=arm64-v8a
libdir=${honkord_prefix}/lib/${abi}
includedir=${honkord_prefix}/include

Name: HonkordGL
Description: HonkordGL native library (Android NDK)
Version: 0.1.0

Libs: -L${libdir} -lHonkordGL -llog -landroid
Cflags: -I${includedir}
''',
}

DEMOS = [
    ("MovingSquare", "Moving square example (works/MovingSquare)"),
    ("Tetris", "Tetris example (works/Tetris)"),
    ("Checkerboard", "Checkerboard example (works/Checkerboard)"),
    ("AsteroidGame", "Asteroid game example (works/AsteroidGame)"),
    ("GPURaytracing", "GPU ray tracing example (works/GPURaytracing)"),
    ("SplitScreen", "Split screen example (works/SplitScreen)"),
    ("CameraPlayer", "Camera player example (works/CameraPlayer)"),
    ("ImGuiDemo", "Dear ImGui demo (works/ImGuiDemo)"),
    ("ImGuiSoftwareDemo", "Dear ImGui software demo (works/ImGuiSoftwareDemo)"),
    ("NuklearDemo", "Nuklear demo (works/NuklearDemo)"),
]

TESTS = [
    ("TestWindow", "tests/Window"),
    ("TestSprite", "tests/DrawSprite"),
    ("TestDeferredRenderer", "tests/DeferredRenderer"),
    ("TestAudioSmoke", "tests/Audio"),
]


def main() -> None:
    for tc, body in HONKORDGL_PC.items():
        d = PKG / tc
        d.mkdir(parents=True, exist_ok=True)
        (d / "HonkordGL.pc").write_text(body)

        for name, desc in DEMOS:
            demo_pc = f"""# {name} demo — toolchain {tc}
Name: {name}
Description: {desc}
Version: 0.1.0
Requires: HonkordGL
"""
            (d / f"{name}.pc").write_text(demo_pc)

        for name, rel in TESTS:
            t_pc = f"""# {name} test — toolchain {tc}
Name: {name}
Description: HonkordGL test ({rel})
Version: 0.1.0
Requires: HonkordGL
"""
            (d / f"{name}.pc").write_text(t_pc)

        print(f"updated {d}")


if __name__ == "__main__":
    main()

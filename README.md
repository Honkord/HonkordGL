### Project is still in development

### Honkord's Game Library (Indie Library) (ISO C++ V.: **C++ 17**)
<img src="./Logo.png">

## STATE OF DEVELOPMENT
Version 1 is still in development. At the moment, I am 
looking forward to any minor changes and revisions to 
amend. Stay in touch. :)

## Welcome
Honkord's Game Library is a powerful cross-platform low-level 
multimedia library built for high performance video game 
development. This library is built for interacting 
directly to hardware, therefore rendering performance must be
carefully managed. 

More details are explained inside the docs/ folder, feel free 
to have a look. Any questions, ideas, or concerns, do not 
hesitate to contact me through my gmail 
<samuel.loves.torock@gmail.com> or github 
<https://github.com/Honkord>, I'd very much appreciate. Thank
you. 

## NOTE TO PROGRAMMERS
**ImGui vs Software Renderer**: ImGui HonkordGL rasterizes the 
full UI on the CPU each frame, so it is much slower; it focuses
at correctness and the pure CPU present path, not performance. 

## Honkord's GL API Manual 
# Other Language Bindings (Primarily works with C++)

## Setup

### Prerequisites

- **CMake** 3.20 or newer  
- A **C++17** toolchain (GCC, Clang, MSVC, or MinGW as appropriate)  
- **Python 3** (optional but recommended): enables `scripts/check_public_headers.py` when `HONKORDGL_ENFORCE_PUBLIC_HEADER_BOUNDARY` is left at its default `ON`

Platform libraries depend on which backends you enable. Full matrices and package names are in [`docs/SupportMatrix.md`](docs/SupportMatrix.md) and [`docs/platforms/`](docs/platforms/).

### Configure and build

From the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Common CMake toggles:

| Option | Default | Meaning |
|--------|---------|---------|
| `BUILD_SHARED_LIBS` | `ON` | Shared library (`.so` / `.dylib` / `.dll`) vs static archive |
| `HONKORDGL_BUILD_EXAMPLES` | `ON` | Build examples under `works/` |
| `HONKORDGL_BUILD_TESTS` | `ON` | Build tests under `tests/` |
| `HONKORDGL_INSTALL` | `ON` | Generate install rules and package files |

Linux desktop extras (see `cmake/HonkordGL.cmake` for the full list): `HONKORDGL_ENABLE_X11`, `HONKORDGL_ENABLE_WAYLAND`, `HONKORDGL_ENABLE_VULKAN` (`AUTO` / `ON` / `OFF`), `HONKORDGL_ENABLE_AUDIO`, `HONKORDGL_ENABLE_PIPEWIRE`, etc.

### Install (library + CMake package)

Pick an installation prefix (system paths such as `/usr/local`, or a project-local directory):

```bash
cmake --install build --prefix /path/to/install/prefix
```

This installs:

- **Headers** under `<prefix>/include/HonkordGL/` (including generated `BuildFeatures.h` when applicable)  
- **Libraries** under `<prefix>/lib/` (name: `HonkordGL` / `libHonkordGL` per platform)  
- **CMake config** under `<prefix>/lib/cmake/HonkordGL/` — import as **`HonkordGL::HonkordGL`**  
- **pkg-config** file `<prefix>/lib/pkgconfig/honkordgl.pc`

Shared builds define `HONKORDGL_BUILDING_DLL` while compiling the library; consuming targets should pick up `HONKORDGL_USING_DLL` via the installed config when linking the shared library.

### Use from CMake (`find_package`)

Point CMake at the prefix with `CMAKE_PREFIX_PATH`, then:

```cmake
find_package(HonkordGL CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE HonkordGL::HonkordGL)
```

A minimal smoke project lives at [`tests/cmake_consumer/`](tests/cmake_consumer/).

### Use from pkg-config

After install, ensure `PKG_CONFIG_PATH` includes `<prefix>/lib/pkgconfig` (or use `pkg-config --with-path`):

```bash
export PKG_CONFIG_PATH=/path/to/install/prefix/lib/pkgconfig:$PKG_CONFIG_PATH
pkg-config --cflags --libs honkordgl
```

### Linux (Debian/Ubuntu) dependencies — typical desktop build

Install development packages matching your enabled backends, for example:

```bash
sudo apt-get install build-essential cmake ninja-build python3 \
  libx11-dev libxrandr-dev libxi-dev libxcursor-dev \
  libwayland-dev wayland-protocols \
  libegl1-mesa-dev libgl1-mesa-dev libasound2-dev libvulkan-dev pkg-config
```

PipeWire audio (`HONKORDGL_ENABLE_PIPEWIRE=ON`) additionally requires `libpipewire-0.3-dev` and a working pkg-config entry for `libpipewire-0.3`.

### Public header check

```bash
python3 scripts/check_public_headers.py
```

This runs automatically before building `HonkordGL` when Python is found unless you pass `-DHONKORDGL_ENFORCE_PUBLIC_HEADER_BOUNDARY=OFF`.

---

## Need Guidance?
Navigate to docs/tutorial/ directory where I kept SOME guides 
that mostly explains SOME features of the library. 
Have fun on your video game programming!

**NOTE**: We also have a simplified library 
interface written in various languages. 
It is primarily written in C.

## Example Gallery
Out of game ideas? Take a tour through our example
gallery.

# Classic Asteroid Game 
<img src="./docs/screenshots/asteroids.png">

# Deferred Rendering Loop
<img src="./docs/screenshots/checkerboard.png">

# Test Debugging & Sprite Movement
<img src="./docs/screenshots/moving_square.png">

# 3D Room Raytracing 
<img src="./docs/screenshots/raytracing.png">

# Single Player World
<img src="./docs/screenshots/single_player.png">

# Classic Tetris Game
<img src="./docs/screenshots/tetris.png">

# Two Player Game
<img src="./docs/screenshots/twoplayers.png">

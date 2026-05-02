# Wayland Protocol Configuration

Protocols tracked for HonkordGL include tablet v2, cursor shapes, fractional scale, DRM syncobj, single-pixel buffer, text input, viewporter, xdg-shell and related extensions (see `protocols.json`).

## Files

- `protocols.json` — registry with paths under **`xml/`** (vendored upstream definitions).
- `xml/` — snapshot of **wayland-protocols** (same layout as `/usr/share/wayland-protocols`).
- `generate_protocols.py` — runs **wayland-scanner**; supports `sync` to refresh **`xml/`**.
- `CMakeLists.wayland-protocols.cmake` — shim that includes **`cmake/WaylandProtocols.cmake`**.

## Refresh vendored XML

```bash
python3 wayland/generate_protocols.py sync
python3 wayland/generate_protocols.py sync /path/to/wayland-protocols
```

## Generate headers/sources

```bash
python3 wayland/generate_protocols.py
```

Output: `src/Wayland/generated/*.h` and `*.c`.

## Notes

- CMake uses **`wayland/xml`** so builds do not rely on `/usr/share` at configure time.
- **`python3 wayland/generate_protocols.py sync`** repopulates **`xml/`** when distros ship newer packages.

# Wayland Protocol Configuration

This folder defines the protocol generation configuration requested for:

- cursor shapes
- fractional scales
- color modifiers
- color management
- frog color management
- single pixel framebuffer
- text inputs
- viewporter
- xdg family:
  - activation
  - decoration
  - dialog
  - foreign unstable
  - shell
  - icons

## Files

- `protocols.json` protocol registry and output naming.
- `generate_protocols.py` scanner runner.
- `CMakeLists.wayland-protocols.cmake` CMake integration snippet.

## Generate now

```bash
python3 wayland/generate_protocols.py
```

Generated files go to:

- `src/Wayland/generated/*.h`
- `src/Wayland/generated/*.c`

## Notes

- Protocol XMLs are expected from `wayland-protocols` package paths under
  `/usr/share/wayland-protocols`.
- Missing XML files are skipped by the Python generator, allowing partial
  protocol availability across distros/toolchains.

# Visual Studio GDK Projects

This folder contains MSBuild project files for the Windows library and all example/test apps.

## Projects

- `HonkordGL.vcxproj` (DLL)
- `MovingSquare.vcxproj`
- `Tetris.vcxproj`
- `Checkerboard.vcxproj`
- `AsteroidGame.vcxproj`
- `GPURaytracing.vcxproj`
- `SplitScreen.vcxproj`
- `CameraPlayer.vcxproj`
- `TestWindow.vcxproj`
- `TestSprite.vcxproj`
- `TestDeferredRenderer.vcxproj`
- `TestAudioSmoke.vcxproj`

## Shared configuration

- `HonkordGL.GDK.Shared.props`
  - C++17
  - Include paths (`include`, `src`, `src/Wayland`)
  - Windows defs (`NOMINMAX`, `UNICODE`, `_UNICODE`)
  - System libs (`user32`, `gdi32`, `winmm`, `ole32`, `ws2_32`, `opengl32`, `d3d11`, `dxgi`, `d3dcompiler`)

## Build (Developer Command Prompt)

```bat
cd visualC_gdk
msbuild HonkordGL.vcxproj /p:Configuration=Release /p:Platform=x64
msbuild MovingSquare.vcxproj /p:Configuration=Release /p:Platform=x64
```

All app projects reference `HonkordGL.vcxproj` and copy `HonkordGL.dll` into their output folder after build.

## GDK Targeting

- Uses `HonkordGL.GDK.Shared.props` for GDK-ready include/lib settings.
- Adds `HONKORDGL_GDK` preprocessor define.
- Adds GameDK include hints via `$(GameDKLatest)`.
- Adds GDK libs: `xgameruntime`, `GameInput`, `xaudio2_9redist`.
- Open `HonkordGL.GDK.sln` in Visual Studio with Microsoft GDK installed.

> Note: Exact Xbox platform configurations depend on installed GDK + VS workloads; this scaffold keeps Win32/x64 project configs and adds GDK link/include settings for Windows/Xbox game-dev workflows.
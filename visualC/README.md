# Visual Studio Projects

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

- `HonkordGL.Shared.props`
  - C++17
  - Include paths (`include`, `src`, `src/Wayland`)
  - Windows defs (`NOMINMAX`, `UNICODE`, `_UNICODE`)
  - System libs (`user32`, `gdi32`, `winmm`, `ole32`, `ws2_32`, `opengl32`, `d3d11`, `dxgi`, `d3dcompiler`)

## Build (Developer Command Prompt)

```bat
cd visualC
msbuild HonkordGL.vcxproj /p:Configuration=Release /p:Platform=x64
msbuild MovingSquare.vcxproj /p:Configuration=Release /p:Platform=x64
```

All app projects reference `HonkordGL.vcxproj` and copy `HonkordGL.dll` into their output folder after build.
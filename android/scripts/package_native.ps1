$ErrorActionPreference = "Stop"

$RootDir = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$OutDir = Join-Path $RootDir "android\dist\native"

if (Test-Path $OutDir) { Remove-Item $OutDir -Recurse -Force }
New-Item -ItemType Directory -Path $OutDir | Out-Null

function Copy-IfExists {
    param([string]$Src, [string]$Dst)
    if (Test-Path $Src) {
        New-Item -ItemType Directory -Path ([System.IO.Path]::GetDirectoryName($Dst)) -Force | Out-Null
        Copy-Item -Path $Src -Destination $Dst -Force
        Write-Host "packaged: $Dst"
    }
}

# Linux
Copy-IfExists "$RootDir\build\linux\shared\libHonkordGL.so" "$OutDir\linux\x86_64\libHonkordGL.so"
Copy-IfExists "$RootDir\build\linux\shared\libhonkord_jni_bridge.so" "$OutDir\linux\x86_64\libhonkord_jni_bridge.so"

# Windows (MinGW artifacts)
Copy-IfExists "$RootDir\build\windows\shared\HonkordGL.dll" "$OutDir\windows\x86_64\HonkordGL.dll"
Copy-IfExists "$RootDir\build\windows\shared\libhonkord_jni_bridge.dll" "$OutDir\windows\x86_64\honkord_jni_bridge.dll"

# macOS
Copy-IfExists "$RootDir\build\macos\shared\libHonkordGL.dylib" "$OutDir\macos\universal\libHonkordGL.dylib"
Copy-IfExists "$RootDir\build\macos\shared\libhonkord_jni_bridge.dylib" "$OutDir\macos\universal\libhonkord_jni_bridge.dylib"

# Android ABIs
$abis = @("arm64-v8a", "armeabi-v7a", "x86_64")
foreach ($abi in $abis) {
    Copy-IfExists "$RootDir\build\android\native-libs\$abi\libHonkordGL.so" "$OutDir\android\$abi\libHonkordGL.so"
    Copy-IfExists "$RootDir\build\android\native-libs\$abi\libhonkord_jni_bridge.so" "$OutDir\android\$abi\libhonkord_jni_bridge.so"
}

Write-Host "Done. Native bundle layout: $OutDir"

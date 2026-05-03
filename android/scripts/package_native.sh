#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT_DIR="${ROOT_DIR}/android/dist/native"

rm -rf "${OUT_DIR}"
mkdir -p "${OUT_DIR}"

copy_if_exists() {
  local src="$1"
  local dst="$2"
  if [[ -f "${src}" ]]; then
    mkdir -p "$(dirname "${dst}")"
    cp -f "${src}" "${dst}"
    echo "packaged: ${dst}"
  fi
}

# Linux
copy_if_exists "${ROOT_DIR}/build/linux/shared/libHonkordGL.so" \
  "${OUT_DIR}/linux/x86_64/libHonkordGL.so"
copy_if_exists "${ROOT_DIR}/build/linux/shared/libhonkord_jni_bridge.so" \
  "${OUT_DIR}/linux/x86_64/libhonkord_jni_bridge.so"

# Windows (MinGW artifacts)
copy_if_exists "${ROOT_DIR}/build/windows/shared/HonkordGL.dll" \
  "${OUT_DIR}/windows/x86_64/HonkordGL.dll"
copy_if_exists "${ROOT_DIR}/build/windows/shared/libhonkord_jni_bridge.dll" \
  "${OUT_DIR}/windows/x86_64/honkord_jni_bridge.dll"

# macOS (if built by host pipeline)
copy_if_exists "${ROOT_DIR}/build/macos/shared/libHonkordGL.dylib" \
  "${OUT_DIR}/macos/universal/libHonkordGL.dylib"
copy_if_exists "${ROOT_DIR}/build/macos/shared/libhonkord_jni_bridge.dylib" \
  "${OUT_DIR}/macos/universal/libhonkord_jni_bridge.dylib"

# Android AAR/JNI style layout
for abi in arm64-v8a armeabi-v7a x86_64; do
  copy_if_exists "${ROOT_DIR}/build/android/native-libs/${abi}/libHonkordGL.so" \
    "${OUT_DIR}/android/${abi}/libHonkordGL.so"
  copy_if_exists "${ROOT_DIR}/build/android/native-libs/${abi}/libhonkord_jni_bridge.so" \
    "${OUT_DIR}/android/${abi}/libhonkord_jni_bridge.so"
done

echo "Done. Native bundle layout: ${OUT_DIR}"

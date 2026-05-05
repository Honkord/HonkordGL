/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — Raw Linux evdev access (`/dev/input/event*`, ioctl, `input_event`)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_LINUXEVDEVINTEGRATION_H
#define HONKORDGL_LINUXEVDEVINTEGRATION_H

#include <HonkordGL/Config.h>

#include <cstddef>
#include <cstdint>

namespace HonkordGL::Graphics {

enum class LinuxEvdevIntegrationResult : int {
    OK = 0,
    INVALID_ARGUMENT = -1,
    UNSUPPORTED_PLATFORM = -2,
    OPEN_FAILED = -3,
    IOCTL_FAILED = -4,
};

/** Iterate `/dev/input/event*` paths (best-effort glob). */
using LinuxEvdevPathCallback = void (*)(void * user, char const * path);

HONKORDGL_API void LinuxEvdevForEachEventDevicePath(LinuxEvdevPathCallback callback, void * user) noexcept;

/** Open with `O_RDONLY | O_NONBLOCK` (or closest available). @return fd or -1. */
HONKORDGL_API HONKORDGL_ND int LinuxEvdevOpenReadOnly(char const * path) noexcept;

HONKORDGL_API void LinuxEvdevClose(int fd) noexcept;

/**
 * Raw `ioctl(fd, request, argp)` for power users (EVIOC*, etc.). `request` is the platform ioctl request code.
 * @return 0 on success, negative errno on failure.
 */
HONKORDGL_API HONKORDGL_ND int LinuxEvdevIoctl(int fd, unsigned long request, void * argp) noexcept;

/** EVIOCGNAME into caller buffer (includes terminating NUL when space allows). */
HONKORDGL_API HONKORDGL_ND int LinuxEvdevGetName(int fd, char * buf, std::size_t buf_bytes) noexcept;

/** Reads one `struct input_event` (Linux `<linux/input.h>`) into `out_bytes` bytes; fails if buffer too small. */
HONKORDGL_API HONKORDGL_ND int LinuxEvdevReadInputEvent(int fd, void * out_bytes, std::size_t out_bytes_size) noexcept;

} // namespace HonkordGL::Graphics

#endif

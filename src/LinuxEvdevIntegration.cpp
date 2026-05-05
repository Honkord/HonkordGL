/**
 * HonkordGL — Linux evdev raw access
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/LinuxEvdevIntegration.h>
#include <HonkordGL/PlatformAdapter.h>

#if defined(__linux__)

#include <fcntl.h>
#include <glob.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace HonkordGL::Graphics {

void LinuxEvdevForEachEventDevicePath(LinuxEvdevPathCallback callback, void * user) noexcept
{
    if (!callback)
        return;
    glob_t g{};
    if (glob("/dev/input/event*", 0, nullptr, &g) != 0)
        return;
    for (std::size_t i = 0; i < g.gl_pathc; ++i)
        callback(user, g.gl_pathv[i]);
    globfree(&g);
}

int LinuxEvdevOpenReadOnly(char const * path) noexcept
{
    if (!path)
        return -1;
    int const fd = ::open(path, O_RDONLY | O_NONBLOCK);
    return fd;
}

void LinuxEvdevClose(int fd) noexcept
{
    if (fd >= 0)
        ::close(fd);
}

int LinuxEvdevIoctl(int fd, unsigned long request, void * argp) noexcept
{
    if (fd < 0)
        return -static_cast<int>(EINVAL);
    int const r = ::ioctl(fd, request, argp);
    return r < 0 ? -errno : 0;
}

int LinuxEvdevGetName(int fd, char * buf, std::size_t buf_bytes) noexcept
{
    if (fd < 0 || !buf || buf_bytes == 0)
        return static_cast<int>(LinuxEvdevIntegrationResult::INVALID_ARGUMENT);
    char tmp[256]{};
    int const ir = ::ioctl(fd, EVIOCGNAME(sizeof(tmp)), tmp);
    if (ir < 0)
        return static_cast<int>(LinuxEvdevIntegrationResult::IOCTL_FAILED);
    std::size_t const n = static_cast<std::size_t>(ir < static_cast<int>(sizeof(tmp)) ? ir : static_cast<int>(sizeof(tmp)) - 1);
    std::size_t const copy = (n + 1 < buf_bytes) ? (n + 1) : buf_bytes;
    std::memcpy(buf, tmp, copy - 1);
    buf[copy - 1] = '\0';
    return static_cast<int>(LinuxEvdevIntegrationResult::OK);
}

int LinuxEvdevReadInputEvent(int fd, void * out_bytes, std::size_t out_bytes_size) noexcept
{
    if (fd < 0 || !out_bytes || out_bytes_size < sizeof(input_event))
        return static_cast<int>(LinuxEvdevIntegrationResult::INVALID_ARGUMENT);
    ssize_t const r = ::read(fd, out_bytes, sizeof(input_event));
    if (r < 0)
        return -errno;
    if (r != static_cast<ssize_t>(sizeof(input_event)))
        return static_cast<int>(LinuxEvdevIntegrationResult::INVALID_ARGUMENT);
    return static_cast<int>(LinuxEvdevIntegrationResult::OK);
}

} // namespace HonkordGL::Graphics

#else

namespace HonkordGL::Graphics {

void LinuxEvdevForEachEventDevicePath(LinuxEvdevPathCallback, void *) noexcept {}

int LinuxEvdevOpenReadOnly(char const *) noexcept
{
    return -1;
}

void LinuxEvdevClose(int) noexcept {}

int LinuxEvdevIoctl(int, unsigned long, void *) noexcept
{
    return static_cast<int>(LinuxEvdevIntegrationResult::UNSUPPORTED_PLATFORM);
}

int LinuxEvdevGetName(int, char *, std::size_t) noexcept
{
    return static_cast<int>(LinuxEvdevIntegrationResult::UNSUPPORTED_PLATFORM);
}

int LinuxEvdevReadInputEvent(int, void *, std::size_t) noexcept
{
    return static_cast<int>(LinuxEvdevIntegrationResult::UNSUPPORTED_PLATFORM);
}

} // namespace HonkordGL::Graphics

#endif

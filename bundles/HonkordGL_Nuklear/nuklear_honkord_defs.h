/**
 * Shared Nuklear compile flags for every translation unit that includes `nuklear.h`
 * (implementation TU + backends + demos). Must stay consistent across TUs.
 */
#pragma once

#include <stdarg.h>
#include <stdio.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT

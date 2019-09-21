#pragma once

#include <vulkan/vulkan.h>

#include <cstdio>
#include <cstdlib>

#include <linalg.h>

static void moVkCheckResult(VkResult err)
{
    if (err == 0) return;
    printf("[vulkan] Result: %d\n", err);
    if (err < 0)
        abort();
}

static VKAPI_ATTR VkBool32 VKAPI_CALL moVkDebugReport(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT,
    uint64_t,
    size_t,
    int32_t messageCode,
    const char* pLayerPrefix,
    const char* pMessage,
    void*)
{
    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
    {
        fprintf(stderr, "ERROR: [%s] Code %d : %s\n", pLayerPrefix, messageCode, pMessage);
    }
    if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
    {
        fprintf(stdout, "WARNING: [%s] Code %d : %s\n", pLayerPrefix, messageCode, pMessage);
    }
    if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
    {
        fprintf(stdout, "PERFORMANCE: [%s] Code %d : %s\n", pLayerPrefix, messageCode, pMessage);
    }
    if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
    {
        fprintf(stdout, "INFO: [%s] Code %d : %s\n", pLayerPrefix, messageCode, pMessage);
    }
    if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
    {
        fprintf(stdout, "DEBUG: [%s] Code %d : %s\n", pLayerPrefix, messageCode, pMessage);
    }
    fflush(flags & VK_DEBUG_REPORT_ERROR_BIT_EXT ? stderr : stdout);
    return VK_FALSE;
}

#define MoPI (355.f/113)
static constexpr float moDegreesToRadians(float angle)
{
    return angle * MoPI / 180.0f;
}

static linalg::aliases::float4x4 correction_matrix =
    { { 1.0f, 0.0f, 0.0f, 0.0f },
      { 0.0f,-1.0f, 0.0f, 0.0f },
      { 0.0f, 0.0f, 1.0f, 0.0f },
      { 0.0f, 0.0f, 0.0f, 1.0f } };
static linalg::aliases::float4x4 projection_matrix =
    linalg::mul(correction_matrix, linalg::perspective_matrix(moDegreesToRadians(75.f), 16/9.f, 0.1f, 1000.f, linalg::pos_z, linalg::zero_to_one));

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2018 Patrick Pelletier
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/

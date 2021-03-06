#include "mo_material_utils.h"

void moCreateDefaultMaterial(MoCommandBuffer commandBuffer, MoMaterial *pMaterial)
{
    MoMaterialCreateInfo info = {};
    info.colorAmbient = { 0.1f, 0.1f, 0.1f, 1.0f };
    info.colorDiffuse = { 0.64f, 0.64f, 0.64f, 1.0f };
    info.colorSpecular = { 0.5f, 0.5f, 0.5f, 1.0f };
    info.colorEmissive = { 0.0f, 0.0f, 0.0f, 1.0f };
    info.commandBuffer = commandBuffer.buffer;
    info.commandPool = commandBuffer.pool;
    moCreateMaterial(&info, pMaterial);
}

void moCreateDemoMaterial(MoCommandBuffer commandBuffer, MoMaterial *pMaterial)
{
    const uint32_t diffuse[8*8] = {0xff1a07e3,0xff48f4fb,0xff66b21d,0xfff9fb00,0xffa91f6c,0xffb98ef1,0xffb07279,0xff6091f7,
                                   0xff6091f7,0xff1a07e3,0xff48f4fb,0xff66b21d,0xfff9fb00,0xffa91f6c,0xffb98ef1,0xffb07279,
                                   0xffb07279,0xff6091f7,0xff1a07e3,0xff48f4fb,0xff66b21d,0xfff9fb00,0xffa91f6c,0xffb98ef1,
                                   0xffb98ef1,0xffb07279,0xff6091f7,0xff1a07e3,0xff48f4fb,0xff66b21d,0xfff9fb00,0xffa91f6c,
                                   0xffa91f6c,0xffb98ef1,0xffb07279,0xff6091f7,0xff1a07e3,0xff48f4fb,0xff66b21d,0xfff9fb00,
                                   0xfff9fb00,0xffa91f6c,0xffb98ef1,0xffb07279,0xff6091f7,0xff1a07e3,0xff48f4fb,0xff66b21d,
                                   0xff66b21d,0xfff9fb00,0xffa91f6c,0xffb98ef1,0xffb07279,0xff6091f7,0xff1a07e3,0xff48f4fb,
                                   0xff48f4fb,0xff66b21d,0xfff9fb00,0xffa91f6c,0xffb98ef1,0xffb07279,0xff6091f7,0xff1a07e3};

    MoMaterialCreateInfo info = {};
    info.colorAmbient = { 0.2f, 0.2f, 0.2f, 1.0f };
    info.colorDiffuse = { 0.64f, 0.64f, 0.64f, 1.0f };
    info.colorSpecular = { 0.5f, 0.5f, 0.5f, 1.0f };
    info.colorEmissive = { 0.0f, 0.0f, 0.0f, 1.0f };
    info.textureDiffuse.pData = (uint8_t*)diffuse;
    info.textureDiffuse.extent = { 8, 8 };
    info.commandBuffer = commandBuffer.buffer;
    info.commandPool = commandBuffer.pool;
    moCreateMaterial(&info, pMaterial);
}

void moCreateGridMaterial(MoCommandBuffer commandBuffer, MoMaterial *pMaterial)
{
    const uint32_t diffuse[8*8] = {0xffffffff,0xffffffff,0xffffffff,0xffffffff,0xffffffff,0xffffffff,0xffffffff,0xffffffff,
                                   0xffffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,
                                   0xffffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,
                                   0xffffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,
                                   0xffffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,
                                   0xffffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,
                                   0xffffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,
                                   0xffffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff};

    MoMaterialCreateInfo info = {};
    info.colorAmbient = { 0.2f, 0.2f, 0.2f, 1.0f };
    info.colorDiffuse = { 0.64f, 0.64f, 0.64f, 1.0f };
    info.colorSpecular = { 0.5f, 0.5f, 0.5f, 1.0f };
    info.colorEmissive = { 0.0f, 0.0f, 0.0f, 1.0f };
    info.textureDiffuse.pData = (uint8_t*)diffuse;
    info.textureDiffuse.extent = { 8, 8 };
    info.commandBuffer = commandBuffer.buffer;
    info.commandPool = commandBuffer.pool;
    moCreateMaterial(&info, pMaterial);
}

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

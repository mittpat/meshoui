#include "mo_pipeline_utils.h"

#include <fstream>
#include <vector>

void moCreatePhongPipeline(MoPipeline *pPipeline)
{
    MoPipelineCreateInfo info = {};
    info.flags = MO_PIPELINE_FEATURE_DEFAULT;
    std::vector<char> mo_phong_shader_vert_spv;
    {
        std::ifstream fileStream("phong.vert.spv", std::ifstream::binary);
        mo_phong_shader_vert_spv = std::vector<char>((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
    }
    std::vector<char> mo_phong_shader_frag_spv;
    {
        std::ifstream fileStream("phong.frag.spv", std::ifstream::binary);
        mo_phong_shader_frag_spv = std::vector<char>((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
    }
    info.pVertexShader = (std::uint32_t*)mo_phong_shader_vert_spv.data();
    info.vertexShaderSize = mo_phong_shader_vert_spv.size();
    info.pFragmentShader = (std::uint32_t*)mo_phong_shader_frag_spv.data();
    info.fragmentShaderSize = mo_phong_shader_frag_spv.size();
    moCreatePipeline(&info, pPipeline);
}

void moCreateDomePipeline(MoPipeline *pPipeline)
{
    MoPipelineCreateInfo info = {};
    std::vector<char> mo_dome_shader_vert_spv;
    {
        std::ifstream fileStream("dome.vert.spv", std::ifstream::binary);
        mo_dome_shader_vert_spv = std::vector<char>((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
    }
    std::vector<char> mo_dome_shader_frag_spv;
    {
        std::ifstream fileStream("dome.frag.spv", std::ifstream::binary);
        mo_dome_shader_frag_spv = std::vector<char>((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
    }
    info.pVertexShader = (std::uint32_t*)mo_dome_shader_vert_spv.data();
    info.vertexShaderSize = mo_dome_shader_vert_spv.size();
    info.pFragmentShader = (std::uint32_t*)mo_dome_shader_frag_spv.data();
    info.fragmentShaderSize = mo_dome_shader_frag_spv.size();
    info.flags = MO_PIPELINE_FEATURE_NONE;
    moCreatePipeline(&info, pPipeline);
}

void moCreatePassthroughPipeline(MoPipeline *pPipeline)
{
    MoPipelineCreateInfo info = {};
    std::vector<char> mo_passthrough_shader_vert_spv;
    {
        std::ifstream fileStream("passthrough.vert.spv", std::ifstream::binary);
        mo_passthrough_shader_vert_spv = std::vector<char>((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
    }
    std::vector<char> mo_passthrough_shader_frag_spv;
    {
        std::ifstream fileStream("passthrough.frag.spv", std::ifstream::binary);
        mo_passthrough_shader_frag_spv = std::vector<char>((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
    }
    info.pVertexShader = (std::uint32_t*)mo_passthrough_shader_vert_spv.data();
    info.vertexShaderSize = mo_passthrough_shader_vert_spv.size();
    info.pFragmentShader = (std::uint32_t*)mo_passthrough_shader_frag_spv.data();
    info.fragmentShaderSize = mo_passthrough_shader_frag_spv.size();
    info.flags = MO_PIPELINE_FEATURE_NONE;
    moCreatePipeline(&info, pPipeline);
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

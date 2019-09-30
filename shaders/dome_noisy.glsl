#version 450 core

layout(std140, binding = 0) uniform Block
{
    uniform mat4 viewMatrix;
    uniform mat4 projectionMatrix;
    uniform vec3 cameraPosition;
    uniform vec3 lightPosition;
} uniformData;

#ifdef COMPILING_VERTEX
layout(location=0) out VertexData
{
    vec3 vertex;
} outData;
layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec2 vertexTexcoord;
layout(location = 2) in vec3 vertexNormal;
layout(location = 3) in vec3 vertexTangent;
layout(location = 4) in vec3 vertexBitangent;
layout(push_constant) uniform uPushConstant
{
    mat4 modelMatrix;
} pc;

void main()
{
    outData.vertex = vec3(pc.modelMatrix * vec4(vertexPosition, 1.0));
    gl_Position = uniformData.projectionMatrix * mat4(mat3(uniformData.viewMatrix)) * pc.modelMatrix * vec4(vertexPosition, 1.0);
}
#endif

#ifdef COMPILING_FRAGMENT
layout(location = 0) out vec4 fragment;
layout(location = 0) in VertexData
{
    vec3 vertex;
} inData;
layout(set = 1, binding = 0) uniform sampler2D uniformTextureAmbient;
layout(set = 1, binding = 1) uniform sampler2D uniformTextureDiffuse;

#define MO_NOISE
#ifdef MO_NOISE
const float PHI = 1.61803398874989484820459 * 00000.1; // Golden Ratio
const float PI  = 3.14159265358979323846264 * 00000.1; // PI
const float SQ2 = 1.41421356237309504880169 * 10000.0; // Square Root of Two

float goldNoise(in vec2 coordinate, in float seed)
{
    return fract(tan(distance(coordinate * (seed + PHI), vec2(PHI, PI))) * SQ2);
}
#endif

void main()
{
    vec3 skyColor = texture(uniformTextureAmbient, vec2(0.0,0.0)).rgb;
    vec3 sunColor = texture(uniformTextureDiffuse, vec2(0.0,0.0)).rgb;
    vec3 position = inData.vertex;
    position.y = abs(position.y);

    vec3 sunPosition = normalize(uniformData.lightPosition - uniformData.cameraPosition);

    float sunCircle = max(1.0 - (1.0 + 10.0 * sunPosition.y + position.y) * length(inData.vertex.xyz - sunPosition), 0.0);
    sunCircle += 0.3 * pow(1.0 - position.y, 12.0) * (1.6 - sunPosition.y);
#ifdef MO_NOISE
    sunCircle += goldNoise(gl_FragCoord.xy * 0.1, 0.666) * 0.1;
#endif

    fragment = vec4(mix(skyColor.rgb, sunColor.rgb, sunCircle)
        * ((0.5 + 1.0 * pow(sunPosition.y, 0.4)) * (1.5 - position.y) + pow(sunCircle, 5.2)
        * sunPosition.y * (5.0 + 15.0 * sunPosition.y)), 1.0);
}
#endif

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

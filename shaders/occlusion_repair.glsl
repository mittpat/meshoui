#version 450 core

#ifdef COMPILING_VERTEX
layout(location=0) out VertexData
{
    vec3 vertex;
} outData;
layout(location = 0) in vec3 vertex;
layout(location = 1) in vec2 vertexTexcoord;
void main()
{
    outData.vertex = vertex + vec3(0.5,0.5,0);
    gl_Position = vec4(2*vertex.xy, 0, 1);
}
#endif

#ifdef COMPILING_FRAGMENT
layout(location = 0) out vec4 fragment;
layout(location = 0) in VertexData
{
    vec3 vertex;
} inData;
layout(set = 1, binding = 5) uniform sampler2D uniformTextureOcclusion;

vec4 sampleFramebufferAO()
{
    vec4 center = texture(uniformTextureOcclusion, inData.vertex.xy);
    if (center.a > 0.99)
        return vec4(center.rgb, 1);

    const ivec2 offsets0[4] = { ivec2(-1, 0), ivec2(1, 0), ivec2(0, -1), ivec2(0, 1) };
    vec4 red = textureGatherOffsets(uniformTextureOcclusion, inData.vertex.xy, offsets0, 0);
    vec4 green = textureGatherOffsets(uniformTextureOcclusion, inData.vertex.xy, offsets0, 1);
    vec4 blue = textureGatherOffsets(uniformTextureOcclusion, inData.vertex.xy, offsets0, 2);
    vec4 alpha = textureGatherOffsets(uniformTextureOcclusion, inData.vertex.xy, offsets0, 3);
    if (alpha[0] > 0.99)
        return vec4(red[0], green[0], blue[0], 1);
    if (alpha[1] > 0.99)
        return vec4(red[1], green[1], blue[1], 1);
    if (alpha[2] > 0.99)
        return vec4(red[2], green[2], blue[2], 1);
    if (alpha[3] > 0.99)
        return vec4(red[3], green[3], blue[3], 1);

    const ivec2 offsets1[4] = { ivec2(-1, 1), ivec2(1, 1), ivec2(-1, 1), ivec2(1, -1) };
    red = textureGatherOffsets(uniformTextureOcclusion, inData.vertex.xy, offsets1, 0);
    green = textureGatherOffsets(uniformTextureOcclusion, inData.vertex.xy, offsets1, 1);
    blue = textureGatherOffsets(uniformTextureOcclusion, inData.vertex.xy, offsets1, 2);
    alpha = textureGatherOffsets(uniformTextureOcclusion, inData.vertex.xy, offsets1, 3);
    if (alpha[0] > 0.99)
        return vec4(red[0], green[0], blue[0], 1);
    if (alpha[1] > 0.99)
        return vec4(red[1], green[1], blue[1], 1);
    if (alpha[2] > 0.99)
        return vec4(red[2], green[2], blue[2], 1);
    if (alpha[3] > 0.99)
        return vec4(red[3], green[3], blue[3], 1);

    return vec4(0);
}

void main()
{
    fragment = sampleFramebufferAO();
}
#endif

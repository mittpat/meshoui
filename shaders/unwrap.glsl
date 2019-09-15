#version 450 core

#ifdef COMPILING_VERTEX
layout(location = 0) in vec4 vertex;
layout(location = 1) in vec2 vertexTexcoord;
void main()
{
    gl_Position = vec4(vertexTexcoord * vec2(2,-2) + vec2(-1,1), 0, 1);
}
#endif

#ifdef COMPILING_FRAGMENT
layout(location = 0) out vec4 fragment;
layout(set = 2, binding = 0) uniform sampler2D uniformPreviousFramebuffer;
void main()
{
    vec4 previousFramebuffer = texture(uniformPreviousFramebuffer, gl_FragCoord.xy / textureSize(uniformPreviousFramebuffer, 0));
    vec4 currentFramebuffer = vec4(1);
    fragment = previousFramebuffer * 254/255.0 + currentFramebuffer * 2/255.0;
    fragment = clamp(fragment, vec4(0,0,0,1), currentFramebuffer);
}
#endif

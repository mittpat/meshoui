#version 450 core

#ifdef COMPILING_VERTEX
layout(location = 0) in vec4 vertex;
layout(location = 1) in vec2 vertexTexcoord;
void main()
{
    //gl_Position = vertex;
    gl_Position = vec4(vertexTexcoord * vec2(2,-2) + vec2(-1,1), 0, 1);
}
#endif

#ifdef COMPILING_FRAGMENT
layout(location = 0) out vec4 fragment;
void main()
{
    fragment = vec4(gl_FragCoord.xy / 128, 0, 1);
}
#endif

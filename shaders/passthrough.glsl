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
void main()
{
    fragment = vec4(inData.vertex.xy, 0, 1);
}
#endif

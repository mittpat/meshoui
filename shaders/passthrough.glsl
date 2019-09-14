#version 450 core

#ifdef COMPILING_VERTEX
layout(location = 0) in vec4 vertex;
void main()
{
    gl_Position = vertex;
}
#endif

#ifdef COMPILING_FRAGMENT
layout(location = 0) out vec4 fragment;
void main()
{
    fragment = vec4(1);
}
#endif

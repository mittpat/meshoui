#version 450 core

layout(std140, binding = 0) uniform Block
{
    uniform mat4 viewMatrix;
    uniform mat4 projectionMatrix;
    uniform vec3 cameraPosition;
    uniform vec3 lightPosition;
} uniformData;

#ifdef COMPILING_VERTEX
layout(location = 0) out VertexData
{
    vec3 vertex;
    vec3 normal;
    vec3 lightDir;
} outData;
layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec2 vertexTexcoord;
layout(location = 2) in vec3 vertexNormal;
layout(push_constant) uniform uPushConstant
{
    mat4 modelMatrix;
} pc;
void main()
{
    outData.vertex = vertexPosition;
    outData.normal = vertexNormal;
    outData.lightDir = normalize(inverse(mat3(pc.modelMatrix)) * uniformData.lightPosition);

    gl_Position = vec4(vertexTexcoord * vec2(2,-2) + vec2(-1,1), 0, 1);
}
#endif

#ifdef COMPILING_FRAGMENT
#extension GL_GOOGLE_include_directive : enable
#include "raytrace.h"
layout(std430, set = 3, binding = 0) buffer BVHObjects
{
    MoTriangle pObjects[];
} inBVHObjects;
layout(std430, set = 3, binding = 1) buffer BVHSplitNodes
{
    MoBVHSplitNode pSplitNodes[];
} inBVHSplitNodes;

// Working set
MoBVHWorkingSet traversal[64];
bool moIntersectTriangleBVH(in MoRay ray, out MoIntersectResult result)
{
    result.distance = 1.0 / 0.0;
    int stackPtr = 0;

    traversal[stackPtr].index = 0;
    traversal[stackPtr].distance = -1.0 / 0.0;

    while (stackPtr >= 0)
    {
        uint index = traversal[stackPtr].index;
        float near = traversal[stackPtr].distance;
        stackPtr--;
        MoBVHSplitNode node = inBVHSplitNodes.pSplitNodes[index];

        if (near > result.distance)
        {
            continue;
        }

        if (node.offset == 0)
        {
            float t = 1.0 / 0.0;
            float u, v;
            MoTriangle triangle = inBVHObjects.pObjects[node.start];
            if (moRayTriangleIntersect(triangle, ray, t, u, v))
            {
                if (t < result.distance)
                {
                    result.triangle = triangle;
                    result.distance = t;
                }
                if (t <= 0.0)
                {
                    return true;
                }
            }
        }
        else
        {
            uint closer = index + 1;
            uint other = index + node.offset;

            float bbhits[5];

            bool hitLeft = moIntersect(inBVHSplitNodes.pSplitNodes[index + 1].boundingBox, ray, bbhits[0], bbhits[1]);
            bool hitRight = moIntersect(inBVHSplitNodes.pSplitNodes[index + node.offset].boundingBox, ray, bbhits[2], bbhits[3]);

            if (hitLeft && hitRight)
            {
                if (bbhits[2] < bbhits[0])
                {
                    other = index + 1;
                    closer = index + node.offset;
                    // swap
                    bbhits[4] = bbhits[0];
                    bbhits[0] = bbhits[2];
                    bbhits[2] = bbhits[4];
                    // swap
                    bbhits[4] = bbhits[1];
                    bbhits[1] = bbhits[3];
                    bbhits[3] = bbhits[4];
                }

                ++stackPtr;
                traversal[stackPtr] = MoBVHWorkingSet(other, bbhits[2]);
                ++stackPtr;
                traversal[stackPtr] = MoBVHWorkingSet(closer, bbhits[0]);
            }
            else if (hitLeft)
            {
                ++stackPtr;
                traversal[stackPtr] = MoBVHWorkingSet(closer, bbhits[0]);
            }
            else if (hitRight)
            {
                ++stackPtr;
                traversal[stackPtr] = MoBVHWorkingSet(other, bbhits[2]);
            }
        }
    }

    return result.distance < (1.0 / 0.0);
}

const vec3 SurfaceBias = vec3(0.01);
layout(location = 0) out vec4 fragment;
layout(location = 0) in VertexData
{
    vec3 vertex;
    vec3 normal;
    vec3 lightDir;
} inData;
void main()
{
    MoRay ray;
    moInitRay(ray, fma(inData.normal, SurfaceBias, inData.vertex), inData.lightDir);
    MoIntersectResult nextResult;
    if (moIntersectTriangleBVH(ray, nextResult))
    {
        fragment = vec4(0,0,0,1);
    }
    else
    {
        fragment = vec4(1);
    }
}
#endif

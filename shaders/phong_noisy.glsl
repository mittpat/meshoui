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
    vec3 normal;
    vec2 texcoord;
    mat3 TBN;
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
    outData.normal = normalize(mat3(transpose(inverse(pc.modelMatrix))) * vertexNormal);
    outData.texcoord = vertexTexcoord;
    vec3 T = normalize(mat3(pc.modelMatrix) * vertexTangent);
    vec3 B = normalize(mat3(pc.modelMatrix) * vertexBitangent);
    vec3 N = normalize(mat3(pc.modelMatrix) * vertexNormal);
    outData.TBN = mat3(T, B, N);
    gl_Position = uniformData.projectionMatrix * uniformData.viewMatrix * pc.modelMatrix * vec4(vertexPosition, 1.0);
}
#endif

#ifdef COMPILING_FRAGMENT
layout(location = 0) out vec4 fragment;
layout(location = 0) in VertexData
{
    vec3 vertex;
    vec3 normal;
    vec2 texcoord;
    mat3 TBN;
} inData;
layout(set = 1, binding = 0) uniform sampler2D uniformTextureAmbient;
layout(set = 1, binding = 1) uniform sampler2D uniformTextureDiffuse;
layout(set = 1, binding = 2) uniform sampler2D uniformTextureNormal;
layout(set = 1, binding = 3) uniform sampler2D uniformTextureSpecular;
layout(set = 1, binding = 4) uniform sampler2D uniformTextureEmissive;
layout(set = 1, binding = 5) uniform sampler2D uniformTextureOcclusion;

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
    vec4 textureAmbient = texture(uniformTextureAmbient, inData.texcoord);
    vec4 textureDiffuse = texture(uniformTextureDiffuse, inData.texcoord);
    vec4 textureSpecular = texture(uniformTextureSpecular, inData.texcoord);
    vec4 textureNormal = texture(uniformTextureNormal, inData.texcoord);
    vec4 textureEmissive = texture(uniformTextureEmissive, inData.texcoord);
    vec4 textureOcclusion = texture(uniformTextureOcclusion, inData.texcoord);

    // discard textureNormal when ~= (0,0,0)
    vec3 normal = length(textureNormal) > 0.1 ? inData.TBN * normalize(2.0 * vec3(textureNormal.x, 1.0 - textureNormal.y, textureNormal.z) - 1.0) : inData.normal;
    vec3 viewDir = normalize(uniformData.cameraPosition - inData.vertex);
    vec3 lightDir = normalize(uniformData.lightPosition - inData.vertex);
    vec3 occlusion = mix(vec3(1), textureOcclusion.rgb, textureOcclusion.a);
#define MO_DARK_LIGHT_ABOVE
#ifdef MO_DARK_LIGHT_ABOVE
    lightDir = normalize(mix(lightDir, vec3(0,1,0), 1 - occlusion.r));
#endif

    const float kPi = 3.14159265;
    const float kShininess = 16.0;
    const float kEnergyConservation = ( 8.0 + kShininess ) / ( 8.0 * kPi );
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = kEnergyConservation * pow(max(dot(normal, halfwayDir), 0.0), kShininess);

    fragment = vec4(textureAmbient.rgb * textureDiffuse.rgb, textureDiffuse.a);
    float diffuseFactor = dot(normal, lightDir);
#ifdef MO_NOISE
    diffuseFactor += goldNoise(gl_FragCoord.xy * 0.1, 0.666) * 0.5;
#endif
    if (diffuseFactor > 0.0)
    {
        fragment += vec4(vec3(diffuseFactor) * (occlusion/1.1+vec3(1-1/1.1)) * (textureDiffuse.rgb + spec * textureSpecular.rgb * occlusion), 0.0);
    }
    fragment += textureEmissive;
}
#endif

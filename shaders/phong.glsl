#version 450 core

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
    mat4 uniformModel;
    mat4 uniformView;
    mat4 uniformProjection;
} pc;

void main()
{
    outData.vertex = vec3(pc.uniformModel * vec4(vertexPosition, 1.0));
    outData.normal = normalize(mat3(transpose(inverse(pc.uniformModel))) * vertexNormal);
    outData.texcoord = vertexTexcoord;
    vec3 T = normalize(mat3(pc.uniformModel) * vertexTangent);
    vec3 B = normalize(mat3(pc.uniformModel) * vertexBitangent);
    vec3 N = normalize(mat3(pc.uniformModel) * vertexNormal);
    outData.TBN = mat3(T, B, N);
    gl_Position = pc.uniformProjection * pc.uniformView * pc.uniformModel * vec4(vertexPosition, 1.0);
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
layout(std140, binding = 0) uniform Block
{
    uniform vec3 viewPosition;
    uniform vec3 lightPosition;
} uniformData;
layout(set = 1, binding = 0) uniform sampler2D uniformTextureAmbient;
layout(set = 1, binding = 1) uniform sampler2D uniformTextureDiffuse;
layout(set = 1, binding = 2) uniform sampler2D uniformTextureNormal;
layout(set = 1, binding = 3) uniform sampler2D uniformTextureSpecular;
layout(set = 1, binding = 4) uniform sampler2D uniformTextureEmissive;

void main()
{
    vec4 textureAmbient = texture(uniformTextureAmbient, inData.texcoord);
    vec4 textureDiffuse = texture(uniformTextureDiffuse, inData.texcoord);
    vec4 textureSpecular = texture(uniformTextureSpecular, inData.texcoord);
    vec4 textureNormal = texture(uniformTextureNormal, inData.texcoord);

    // discard textureNormal when ~= (0,0,0)
    vec3 normal = length(textureNormal) > 0.1 ? inData.TBN * normalize(2.0 * vec3(textureNormal.x, 1.0 - textureNormal.y, textureNormal.z) - 1.0) : inData.normal;
    vec3 viewDir = normalize(uniformData.viewPosition - inData.vertex);
    vec3 lightDir = normalize(uniformData.lightPosition - inData.vertex);

    const float kPi = 3.14159265;
    const float kShininess = 16.0;
    const float kEnergyConservation = ( 8.0 + kShininess ) / ( 8.0 * kPi );
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = kEnergyConservation * pow(max(dot(normal, halfwayDir), 0.0), kShininess);

    fragment = vec4(textureAmbient.rgb * textureDiffuse.rgb, textureDiffuse.a);
    float diffuseFactor = dot(normal, lightDir);
    if (diffuseFactor > 0.0)
    {
        fragment += vec4(diffuseFactor * textureDiffuse.rgb + spec * textureSpecular.rgb, 0.0);
    }
}
#endif

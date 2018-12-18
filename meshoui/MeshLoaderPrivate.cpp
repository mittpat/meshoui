#include "MeshLoaderPrivate.h"
#include "MeshLoader.h"

#include <algorithm>
#include <linalg.h>
using namespace linalg;
using namespace linalg::aliases;
#define MESHOUI_COLLADA_LINALG
#include <collada.h>

using namespace Meshoui;

namespace
{
    #define DOT_PRODUCT_THRESHOLD 0.9f
    struct Node
    {
        Node(const Vertex & v, unsigned int i) : vertex(v), index(i) {}
        float3 position() const { return vertex.position; }
        bool operator==(const Vertex &rhs) const { return vertex.position == rhs.position && vertex.texcoord == rhs.texcoord && linalg::dot(vertex.normal, rhs.normal) > DOT_PRODUCT_THRESHOLD; }
        Vertex vertex;
        unsigned int index;
    };

    template<typename T>
    struct Octree final
    {
        ~Octree() { for (auto * child : children) { delete child; } }
        Octree(const float3 & origin, const float3 & halfDimension) : origin(origin), halfDimension(halfDimension), children({}), data() {}
        Octree(const Octree & copy) : origin(copy.origin), halfDimension(copy.halfDimension), data(copy.data) {}
        bool isLeafNode() const { return children[0] == nullptr; }

        unsigned getOctantContainingPoint(const float3 & point) const
        {
            unsigned oct = 0;
            if (point.x >= origin.x) oct |= 4;
            if (point.y >= origin.y) oct |= 2;
            if (point.z >= origin.z) oct |= 1;
            return oct;
        }

        void insert(const T & point)
        {
            if (isLeafNode())
            {
                if (data.empty() || point.position() == data.front().position())
                {
                    data.push_back(point);
                    return;
                }
                else
                {
                    float3 nextHalfDimension = halfDimension*.5f;
                    if (nextHalfDimension.x == 0.f || nextHalfDimension.y == 0.f || nextHalfDimension.z == 0.f)
                    {
                        data.push_back(point);
                    }
                    else
                    {
                        for (unsigned i = 0; i < 8; ++i)
                        {
                            float3 newOrigin = origin;
                            newOrigin.x += halfDimension.x * (i&4 ? .5f : -.5f);
                            newOrigin.y += halfDimension.y * (i&2 ? .5f : -.5f);
                            newOrigin.z += halfDimension.z * (i&1 ? .5f : -.5f);
                            children[i] = new Octree(newOrigin, nextHalfDimension);
                        }
                        for (const auto & oldPoint : data)
                        {
                            children[getOctantContainingPoint(oldPoint.position())]->insert(oldPoint);
                        }
                        data.clear();
                        children[getOctantContainingPoint(point.position())]->insert(point);
                    }
                }
            }
            else
            {
                unsigned octant = getOctantContainingPoint(point.position());
                children[octant]->insert(point);
            }
        }

        void getPointsInsideBox(const float3 & bmin, const float3 & bmax, std::vector<T> & results)
        {
            if (isLeafNode())
            {
                if (!data.empty())
                {
                    for (const auto & point : data)
                    {
                        const float3 p = point.position();
                        if (p.x > bmax.x || p.y > bmax.y || p.z > bmax.z) return;
                        if (p.x < bmin.x || p.y < bmin.y || p.z < bmin.z) return;
                        results.push_back(point);
                    }
                }
            }
            else
            {
                for (auto * child : children)
                {
                    const float3 cmax = child->origin + child->halfDimension;
                    const float3 cmin = child->origin - child->halfDimension;
                    if (cmax.x < bmin.x || cmax.y < bmin.y || cmax.z < bmin.z) continue;
                    if (cmin.x > bmax.x || cmin.y > bmax.y || cmin.z > bmax.z) continue;
                    child->getPointsInsideBox(bmin, bmax, results);
                }
            }
        }

        float3 origin, halfDimension;
        std::array<Octree*, 8> children;
        std::vector<T> data;
    };
}

void MeshLoader::buildGeometry(MeshDefinition & definition, const DAE::Mesh & mesh, bool renormalize)
{
    struct AABB final
    {
        AABB() : lower(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()),
                 upper(std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min()) {}
        void extend(float3 p) { lower = min(lower, p); upper = max(upper, p); }
        float3 center() const { return (lower + upper) * 0.5f; }
        float3 half() const { return (upper - lower) * 0.5f; }
        float3 lower, upper;
    } bbox;
    for (const auto & vertex : mesh.vertices)
        bbox.extend({vertex.x,vertex.y,vertex.z});

    std::vector<Node> nodes;
    nodes.reserve(unsigned(sqrt(mesh.triangles.size())));
    Octree<Node> octree(bbox.center(), bbox.half());

    printf("Loading '%s'\n", definition.definitionId.str.empty() ? "(unnamed root)" : definition.definitionId.str.c_str());

    // indexed
    for (size_t i = 0; i < mesh.triangles.size(); ++i)
    {
        std::array<Vertex, 3> avertex;

        const auto & ivertices = mesh.triangles[i].vertices;
        avertex[0].position = mesh.vertices[ivertices.x-1];
        avertex[1].position = mesh.vertices[ivertices.y-1];
        avertex[2].position = mesh.vertices[ivertices.z-1];

        const auto & itexcoords = mesh.triangles[i].texcoords;
        if (itexcoords.x-1 < mesh.texcoords.size())
            avertex[0].texcoord = mesh.texcoords[itexcoords.x-1];
        if (itexcoords.y-1 < mesh.texcoords.size())
            avertex[1].texcoord = mesh.texcoords[itexcoords.y-1];
        if (itexcoords.z-1 < mesh.texcoords.size())
            avertex[2].texcoord = mesh.texcoords[itexcoords.z-1];

        if (renormalize)
        {
            float3 a = avertex[1].position - avertex[0].position;
            float3 b = avertex[2].position - avertex[0].position;
            float3 normal = normalize(cross(a, b));
            avertex[0].normal = normal;
            avertex[1].normal = normal;
            avertex[2].normal = normal;
        }
        else
        {
            const auto & inormals = mesh.triangles[i].normals;
            if (inormals.x-1 < mesh.normals.size())
                avertex[0].normal = mesh.normals[inormals.x-1];
            if (inormals.y-1 < mesh.normals.size())
                avertex[1].normal = mesh.normals[inormals.y-1];
            if (inormals.z-1 < mesh.normals.size())
                avertex[2].normal = mesh.normals[inormals.z-1];
        }

        // tangent + bitangent
        float3 edge1 = avertex[1].position - avertex[0].position;
        float3 edge2 = avertex[2].position - avertex[0].position;
        float2 deltaUV1 = avertex[1].texcoord - avertex[0].texcoord;
        float2 deltaUV2 = avertex[2].texcoord - avertex[0].texcoord;

        float f = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
        if (f != 0.f)
        {
            f = 1.0f / f;

            float3 tangent(0.f,0.f,0.f);
            tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
            tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
            tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
            avertex[0].tangent = avertex[1].tangent = avertex[2].tangent = normalize(tangent);

            float3 bitangent(0.f,0.f,0.f);
            bitangent.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
            bitangent.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
            bitangent.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
            avertex[0].bitangent = avertex[1].bitangent = avertex[2].bitangent = normalize(bitangent);
        }

        for (auto & vertex : avertex)
        {
            nodes.clear();
            octree.getPointsInsideBox(vertex.position, vertex.position, nodes);
            auto found = std::find(nodes.begin(), nodes.end(), vertex);
            if (found == nodes.end())
            {
                definition.vertices.push_back(vertex);
                definition.indices.push_back(unsigned(definition.vertices.size()) - 1);
                octree.insert(Node(vertex, definition.indices.back()));
            }
            else
            {
                definition.indices.push_back((*found).index);
            }
        }
    }
}

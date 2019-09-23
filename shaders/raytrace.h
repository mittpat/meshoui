/// RAY
struct MoRay
{
    vec3 origin;
    vec3 direction;
    vec3 oneOverDirection;
};

void moInitRay(inout MoRay self, in vec3 origin, in vec3 direction)
{
    self.origin = origin;
    self.direction = direction;
    self.oneOverDirection = 1.0 / self.direction;
}

/// BBOX
struct MoBBox
{
    vec3 min;
    vec3 max;
};

bool moIntersect(in MoBBox self, in MoRay ray, out float t_near, out float t_far)
{
    vec3 t1 = (self.min - ray.origin) * ray.oneOverDirection;
    vec3 t2 = (self.max - ray.origin) * ray.oneOverDirection;

    t_near = max(max(min(t1.x, t2.x), min(t1.y, t2.y)), min(t1.z, t2.z));
    t_far = min(min(max(t1.x, t2.x), max(t1.y, t2.y)), max(t1.z, t2.z));

    return t_far >= t_near;
}

/// TRIANGLE
struct MoTriangle
{
    vec3 v0, v1, v2;
};

bool moRayTriangleIntersect(in MoTriangle self, in MoRay ray, out float t, out float u, out float v)
{
    float EPSILON = 0.0000001f;
    vec3 edge1 = self.v1 - self.v0;
    vec3 edge2 = self.v2 - self.v0;
    vec3 h = cross(ray.direction, edge2);
    float a = dot(edge1, h);
    if (a > -EPSILON && a < EPSILON)
    {
        // This ray is parallel to this triangle.
        return false;
    }

    float f = 1.0/a;
    vec3 s = ray.origin - self.v0;
    u = f * dot(s, h);
    if (u < 0.0 || u > 1.0)
    {
        return false;
    }

    vec3 q = cross(s, edge1);
    v = f * dot(ray.direction, q);
    if (v < 0.0 || u + v > 1.0)
    {
        return false;
    }

    // At this stage we can compute t to find out where the intersection point is on the line.
    t = f * dot(edge2, q);
    if (t > EPSILON)
    {
        // ray intersection
        return true;
    }

    // This means that there is a line intersection but not a ray intersection.
    return false;
}

/// BVH
struct MoBVHSplitNode
{
    MoBBox boundingBox;
    uint start;
    uint offset;
};

struct MoBVHWorkingSet
{
    uint index;
    float distance;
};

struct MoIntersectResult
{
    MoTriangle triangle;
    float distance;
};

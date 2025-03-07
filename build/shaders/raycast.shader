#version 440 core
layout (local_size_x = 1, local_size_y = 1) in;

struct Vertex
{
    vec3 pos;
    vec3 normal;
    float u, v;
};

struct BVH_Node
{
    vec3 aabbMin;
    vec3 aabbMax;
    uint leftChild;
    uint rightChild;
    uint firstIndex;
    uint indexCount;
};

struct MeshPartition
{
    uint verticesStart;
    uint indicesStart;
    uint materialIndex;
    uint bvhNodeStart;
    mat4x4 inverseTransform;
};

struct CameraInfo
{
    vec3 pos;
    vec3 forward;
    vec3 right;
    vec3 up;
    float FOV;
    uint DOF;
    float focusDistance;
    float aperture;
    uint antiAliasing;
    float exposure;
};

struct Ray
{
    vec3 origin;
    vec3 dir;
};

struct RayHit
{
    vec3 pos;
    vec3 normal;
    vec2 uv;
    float dist;
    bool hit;
    bool frontFace;
    uint materialIndex;
};

struct RaycastHit
{
    int meshIndex;
};

layout(binding = 2) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(binding = 3) readonly buffer IndexBuffer {
    uint indices[];
};

layout(binding = 5) readonly buffer BVHBuffer {
    BVH_Node bvhNodes[];
};

layout(binding = 6) readonly buffer PartitionBuffer {
    MeshPartition meshPartitions[];
};

layout(binding = 11) buffer RaycastBuffer {
    RaycastHit raycastHit[];
};

uniform CameraInfo cameraInfo;
uniform int u_meshCount;
uniform int u_cursorX;
uniform int u_cursorY;
uniform int u_width;
uniform int u_height;

float DegreesToRadians(float degrees)
{
    return degrees * 0.01745329f;
}

vec3 PixelRayPos(int x, int y)
{
    float FOV_Radians = DegreesToRadians(cameraInfo.FOV);
    float aspectRatio = float(u_width) / float(u_height);
    float nearPlane = 0.1f;
    
    // VIEWING PLANE
    float planeHeight = nearPlane * tan(FOV_Radians * 0.5f);
    float planeWidth = planeHeight * aspectRatio;

    // NORMALISED PIXEL COORDINATES
    float nx = (x) / (u_width - 1.0f); 
    float ny = (y) / (u_height - 1.0f);

    // CALCULATE PIXEL COORDINATE IN PLANE SPACE
    vec3 localBL = vec3(-planeWidth * 0.5f, -planeHeight * 0.5f, nearPlane);
    vec3 localPoint = localBL + vec3(planeWidth * nx, planeHeight * ny, 0.0f);

    // CALCULATE PIXEL COORDINATE IN WORLD SPACE
    vec3 worldPoint = cameraInfo.pos - cameraInfo.right * localPoint.x + cameraInfo.up * localPoint.y + cameraInfo.forward * localPoint.z;
    return worldPoint;
}

// adapted from https://jacco.ompf2.com/2022/04/13/how-to-build-a-bvh-part-1-basics/
float IntersectAABB(Ray ray, vec3 aabbMin, vec3 aabbMax)
{
    vec3 tMin = (aabbMin - ray.origin) * (1.0f / ray.dir);
    vec3 tMax = (aabbMax - ray.origin) * (1.0f / ray.dir);
    vec3 t1 = min(tMin, tMax);
    vec3 t2 = max(tMin, tMax);
    float distFar = min(min(t2.x, t2.y), t2.z);
    float distNear = max(max(t1.x, t1.y), t1.z);
    bool hit = distFar >= distNear && distFar > 0.0f;
    return hit ? distNear : 100000.0f;
}

RayHit RayTriangle(Ray ray, Vertex v1, Vertex v2, Vertex v3)
{
    // DEFAULT RAY HIT
    RayHit hit;
    hit.pos = vec3(0.0f, 0.0f, 0.0f);
    hit.normal = vec3(0.0f, 0.0f, 0.0f);
    hit.dist = 10000000.0f;
    hit.hit = false;

    // CALCULATE THE DETERMINANT
    vec3 edge1 = v2.pos - v1.pos;
    vec3 edge2 = v3.pos - v1.pos;
    vec3 p = cross(ray.dir, edge2);
    float determinant = dot(edge1, p);
    if (abs(determinant) < 0.000001f) return hit;

    // CALCULATE U BARYCENTRIC COORDINATE
    float inverseDeterminant = 1.0f / determinant;
    vec3 v1TOorigin = ray.origin - v1.pos;
    float u = dot(v1TOorigin, p) * inverseDeterminant;
    if (u < 0.0f || u > 1.0f) return hit;

    // CALCULATE V BARYCENTRIC COORDINATE
    vec3 q = cross(v1TOorigin, edge1);
    float v = dot(ray.dir, q) * inverseDeterminant;
    if (v < 0.0f || u + v > 1.0f) return hit;

    // CALCULATE HIT DISTANCE
    float dist = dot(edge2, q) * inverseDeterminant;
    if (dist < 0.0f) return hit;

    // CALCULATE W BARYCENTRIC COORDINATE
    float w = 1.0f - u - v;

    // SET AND RETURN THE HIT INFORMATION
    hit.pos = ray.origin + ray.dir * dist;
    vec3 normal = normalize(v1.normal * w + v2.normal * u + v3.normal * v); // INTERPOLATE NORMAL USING BARYCENTRIC COORDINATES

    // SET HIT VALUES
    hit.frontFace = dot(ray.dir, normal) < 0.0f;
    hit.normal = hit.frontFace ? normal : -normal;
    hit.uv = vec2(v1.u, v1.v) * w + vec2(v2.u, v2.v) * u + vec2(v3.u, v3.v) * v;
    hit.dist = dist;
    hit.hit = true;
    return hit;
}

int Raycast(Ray ray)
{   
    float hitDist = 100000.0f;
    int meshIndex = -1;

    // FOR EACH MESH
    for (int m=0; m<u_meshCount; m++) 
    {
        uint indicesStart = meshPartitions[m].indicesStart;
        uint verticesStart = meshPartitions[m].verticesStart;
        uint bvhStart = meshPartitions[m].bvhNodeStart;

        // TRANSFORM RAY TO BE IN MESH SPACE
        Ray transformedRay;
        transformedRay.origin = (meshPartitions[m].inverseTransform * vec4(ray.origin, 1.0)).xyz;
        transformedRay.dir = (meshPartitions[m].inverseTransform * vec4(ray.dir, 0.0)).xyz;

        // BVH TRAVERSAL
        uint stack[32];
        int stackIndex = 0;
        stack[stackIndex] = bvhStart;
        while(stackIndex >= 0)
        {
            BVH_Node node = bvhNodes[stack[stackIndex--]];

            if (node.indexCount == 0)
            {
                BVH_Node leftChild = bvhNodes[node.leftChild + bvhStart];
                BVH_Node rightChild = bvhNodes[node.rightChild + bvhStart];

                float leftBoxDist = IntersectAABB(transformedRay, leftChild.aabbMin, leftChild.aabbMax);
                float rightBoxDist = IntersectAABB(transformedRay, rightChild.aabbMin, rightChild.aabbMax);
                
                if (leftBoxDist > rightBoxDist)
                {
                    if (leftBoxDist < hitDist) stack[++stackIndex] = node.leftChild + bvhStart;
                    if (rightBoxDist < hitDist) stack[++stackIndex] = node.rightChild + bvhStart;
                }
                else
                {
                    if (rightBoxDist < hitDist) stack[++stackIndex] = node.rightChild + bvhStart;
                    if (leftBoxDist < hitDist) stack[++stackIndex] = node.leftChild + bvhStart;
                }
            }

            // NODE IS A LEAF: CHECK FOR TRIANGLE INTERSECTION
            else
            {
                // FOR EACH TRIANGLE IN NODE's BOUNDING BOX
                for (int i=0; i<node.indexCount; i+=3) 
                {
                    uint index = node.firstIndex + indicesStart + i;
                    uint v1_index = verticesStart + indices[index];
                    uint v2_index = verticesStart + indices[index + 1];
                    uint v3_index = verticesStart + indices[index + 2];
                    RayHit hit = RayTriangle(
                        transformedRay, 
                        vertices[v1_index], 
                        vertices[v2_index], 
                        vertices[v3_index]
                    );
                    if (hit.dist < hitDist) 
                    {
                        hitDist = hit.dist;
                        meshIndex = m;
                    }
                }
            }
        }
    }
    return meshIndex;
}

void main()
{   
    // CREATE CAMERA RAY FOR THIS PIXEL
    Ray camRay;
    camRay.origin = PixelRayPos(u_cursorX, u_cursorY);
    camRay.dir = normalize(camRay.origin - cameraInfo.pos);

    // PERFORM THE RAYCAST
    raycastHit[0].meshIndex = Raycast(camRay);
}


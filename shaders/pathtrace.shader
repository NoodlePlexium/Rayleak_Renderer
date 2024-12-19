#version 440 core

layout (local_size_x = 32, local_size_y = 32) in;
layout (binding = 0, rgba8) uniform image2D renderImage;

struct Vertex
{
    vec3 
    pos;
    vec3 normal;
    float u, v;
};

struct Material
{
    vec3 colour;
    float roughness;
    float emission;
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
};

struct CameraInfo
{
    vec3 pos;
    vec3 forward;
    vec3 right;
    vec3 up;
    float FOV;
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
    float dist;
    bool hit;
    uint materialIndex;
};


layout(binding = 1) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(binding = 2) readonly buffer IndexBuffer {
    uint indices[];
};

layout(binding = 3) readonly buffer MaterialBuffer {
    Material materials[];
};

layout(binding = 4) readonly buffer BVHBuffer {
    BVH_Node bvhNodes[];
};

layout(binding = 5) readonly buffer PartitionBuffer {
    MeshPartition meshPartitions[];
};

uniform CameraInfo cameraInfo;
uniform int u_meshCount;
uniform uint u_frameCount;
uniform uint u_accumulationFrame;








// FROM Sebastian Lague
// BY // www.pcg-random.org and www.shadertoy.com/view/XlGcRh
float Random(uint seed)
{
    uint state = seed * 747796405 + 2891336453;
    uint result = ((state >> ((state >> 28) + 4)) ^ state) * 277803737;
    result = (result >> 22) ^ result;
    return result / 4294967295.0f;
}
float RandomValueNormalDistribution(uint seed)
{
    float theta = 2.0f * 3.1415926 * Random(seed);
    float rho = sqrt(-2.0f * log(Random(seed + 124284930)));
    return rho * cos(theta);
}

vec3 RandomDirection(uint seed)
{
    float x = RandomValueNormalDistribution(seed * 957356527);
    float y = RandomValueNormalDistribution(seed * 821473903);
    float z = RandomValueNormalDistribution(seed * 618031433);
    vec3 randomDir = vec3(x, y, z);
    return normalize(randomDir);
}

vec3 RandomHemisphereDirection(vec3 normal, uint seed)
{
    vec3 randDir = RandomDirection(seed);
    if (dot(randDir, normal) < 0.0f)
    {
        randDir = -randDir;
    }
    return randDir;
}

vec3 PixelRayPos(uint x, uint y, float width, float height)
{
    float FOV_Radians = cameraInfo.FOV * 0.01745329f;
    float aspectRatio = float(width) / float(height);
    float nearPlane = 0.1f;
    
    // VIEWING PLANE
    float planeHeight = nearPlane * tan(FOV_Radians * 0.5f);
    float planeWidth = planeHeight * aspectRatio;

    // NORMALISE PIXEL COORDINATES
    float nx = x / (width - 1.0f);
    float ny = y / (height - 1.0f);

    // CALCULATE PIXEL COORDINATE IN PLANE SPACE
    vec3 localBL = vec3(-planeWidth * 0.5f, -planeHeight * 0.5f, nearPlane);
    vec3 localPoint = localBL + vec3(planeWidth * nx, planeHeight * ny, 0.0f);

    // CALCULATE PIXEL COORDINATE IN WORLD SPACE
    vec3 worldPoint = cameraInfo.pos + cameraInfo.right * localPoint.x + cameraInfo.up * localPoint.y + cameraInfo.forward * localPoint.z;
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
    hit.dist = 100000.0f;
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
    hit.normal = normalize(v1.normal * w + v2.normal * u + v3.normal * v); // INTERPOLATE NORMAL USING BARYCENTRIC COORDINATES
    // hit.normal = normalize(cross(edge1, edge2));
    hit.dist = dist;
    hit.hit = true;
    return hit;
}

RayHit CastRay(Ray ray)
{   
    RayHit hit;
    hit.dist = 10000000.0f;
    hit.hit = false;

    // FOR EACH MESH
    for (int m = 0; m < u_meshCount; m++) 
    {
        uint indicesStart = meshPartitions[m].indicesStart;
        uint verticesStart = meshPartitions[m].verticesStart;
        uint bvhStart = meshPartitions[m].bvhNodeStart;

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

                float leftBoxDist = IntersectAABB(ray, leftChild.aabbMin, leftChild.aabbMax);
                float rightBoxDist = IntersectAABB(ray, rightChild.aabbMin, rightChild.aabbMax);
                
                if (leftBoxDist > rightBoxDist)
                {
                    if (leftBoxDist < hit.dist) stack[++stackIndex] = node.leftChild + bvhStart;
                    if (rightBoxDist < hit.dist) stack[++stackIndex] = node.rightChild + bvhStart;
                }
                else
                {
                    if (rightBoxDist < hit.dist) stack[++stackIndex] = node.rightChild + bvhStart;
                    if (leftBoxDist < hit.dist) stack[++stackIndex] = node.leftChild + bvhStart;
                }
            }

            // NODE IS A LEAF: CHECK FOR TRIANGLE INTERSECTION
            else
            {
                // FOR EACH TRIANGLE IN NODE's BOUNDING BOX
                for (int i = 0; i < node.indexCount; i += 3) 
                {
                    uint index = node.firstIndex + indicesStart + i;
                    RayHit newHit = RayTriangle(
                        ray, 
                        vertices[verticesStart + indices[index]], 
                        vertices[verticesStart + indices[index + 1]], 
                        vertices[verticesStart + indices[index + 2]]
                    );
                    if (newHit.dist < hit.dist) {
                        hit = newHit;
                        hit.materialIndex = meshPartitions[m].materialIndex;
                    }
                }
            }
        }
    }
    return hit;
}


vec3 PathTrace(Ray ray, int bounces, uint seed)
{
    vec3 light = vec3(0.0f, 0.0f, 0.0f);
    vec3 rayColour = vec3(1.0f, 1.0f, 1.0f);

    for (int b=0; b<1+bounces; b++)
    {
        RayHit hit = CastRay(ray);
        
        if (hit.hit)
        {
            const Material material = materials[hit.materialIndex];

            float cosineFactor = max(0.0f, dot(hit.normal, -ray.dir));
            light += rayColour * material.emission;
            rayColour *= material.colour * cosineFactor;
            
            vec3 diffuseDir = RandomHemisphereDirection(hit.normal, seed + b);
            vec3 specularDir = ray.dir - hit.normal * 2.0f * dot(ray.dir, hit.normal);
            ray.dir = normalize(diffuseDir * material.roughness + specularDir * (1.0f - material.roughness)); 
            ray.origin = hit.pos + hit.normal * 0.0001f; 
        }
        else
        {
            light += vec3(0.5f, 0.7f, 0.95f) * 1.0f * rayColour;
            break;
        }
    }
    return light;
}

// SIMPLIFIED ACES TONE MAPPING
vec3 ACES(vec3 colour)
{
    vec3 numerator = colour * (2.51f * colour + 0.03f);
    vec3 denominator = colour * (2.43f * colour + 0.59f) + 0.14f;
    vec3 result = numerator / denominator;
    return clamp(result, 0.0f, 1.0f);
}

void main()
{   
    // GET IMAGE DIMENSIONS
    float width = imageSize(renderImage).x;
    float height = imageSize(renderImage).y;

    // CREATE CAMERA FOR THIS PIXEL
    Ray camRay;
    camRay.origin = PixelRayPos(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y, width, height);
    camRay.dir = normalize(camRay.origin - cameraInfo.pos);

    // GENERATE A PSEUDORANDOM SEED
    uint pixelIndex = gl_GlobalInvocationID.y * uint(width) + gl_GlobalInvocationID.x;
    uint seed = u_frameCount * 477300 + pixelIndex * 241263;

    // TRACE CAMERA TO GET PIXEL COLOUR 
    vec3 colour = ACES(PathTrace(camRay, 2, seed));

    // FRAME ACCUMULATION
    vec4 lastFrameColour = imageLoad(renderImage, ivec2(gl_GlobalInvocationID.xy)); 
    float weight = 1.0f / (u_accumulationFrame + 1.0f);
    vec4 accColour = lastFrameColour * (1.0f - weight) + vec4(colour, 1.0f) * weight;
    accColour = clamp(accColour, 0.0f, 1.0f);
    imageStore(renderImage, ivec2(gl_GlobalInvocationID.xy), vec4(accColour.xyz, 1.0f));  

}


#version 440 core

layout (local_size_x = 32, local_size_y = 32) in;
layout (binding = 0) uniform writeonly image2D renderImage;

struct Vertex
{
    float x, y, z;
    float nx, ny, nz;
    float u, v;
};

struct MeshPartition
{
    uint verticesStart;
    uint indicesStart;
    uint indicesCount;
};

layout(binding = 1) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(binding = 2) readonly buffer IndexBuffer {
    uint indices[];
};

layout(binding = 3) readonly buffer PartitionBuffer {
    MeshPartition meshPartitions[];
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
};

uniform CameraInfo cameraInfo;
uniform int u_meshCount;

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

    vec3 localBL = vec3(-planeWidth * 0.5f, -planeHeight * 0.5f, nearPlane);
    vec3 localPoint = localBL + vec3(planeWidth * nx, planeHeight * ny, 0.0f);

    // CALCULATE PIXEL COORDINATE IN WORLD SPACE
    vec3 worldPoint = cameraInfo.pos + cameraInfo.right * localPoint.x + cameraInfo.up * localPoint.y + cameraInfo.forward * localPoint.z;
    return worldPoint;
}

float Random(float min, float max, float seed) 
{
    float randomValue = fract(sin(seed * 12.9898 + 78.233) * 43758.5453);
    return min + randomValue * (max - min);
}

float RandomValueNormalDistribution(float seed)
{
    float theta = 2.0f * 3.1415926 * Random(0.0f, 1.0f, seed);
    float rho = sqrt(-2.0f * log(Random(0.0f, 1.0f, seed * 12435.0f)));
    return rho * cos(theta);
}

vec3 RandomDirection(float seed)
{
    float x = RandomValueNormalDistribution(seed);
    float y = RandomValueNormalDistribution(seed * 2323.0f);
    float z = RandomValueNormalDistribution(seed * 956856.0f);
    vec3 randomDir = vec3(x, y, z);
    return normalize(randomDir);
}

vec3 RandomHemisphereDirection(vec3 normal, float seed)
{
    vec3 randDir = RandomDirection(seed);
    if (dot(randDir, normal) < 0.0f) randDir = -randDir;
    return randDir;
}

float Distance(vec3 a, vec3 b)
{
    return sqrt(a.x * b.x + a.y * b.y + a.z * b.z);
}

vec3 InterpolateNormal(vec3 p, Vertex v1, Vertex v2, Vertex v3)
{
    vec3 a = vec3(v1.x, v1.y, v1.z);
    vec3 b = vec3(v2.x, v2.y, v2.z);
    vec3 c = vec3(v3.x, v3.y, v3.z);

    vec3 n1 = vec3(v1.nx, v1.ny, v1.nz);
    vec3 n2 = vec3(v2.nx, v2.ny, v2.nz);
    vec3 n3 = vec3(v3.nx, v3.ny, v3.nz);

    vec3 edgeAB = b - a;
    vec3 edgeAC = c - a;
    vec3 edgeBC = c - b;

    float area = length(cross(edgeAB, edgeAC));
    float area1 = length(cross(edgeAC, p - a)) / area;
    float area2 = length(cross(edgeBC, p - b)) / area;
    float area3 = 1.0 - area1 - area2;

    return normalize(n1 * area1 + n2 * area2 + n3 * area3);
}

RayHit RayTriangle(Ray ray, Vertex v1, Vertex v2, Vertex v3)
{
    vec3 edge1 = vec3(v2.x, v2.y, v2.z) - vec3(v1.x, v1.y, v1.z);
    vec3 edge2 = vec3(v3.x, v3.y, v3.z) - vec3(v1.x, v1.y, v1.z);

    vec3 p = cross(ray.dir, edge2);
    float determinant = dot(edge1, p);
    
    RayHit hit;
    hit.pos = vec3(0.0f, 0.0f, 0.0f);
    hit.normal = vec3(0.0f, 0.0f, 0.0f);
    hit.dist = 100000.0f;
    hit.hit = false;

    if (abs(determinant) < 0.000001f) 
    {
        return hit;
    }
    
    float inverseDeterminant = 1.0f / determinant;
    vec3 origin = ray.origin - vec3(v1.x, v1.y, v1.z);
    float u = dot(origin, p) * inverseDeterminant;

    if (u < 0.0f || u > 1.0f)
    {
        return hit;
    }

    vec3 q = cross(origin, edge1);
    float v = dot(ray.dir, q) * inverseDeterminant;

    if (v < 0.0f || u + v > 1.0f)
    {
        return hit;
    }

    float dist = dot(edge2, q) * inverseDeterminant;

    if (dist < 0.0f)
    {
        return hit;
    }
    hit.pos = ray.origin + ray.dir * dist;
    // hit.normal = InterpolateNormal(hit.pos, v1, v2, v3);
    hit.normal = normalize(cross(edge1, edge2));
    hit.dist = dist;
    hit.hit = true;   
    return hit;
}

RayHit CastRay(Ray ray)
{   
    // Default
    RayHit hit;
    hit.pos = vec3(0.0f, 0.0f, 0.0f);
    hit.normal = vec3(0.0f, 0.0f, 0.0f);
    hit.dist = 10000000.0f;
    hit.hit = false;

    // FOR EACH MESH
    for (int m = 0; m < u_meshCount; m++) {
        uint verticesStart = meshPartitions[m].verticesStart;
        uint indicesStart = meshPartitions[m].indicesStart;
        uint count = meshPartitions[m].indicesCount;

        for (int i = 0; i < count; i += 3) 
        {
            RayHit newHit = RayTriangle(ray, 
                vertices[verticesStart + indices[indicesStart + i]], 
                vertices[verticesStart + indices[indicesStart + i + 1]], 
                vertices[verticesStart + indices[indicesStart + i + 2]]);
            if (newHit.dist < hit.dist) hit = newHit;
        }
    }
    return hit;
}

vec3 PathTrace(Ray ray, int bounces, float seed)
{
    vec3 light = vec3(0.0f, 0.0f, 0.0f);
    vec3 rayColour = vec3(1.0f, 1.0f, 1.0f);

    for (int b=0; b<bounces+1; b++)
    {
        RayHit hit = CastRay(ray);

        if (hit.hit)
        {
            float cosineFactor = max(0.0f, dot(hit.normal, -ray.dir));
            light += rayColour * vec3(0.8f, 0.8f, 0.8f) * 0.0f;
            rayColour *= vec3(0.8f, 0.8f, 0.8f) * cosineFactor;

            vec3 diffuseDir = RandomHemisphereDirection(hit.normal, seed);
            vec3 specularDir = ray.dir - hit.normal * 2.0f * dot(ray.dir, hit.normal);

            ray.origin = hit.pos + hit.normal * 0.00001f; 
            ray.dir = normalize(diffuseDir * 0.9f + specularDir * (1.0f - 0.9f)); 
        }
        else
        {
            light += vec3(0.5f, 0.7f, 0.95f) * rayColour;
            break;
        }
    }
    return light;
}

vec3 ACES(vec3 colour)
{
    vec3 numerator = colour * (2.51 * colour + 0.03);
    vec3 denominator = colour * (2.43 * colour + 0.59) + 0.14;
    vec3 result = numerator / denominator;
    return clamp(result, 0.0, 1.0);
}

void main()
{   




    float width = imageSize(renderImage).x;
    float height = imageSize(renderImage).y;


    Ray camRay;
    camRay.origin = PixelRayPos(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y, width, height);
    camRay.dir = normalize(camRay.origin - cameraInfo.pos);

    float seed = float(gl_GlobalInvocationID.y ^ gl_GlobalInvocationID.y);
    vec3 colour = ACES(PathTrace(camRay, 2, seed));


    ivec2 pixelCoords = ivec2(gl_GlobalInvocationID.xy);
    imageStore(renderImage, pixelCoords, vec4(colour, 1.0f));  
}


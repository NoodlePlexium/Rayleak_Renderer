#version 440 core
#extension GL_ARB_bindless_texture : enable
#extension GL_NV_gpu_shader5 : enable

layout (local_size_x = 32, local_size_y = 32) in;
layout (binding = 0, rgba32f) uniform image2D renderImage;
layout (binding = 1, rgba8) uniform image2D displayImage;

struct Vertex
{
    vec3 pos;
    vec3 normal;
    float u, v;
};

struct Material
{
    vec3 colour;
    float roughness;
    float emission;
    float IOR;
    int refractive;

    uint64_t albedoHandle;
    uint64_t normalHandle;
    uint64_t roughnessHandle;
    uint textureFlags;
};

struct DirectionalLight
{
    vec3 direction;
    vec3 colour;
    float brightness;
};

struct PointLight
{
    vec3 position;
    vec3 colour;
    float brightness;
};

struct Spotlight
{
    vec3 position;
    vec3 direction;
    vec3 colour;
    float brightness;
    float angle;
    float falloff;
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

struct EmissiveTriangle
{
    uint index;
    uint materialIndex;
    float area;
    float weight;
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

struct LightPathOrigin
{
    vec3 position;
    vec3 dir;
    vec3 light;
};

struct PathVertex
{
    vec3 surfacePosition;
    vec3 surfaceNormal;
    vec3 surfaceColour;
    vec3 reflectedDir;
    vec3 outgoingLight;
    vec3 directLight;
    float surfaceRoughness;
    float surfaceEmission;
    float IOR;
    int refractive;
    int hitSky;
    int inside;
    int refracted;
    int cachedDirectLight;
};

layout(binding = 2) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(binding = 3) readonly buffer IndexBuffer {
    uint indices[];
};

layout(binding = 4) readonly buffer MaterialBuffer {
    Material materials[];
};

layout(binding = 5) readonly buffer BVHBuffer {
    BVH_Node bvhNodes[];
};

layout(binding = 6) readonly buffer PartitionBuffer {
    MeshPartition meshPartitions[];
};

layout(binding = 7) readonly buffer EmissiveBuffer {
    EmissiveTriangle emissiveTriangles[];
};

layout(binding = 8) readonly buffer DirectionalLightBuffer {
    DirectionalLight directionalLights[];
};

layout(binding = 9) readonly buffer PointLightBuffer {
    PointLight pointLights[];
};

layout(binding = 10) readonly buffer SpotlightLightBuffer {
    Spotlight spotlights[];
};

layout(binding = 11) buffer CameraPathVertexBuffer {
    PathVertex cameraPathVertices[];
};

layout(binding = 12) buffer LightPathVertexBuffer {
    PathVertex lightPathVertices[];
};

uniform uint u_tileX;
uniform uint u_tileY;
uniform CameraInfo cameraInfo;
uniform int u_meshCount;
uniform uint u_frameCount;
uniform uint u_accumulationFrame;
uniform uint u_debugMode;
uniform uint u_bounces;
uniform uint u_light_bounces;
uniform uint u_directionalLightCount;
uniform uint u_pointLightCount;
uniform uint u_spotlightCount;
uniform uint u_emissiveTriangleCount;
uniform float u_resolution_scale;



// FROM Sebastian Lague
// BY // www.pcg-random.org and www.shadertoy.com/view/XlGcRh
float Random(uint seed)
{
    seed = seed * 747796405 + 2891336453;
    uint result = ((seed >> ((seed >> 28) + 4)) ^ seed) * 277803737;
    result = (result >> 22) ^ result;
    return result / 4294967295.0;
}

float RandomValueNormalDistribution(uint seed)
{
    float theta = 2.0f * 3.1415926 * Random(seed);
    float rho = sqrt(-2.0f * log(Random(seed + 124284930)));
    return rho * cos(theta);
}

vec3 RandomDirection(uint seed)
{
    float x = RandomValueNormalDistribution(seed+100);
    float y = RandomValueNormalDistribution(seed+101);
    float z = RandomValueNormalDistribution(seed+102);
    vec3 randomDir = vec3(x, y, z);
    return normalize(randomDir);
}

vec3 RandomHemisphereDirection(vec3 normal, uint seed)
{
    vec3 randDir = RandomDirection(seed);
    if (dot(randDir, normal) < 0.0f) randDir = -randDir;
    return randDir;
}

vec3 RandomHemisphereDirectionCosine(vec3 normal, uint seed)
{
    float u1 = Random(seed+103);
    float u2 = Random(seed+104);
    float r = sqrt(u1);
    float theta = 6.2831853f * u2;
    float x = r * cos(theta);
    float y = r * sin(theta);
    vec3 randDir = vec3(x, y, sqrt(max(0.0f, 1.0f - u1)));
    if (dot(randDir, normal) < 0.0f) randDir = -randDir;
    return randDir;
}

vec3 RandomPointOnTriangle(uint v1_index, uint v2_index, uint v3_index, uint seed)
{
    Vertex v1 = vertices[indices[v1_index]];
    Vertex v2 = vertices[indices[v2_index]];
    Vertex v3 = vertices[indices[v3_index]];
    float a = Random(seed);
    float b = Random(seed+1);
    float rootA = sqrt(a);
    vec3 randomPoint = v1.pos * (1 - rootA) + v2.pos * (rootA * (1 - b)) + v3.pos * (b * rootA);
    return randomPoint;
}

// ADAPTED FROM USER Pommy https://stackoverflow.com/questions/5837572/generate-a-random-point-within-a-circle-uniformly
vec2 RandomPointInCircle(uint seed)
{
    float rho = Random(seed);
    float phi = Random(seed) * 6.2831853f;
    float x = rho * cos(phi);
    float y = rho * sin(phi);
    return vec2(x, y);
}

float DegreesToRadians(float degrees)
{
    return degrees * 0.01745329f;
}

float CosineInterpolation(float min, float max, float value)
{
    // CONVERT TO RADIANS
    float radians = ((value - min) / (max - min)) * 1.570796f; 
    return cos(radians);
}

vec3 PixelRayPos(uint x, uint y, uint width, uint height, uint seed, bool antiAliased)
{
    float FOV_Radians = DegreesToRadians(cameraInfo.FOV);
    float aspectRatio = float(width) / float(height);
    float nearPlane = 0.1f;
    
    // VIEWING PLANE
    float planeHeight = nearPlane * tan(FOV_Radians * 0.5f);
    float planeWidth = planeHeight * aspectRatio;

    // NORMALISED PIXEL COORDINATES
    float nx, ny;
    if (antiAliased)
    {
        vec2 randomCirclePoint = RandomPointInCircle(seed);
        nx = (x + randomCirclePoint.x) / (width - 1.0f);
        ny = (y + randomCirclePoint.y) / (height - 1.0f);
    }
    else
    {
        nx = (x) / (width - 1.0f);
        ny = (y) / (height - 1.0f);
    }

    // CALCULATE PIXEL COORDINATE IN PLANE SPACE
    vec3 localBL = vec3(-planeWidth * 0.5f, -planeHeight * 0.5f, nearPlane);
    vec3 localPoint = localBL + vec3(planeWidth * nx, planeHeight * ny, 0.0f);

    // CALCULATE PIXEL COORDINATE IN WORLD SPACE
    vec3 worldPoint = cameraInfo.pos + cameraInfo.right * localPoint.x + cameraInfo.up * localPoint.y + cameraInfo.forward * localPoint.z;
    return worldPoint;
}

vec3 BarycentricNormal(uint v1_index, uint v2_index, uint v3_index, vec3 pos, float area)
{
    const Vertex v1 = vertices[v1_index];
    const Vertex v2 = vertices[v2_index];
    const Vertex v3 = vertices[v3_index];

    vec3 v1v2 = v2.pos - v1.pos;
    vec3 v1v3 = v3.pos - v1.pos;
    vec3 v1pos = pos - v1.pos;
    
    float v1_pos_v2_area = length(cross(v1v2, v1pos)) * 0.5f;
    float v1_pos_v3_area = length(cross(v1v3, v1pos)) * 0.5f;

    float w = v1_pos_v2_area / area;
    float v = v1_pos_v3_area / area;
    float u = area - w - v;

    vec3 normal = normalize(v1.normal * u + v2.normal * v + v3.normal * w);
    return normal;
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

RayHit RayTriangle(Ray ray, uint v1_index, uint v2_index, uint v3_index)
{
    const Vertex v1 = vertices[v1_index];
    const Vertex v2 = vertices[v2_index];
    const Vertex v3 = vertices[v3_index];

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
    vec3 normal = normalize(v1.normal * w + v2.normal * u + v3.normal * v); // INTERPOLATE NORMAL USING BARYCENTRIC COORDINATES

    // SET HIT VALUES
    hit.frontFace = dot(ray.dir, normal) < 0.0f;
    hit.normal = hit.frontFace ? normal : -normal;
    hit.uv = vec2(v1.u, v1.v) * w + vec2(v2.u, v2.v) * u + vec2(v3.u, v3.v) * v;
    hit.dist = dist;
    hit.hit = true;
    return hit;
}

vec3 Refract(vec3 inDir, vec3 normal, float eta, float cosTheta)
{
    vec3 rPerp = eta * (inDir + cosTheta * normal);
    float rPerpSquared = dot(rPerp, rPerp);

    if (rPerpSquared > 1.0f) return vec3(0.0f, 0.0f, 0.0f);

    vec3 rParallel = -sqrt(1 - rPerpSquared) * normal;
    return rPerp + rParallel;
}

// FROM https://graphicscompendium.com/raytracing/11-fresnel-beer
float SchlicksReflectionProbability(vec3 inDir, vec3 normal, float IOR)
{
    float F0 = pow((1.0f - IOR) / (1.0f + IOR), 2.0f);
    return F0 + (1.0f - F0) * pow((1.0f - dot(normal, inDir)), 5.0f);
}

RayHit CastRay(Ray ray)
{   
    RayHit hit;
    hit.dist = 10000000.0f;
    hit.hit = false;

    mat4x4 inverseModelTransform;

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
                for (int i=0; i<node.indexCount; i+=3) 
                {
                    uint index = node.firstIndex + indicesStart + i;
                    uint v1_index = verticesStart + indices[index];
                    uint v2_index = verticesStart + indices[index + 1];
                    uint v3_index = verticesStart + indices[index + 2];
                    RayHit newHit = RayTriangle(
                        transformedRay, 
                        v1_index, 
                        v2_index, 
                        v3_index
                    );
                    if (newHit.dist < hit.dist) 
                    {
                        hit = newHit;
                        hit.materialIndex = meshPartitions[m].materialIndex;
                        inverseModelTransform = meshPartitions[m].inverseTransform;
                    }
                }
            }
        }
    }
    mat3 normalMatrix = transpose(mat3(inverseModelTransform));
    hit.normal = normalMatrix * hit.normal;
    hit.pos = (inverse(inverseModelTransform) * vec4(hit.pos, 1.0)).xyz;
    return hit;
}

bool ShadowCast(Ray ray, vec3 lightPos)
{
    float lightDist = length(lightPos - ray.origin);
    bool inShadow = false;
    
    // FOR EACH MESH
    for (int m=0; m<u_meshCount && !inShadow; m++) 
    {
        uint indicesStart = meshPartitions[m].indicesStart;
        uint verticesStart = meshPartitions[m].verticesStart;
        uint bvhStart = meshPartitions[m].bvhNodeStart;

        // TRANSFORM RAY TO BE IN MESH SPACE
        Ray transformedRay;
        transformedRay.origin = (meshPartitions[m].inverseTransform * vec4(ray.origin, 1.0)).xyz;
        transformedRay.dir = (meshPartitions[m].inverseTransform * vec4(ray.dir, 0.0)).xyz;

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
                
                if (leftBoxDist < lightDist) stack[++stackIndex] = node.leftChild + bvhStart;
                if (rightBoxDist < lightDist) stack[++stackIndex] = node.rightChild + bvhStart;
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
                    RayHit newHit = RayTriangle(
                        transformedRay, 
                        v1_index, 
                        v2_index, 
                        v3_index
                    );
                    
                    if (newHit.dist < lightDist) 
                    {
                        inShadow = true;
                        break;
                    }
                }

                if (inShadow)
                    break;
            }
        }
    }

    return inShadow;
}

vec3 DirectionalLightContribution(vec3 position, vec3 normal, float roughness, uint seed, bool subSample)
{
    uint bias = 1;
    vec3 light = vec3(0.0f, 0.0f, 0.0f);
    for (int d=0; d<u_directionalLightCount; d++)
    {
        float random = Random(seed + d * 34637334);
        bool sampleLight = subSample == false || u_directionalLightCount == 1 || random < 1.0f / float(u_directionalLightCount);
        if (subSample) bias = u_directionalLightCount;
        if (sampleLight)
        {
            // SKIP COMPUTATION IF SURFACE FACES AWAY FROM LIGHT
            if (dot(normal, -directionalLights[d].direction) < 0.0f)
            continue;

            Ray shadowRay;
            shadowRay.dir = -directionalLights[d].direction;
            shadowRay.origin = position + shadowRay.dir * 0.0001f;
            vec3 lightPosition = shadowRay.origin + shadowRay.dir * 5000.0f;
            bool inShadow = ShadowCast(shadowRay, lightPosition); 
            if (!inShadow)
            {
                // PERTURB THE SURFACE NORMAL BASED ON ROUGHNESS
                vec3 roughNormal = RandomHemisphereDirection(normal, seed + d * 23563456);
                vec3 surfaceNormal = normalize((1.0f - roughness) * normal + roughness * roughNormal);
                float surfaceCosineFactor = max(0.0f, dot(surfaceNormal, shadowRay.dir)); 

                light += surfaceCosineFactor * (directionalLights[d].colour * directionalLights[d].brightness);
            }
        }
    }
    return light * bias; // ACCOUNT FOR BIAS
}

vec3 PointLightContribution(vec3 position, vec3 normal, float roughness, uint seed, bool subSample)
{   
    uint bias = 1;
    vec3 totalLight = vec3(0.0f, 0.0f, 0.0f);
    for (int p=0; p<u_pointLightCount; p++)
    {
        float random = Random(seed + p * 95674354);
        bool sampleLight = subSample == false || u_pointLightCount == 1 || random < 1.0f / float(u_pointLightCount);
        if (subSample) bias = u_pointLightCount;
        if (sampleLight)
        {
            Ray shadowRay;
            shadowRay.dir = normalize(pointLights[p].position - position);  // Direction to the light
            
            // SKIP COMPUTATION IF SURFACE FACES AWAY FROM LIGHT
            if (dot(normal, shadowRay.dir) < 0.0f)
                continue;
    
                shadowRay.origin = position + shadowRay.dir * 0.0001f; 
            bool inShadow = ShadowCast(shadowRay, pointLights[p].position);
            if (!inShadow)
            {
                float lightDist = length(pointLights[p].position - shadowRay.origin);

                // PERTURB THE SURFACE NORMAL BASED ON ROUGHNESS
                vec3 roughNormal = RandomHemisphereDirection(normal, seed + p * 345768996);
                vec3 surfaceNormal = normalize((1.0f - roughness) * normal + roughness * roughNormal);
                float surfaceCosineFactor = max(0.0f, dot(surfaceNormal, shadowRay.dir)); 

                // Calculate the light contribution
                vec3 light = surfaceCosineFactor * (pointLights[p].colour * pointLights[p].brightness) / (lightDist * lightDist);
                totalLight += light;
            }
        }
    }
    return totalLight * bias; // ACCOUNT FOR BIAS
}

vec3 SpotlightContribution(vec3 position, vec3 normal, float roughness, uint seed, bool subSample)
{
    uint bias = 1;
    vec3 light = vec3(0.0f, 0.0f, 0.0f);
    for (int s=0; s<u_spotlightCount; s++)
    {
        float random = Random(seed + s * 5231912);
        bool sampleLight = subSample == false || u_spotlightCount == 1 || random < 1.0f / float(u_spotlightCount);
        if (subSample) bias = u_spotlightCount;
        if (sampleLight)
        {
            Ray shadowRay; 
            shadowRay.dir = normalize(spotlights[s].position - position);

            // SKIP COMPUTATION IF SURFACE FACES AWAY FROM LIGHT
            if (dot(normal, shadowRay.dir) < 0.0f)
            continue;

            shadowRay.origin = position + shadowRay.dir * 0.0001f;
            vec3 dirFromSpotlight = normalize(shadowRay.origin - spotlights[s].position);

            // ANGLE BETWEEN SPOTLIGHT DIRECTION AND DIRECTION OF LIGHT
            // RAY EMMITED FROM SPOTLIGHT TO SURFACE POINT
            float surfaceToSpotlightRadians = acos(dot(spotlights[s].direction, dirFromSpotlight));
            float spotlightAngleRadians = DegreesToRadians(spotlights[s].angle);
            float spotlightFalloffRadians = DegreesToRadians(spotlights[s].falloff);
            float spotlightMaxAngleRadians = spotlightAngleRadians + spotlightFalloffRadians;

            // SKIP SHADOW CAST IF SURFACE POINT NOT IN VISIBLE CONE
            if (surfaceToSpotlightRadians > spotlightMaxAngleRadians) continue;

            // SKIP IF IN SHADOW
            bool inShadow = ShadowCast(shadowRay, spotlights[s].position);
            if (inShadow) continue;

            // PERTURB THE SURFACE NORMAL BASED ON ROUGHNESS
            vec3 roughNormal = RandomHemisphereDirection(normal, seed + s * 54739845);
            vec3 surfaceNormal = normalize((1.0f - roughness) * normal + roughness * roughNormal);
            float surfaceCosineFactor = max(0.0f, dot(surfaceNormal, shadowRay.dir)); 

            // POINT INSIDE INNER CONE
            if (surfaceToSpotlightRadians < spotlightAngleRadians)
            {
                float lightDist = length(spotlights[s].position - shadowRay.origin);
                light += surfaceCosineFactor * (spotlights[s].colour * spotlights[s].brightness) / (lightDist * lightDist);
            }

            // POINT INSIDE CONE FALLOFF
            else if (surfaceToSpotlightRadians < spotlightMaxAngleRadians)
            {
                float lightDist = length(spotlights[s].position - shadowRay.origin);
                float fallOffFade = CosineInterpolation(spotlightAngleRadians, spotlightMaxAngleRadians, surfaceToSpotlightRadians);
                light += surfaceCosineFactor * fallOffFade * (spotlights[s].colour * spotlights[s].brightness) / (lightDist * lightDist);
            }
        }
    }
    return light * bias; // ACCOUNT FOR BIAS
}

// GENERATES A PATH FROM THE CAMERA AND STORES INFO IN PATHVERTEX BUFFER
int CameraPath(Ray ray, uint bounces, uint pixelIndex, uint seed)
{
    uint pathIndex = pixelIndex * (bounces+1);

    if (u_debugMode == 1) bounces = 0;

    // IF FIRST RAY SEGMENT IS CACHED 
    int cameraVertices = 0;

    for (uint b=0; b<bounces+1; b++)
    {
        RayHit hit = CastRay(ray);

        cameraPathVertices[pathIndex + b].inside = 0;
        cameraPathVertices[pathIndex + b].refracted = 0;
        cameraPathVertices[pathIndex + b].hitSky = 0;
        cameraPathVertices[pathIndex + b].cachedDirectLight = 0;

        if (hit.hit)
        {   
            const Material material = materials[hit.materialIndex];

            cameraPathVertices[pathIndex + b].surfacePosition = hit.pos;
            cameraPathVertices[pathIndex + b].surfaceNormal = hit.normal;

            // IF MATERIAL HAS AN ALBEDO TEXTURE
            if ((material.textureFlags & (1 << 0)) != 0) { 
                vec3 albedo = texture(sampler2D(material.albedoHandle), hit.uv).xyz;
                cameraPathVertices[pathIndex + b].surfaceColour = albedo * material.colour;
            }
            else {
                cameraPathVertices[pathIndex + b].surfaceColour = material.colour;
            }

            // IF MATERIAL HAS A ROUGHNESS TEXTURE
            if ((material.textureFlags & (1 << 2)) != 0) { 
                float roughness = texture(sampler2D(material.roughnessHandle), hit.uv).x;
                cameraPathVertices[pathIndex + b].surfaceRoughness = roughness;
            }
            else {
                cameraPathVertices[pathIndex + b].surfaceRoughness = material.roughness;
            }

            cameraPathVertices[pathIndex + b].surfaceEmission = material.emission;
            cameraPathVertices[pathIndex + b].refractive = material.refractive;
            cameraPathVertices[pathIndex + b].reflectedDir = -ray.dir;
            cameraPathVertices[pathIndex + b].inside = hit.frontFace ? 0 : 1;
            
            // PREPARE FOR NEXT BOUNCE
            bool refracted = false;
            if (material.refractive == 1)
            {
                float reflectProbability = SchlicksReflectionProbability(ray.dir, -hit.normal, material.IOR);
                float random = Random(seed + b + 534805);
                if (random > reflectProbability)
                {
                    float eta = hit.frontFace ? (1.0f / material.IOR) : material.IOR;
                    float cosTheta = min(dot(ray.dir, hit.normal), 1.0f);
                    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);
                    refracted = eta * sinTheta < 1.0f;

                    // REFRACT RAY
                    if (refracted)
                    {
                        float roughness = cameraPathVertices[pathIndex + b].surfaceRoughness;
                        cameraPathVertices[pathIndex + b].refracted = 1;
                        vec3 refractDir = Refract(-ray.dir, hit.normal, eta, cosTheta);
                        vec3 roughRefractDir = RandomHemisphereDirectionCosine(refractDir, seed + b);
                        ray.dir = normalize(roughRefractDir * roughness + refractDir * (1.0f - roughness));
                        ray.origin = hit.pos + ray.dir * 0.0001f;
                    }
                }
            }
            
            // REFLECT RAY
            if (!refracted)
            {
                float roughness = cameraPathVertices[pathIndex + b].surfaceRoughness;
                vec3 diffuseDir = RandomHemisphereDirectionCosine(hit.normal, seed + b);
                vec3 specularDir = ray.dir - hit.normal * 2.0f * dot(ray.dir, hit.normal);
                ray.dir = normalize(diffuseDir * roughness + specularDir * (1.0f - roughness)); 
                ray.origin = hit.pos + ray.dir * 0.0001f; 
            }
        }
        else
        {
            cameraPathVertices[pathIndex + b].hitSky = 1;
            break;
        }
        cameraVertices += 1;
    }
    return cameraVertices;
}

LightPathOrigin SampleLightPathOrigin(uint seed)
{
    LightPathOrigin lightPathOrigin;

    uint category;
    float multiplier = 0.0f;

    // ALL TYPES EXIST
    if (u_emissiveTriangleCount > 0 && u_pointLightCount > 0 && u_spotlightCount > 0) {
        float random = Random(seed + 1245677454) * 3;
        category = uint(floor(random));
        multiplier = 3.0f;
    }

    // ONLY EMISSIVE SURFACES EXIST
    else if (u_emissiveTriangleCount > 0 && u_pointLightCount == 0 && u_spotlightCount == 0) {
        category = 0;
        multiplier = 1.0f;
    }

    // ONLY POINT LIGHTS EXIST
    else if (u_pointLightCount > 0 && u_emissiveTriangleCount == 0 && u_spotlightCount == 0) {
        category = 1;
        multiplier = 1.0f;
    }

    // ONLY POINT LIGHTS EXIST
    else if (u_spotlightCount > 0 && u_pointLightCount == 0 && u_emissiveTriangleCount == 0) {
        category = 2;
        multiplier = 1.0f;
    }

    // SPOTLIGHTS DONT EXIST
    else if (u_emissiveTriangleCount > 0 && u_pointLightCount > 0 && u_spotlightCount == 0) {
        float random = Random(seed + 89456644) * 2;
        uint category = uint(floor(random));
        multiplier = 2.0f;
    }

    // POINT LIGHTS DONT EXIST
    else if (u_emissiveTriangleCount > 0 && u_spotlightCount > 0 && u_pointLightCount == 0) {
        float random = Random(seed + 12358567) * 2;
        if (random > 1) category = 0;
        else category = 2;
        multiplier = 2.0f;
    }

    // EMISSIVE SURFACES DONT EXIST
    else if (u_spotlightCount > 0 && u_pointLightCount > 0 && u_emissiveTriangleCount == 0) {
        float random = Random(seed + 8459934) * 2;
        if (random > 1) category = 1;
        else category = 2;
        multiplier = 2.0f;
    }

    // SAMPLE EMISSIVE SURFACE
    if (category == 0)
    {
        // CHOOSE AN EMISSIVE SURFACE 
        float randomTri = Random(seed + 498267613);
        uint triIndex = uint(floor(randomTri * u_emissiveTriangleCount));
        uint materialIndex = emissiveTriangles[triIndex].materialIndex;

        const float area = emissiveTriangles[triIndex].area;
        const uint v1_index = emissiveTriangles[triIndex].index;
        const uint v2_index = emissiveTriangles[triIndex].index+1;
        const uint v3_index = emissiveTriangles[triIndex].index+2;

        lightPathOrigin.light = materials[materialIndex].colour * materials[materialIndex].emission * emissiveTriangles[triIndex].weight;
        lightPathOrigin.position = RandomPointOnTriangle(v1_index, v2_index, v3_index, seed + 9361937);
        vec3 normal = BarycentricNormal(v1_index, v2_index, v3_index, lightPathOrigin.position, area);
        lightPathOrigin.dir = RandomHemisphereDirectionCosine(normal, seed + 27873283);
    } 

    // SAMPLE POINT LIGHT
    if (category == 1)
    {
        float randPointLight = Random(seed + 37728841);
        uint pointLightIndex = uint(floor(randPointLight * u_pointLightCount));

        lightPathOrigin.light = pointLights[pointLightIndex].colour * pointLights[pointLightIndex].brightness * u_pointLightCount;
        lightPathOrigin.position = pointLights[pointLightIndex].position;
        lightPathOrigin.dir = RandomDirection(seed + 199471728);
    }

    // SAMPLE SPOTLIGHT
    if (category == 2)
    {
        float randSpotlight = Random(seed + 777329197);
        uint spotlightIndex = uint(floor(randSpotlight * u_spotlightCount));

        lightPathOrigin.light = spotlights[spotlightIndex].colour * spotlights[spotlightIndex].brightness * u_spotlightCount;
        lightPathOrigin.position = spotlights[spotlightIndex].position;
        lightPathOrigin.dir = RandomDirection(seed + 119837223);
    }

    lightPathOrigin.light *= multiplier;
    return lightPathOrigin;
}

int LightPath(uint pixelIndex, uint seed)
{
    // SAMPLE A RANODM LIGHT SOURCE
    LightPathOrigin lightPathOrigin = SampleLightPathOrigin(seed);
    
    // VERTEX AT LIGHT SOURCE
    uint pathIndex = pixelIndex * (u_light_bounces+2);
    lightPathVertices[pathIndex].surfacePosition = lightPathOrigin.position;
    lightPathVertices[pathIndex].reflectedDir = lightPathOrigin.dir;
    lightPathVertices[pathIndex].outgoingLight = lightPathOrigin.light;

    // INITIALIZE RAY
    Ray ray;
    ray.dir = lightPathOrigin.dir;
    ray.origin = lightPathOrigin.position + ray.dir * 0.00001f;

    // COUNT VALID LIGHT VERTICES (TO EXCLUDE RAY MISSES)
    int lightVertices = 1;

    // BOUNCE LIGHT RAY THROUGH THE SCENE
    for (uint b=1; b<u_light_bounces+2; b++)
    {
        RayHit hit = CastRay(ray);

        // INITIALISE REFRACTION STATES
        lightPathVertices[pathIndex + b].inside = 0;
        lightPathVertices[pathIndex + b].refracted = 0;

        if (hit.hit)
        {   
            // RETRIEVE SURFACE MATERIAL
            const Material material = materials[hit.materialIndex];

            // SET LIGHT PATH VERTEX INFO
            lightPathVertices[pathIndex + b].surfacePosition = hit.pos;
            lightPathVertices[pathIndex + b].surfaceNormal = hit.normal;

            // IF MATERIAL HAS AN ALBEDO TEXTURE
            if ((material.textureFlags & (1 << 0)) != 0) { 
                vec3 albedo = texture(sampler2D(material.albedoHandle), hit.uv).xyz;
                lightPathVertices[pathIndex + b].surfaceColour = albedo * material.colour;
            }
            else {
                lightPathVertices[pathIndex + b].surfaceColour = material.colour;
            }

            // IF MATERIAL HAS A ROUGHNESS TEXTURE
            if ((material.textureFlags & (1 << 2)) != 0) { 
                float roughness = texture(sampler2D(material.roughnessHandle), hit.uv).x;
                lightPathVertices[pathIndex + b].surfaceRoughness = roughness;
            }
            else {
                lightPathVertices[pathIndex + b].surfaceRoughness = material.roughness;
            }

            lightPathVertices[pathIndex + b].surfaceEmission = material.emission;
            lightPathVertices[pathIndex + b].refractive = material.refractive;
            lightPathVertices[pathIndex + b].inside = hit.frontFace ? 0 : 1;

            // PREPARE FOR NEXT BOUNCE
            bool refracted = false;
            if (material.refractive == 1)
            {
                float reflectProbability = SchlicksReflectionProbability(ray.dir, -hit.normal, material.IOR);
                float random = Random(seed + b + 39400023);
                if (random > reflectProbability)
                {
                    float eta = hit.frontFace ? (1.0f / material.IOR) : material.IOR;
                    float cosTheta = min(dot(ray.dir, hit.normal), 1.0f);
                    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);
                    refracted = eta * sinTheta < 1.0f;

                    // REFRACT RAY
                    if (refracted)
                    {
                        const float roughness = cameraPathVertices[pathIndex + b].surfaceRoughness;
                        lightPathVertices[pathIndex + b].refracted = 1;
                        vec3 refractDir = Refract(-ray.dir, hit.normal, eta, cosTheta);
                        vec3 roughRefractDir = RandomHemisphereDirectionCosine(refractDir, seed + b + 382847772);
                        ray.dir = normalize(roughRefractDir * roughness + refractDir * (1.0f - roughness));
                        ray.origin = hit.pos + ray.dir * 0.0001f;
                    }
                }
            }

            // REFLECT RAY
            if (!refracted)
            {
                const float roughness = cameraPathVertices[pathIndex + b].surfaceRoughness;
                vec3 diffuseDir = RandomHemisphereDirectionCosine(hit.normal, seed + b + 90249938);
                vec3 specularDir = ray.dir - hit.normal * 2.0f * dot(ray.dir, hit.normal);
                ray.dir = normalize(diffuseDir * roughness + specularDir * (1.0f - roughness)); 
                ray.origin = hit.pos + ray.dir * 0.0001f; 
            }

            // SET LIGHT PATH REFLECTION DIRECTION
            lightPathVertices[pathIndex + b].reflectedDir = ray.dir;

            // INCREMENT USED LIGHT PATH VERTEX COUNTER
            lightVertices += 1;
        }
        else { break; }
    }

    // RETURN USED LIGHT VERTEX COUNT
    return lightVertices;
}


vec3 EvaluateBidirectional(int cameraVertices, int lightVertices, uint pixelIndex, uint seed)
{
    uint camPathIndex = pixelIndex * (u_bounces+1);
    uint lightPathIndex = pixelIndex * (u_light_bounces+2);

    vec3 light = vec3(0.0f, 0.0f, 0.0f);

    // HIT SKY - SKIP ALL COMPUTATION AND RETURN SKY COLOUR
    if (cameraPathVertices[camPathIndex].hitSky == 1)
    {
        return vec3(0.5f, 0.7f, 0.95f) * 3.5f;
    } 

    // EVALUATE LIGHT PATH ILLUMINATION AT EACH VERTEX
    for (int i=1; i<lightVertices; i++)
    {
        // GET LIGHT PATH VERTEX INFO
        const vec3 position = lightPathVertices[lightPathIndex + i].surfacePosition;
        const vec3 normal = lightPathVertices[lightPathIndex + i].surfaceNormal;
        const vec3 surfaceColour = lightPathVertices[lightPathIndex + i].surfaceColour;
        const vec3 reflectedDir = lightPathVertices[lightPathIndex + i].reflectedDir;
        const float surfaceRoughness = lightPathVertices[lightPathIndex + i].surfaceRoughness;
        const float surfaceEmission = lightPathVertices[lightPathIndex + i].surfaceEmission;
        const int inside = lightPathVertices[lightPathIndex + i].inside;
        const int refracted = lightPathVertices[lightPathIndex + i].refracted;
        const int cachedDirectLight = lightPathVertices[lightPathIndex + i].cachedDirectLight;

        float cosineFactor = max(0.0f, dot(normal, reflectedDir));
        if (refracted == 1 || inside == 1) cosineFactor = 1;

        // ACCUMULATE LIGHT
        vec3 indirectLight = lightPathVertices[lightPathIndex + i - 1].outgoingLight * surfaceColour * cosineFactor;
        vec3 emittedLight = surfaceColour * surfaceEmission;
    
        // LIGHT EMITTED AT LIGHT PATH VERTEX
        lightPathVertices[lightPathIndex + i].outgoingLight = indirectLight + emittedLight;
    }

    // EVALUATE LIGHTING
    for (int i=cameraVertices; i>=0; i--)
    {
        if (cameraPathVertices[camPathIndex + i].hitSky == 1)
        {
            light += vec3(0.5f, 0.7f, 0.95f) * 3.5f;
            continue;
        }

        // GET CAMERA PATH VERTEX INFO
        const vec3 position = cameraPathVertices[camPathIndex + i].surfacePosition;
        const vec3 normal = cameraPathVertices[camPathIndex + i].surfaceNormal;
        const vec3 surfaceColour = cameraPathVertices[camPathIndex + i].surfaceColour;
        const vec3 reflectedDir = cameraPathVertices[camPathIndex + i].reflectedDir;
        const float surfaceEmission = cameraPathVertices[camPathIndex + i].surfaceEmission;
        const float surfaceRoughness = cameraPathVertices[camPathIndex + i].surfaceRoughness;
        const int inside = cameraPathVertices[camPathIndex + i].inside;
        const int refractive = cameraPathVertices[camPathIndex + i].refractive;
        const int refracted = cameraPathVertices[camPathIndex + i].refracted;


        
        vec3 lightPathIllumination = vec3(0.0f, 0.0f, 0.0f);
        for (int i=1; i<lightVertices; i++)
        {
            uint index = lightPathIndex + i;
        
            Ray shadowRay;
            shadowRay.dir = normalize(position - lightPathVertices[index].surfacePosition);
            shadowRay.origin = lightPathVertices[index].surfacePosition + shadowRay.dir * 0.00001f;

            if (!ShadowCast(shadowRay, position - shadowRay.dir * 0.0001f))
            {
                float lightPathVertexDist = length(lightPathVertices[index].surfacePosition - position);
                float lightPathVertexDistSquared = lightPathVertexDist * lightPathVertexDist;
                vec3 dirFromCamSurfaceToLightSurface = normalize(lightPathVertices[index].surfacePosition - position);

                // INFO FROM LIGHT PATH VERTEX
                const vec3 lightPos = lightPathVertices[index].surfacePosition;
                const vec3 prevLightPos = lightPathVertices[index - 1].surfacePosition;
                const vec3 lightNormal = lightPathVertices[index].surfaceNormal;
                const float lightPointRoughness = lightPathVertices[index].surfaceRoughness;


                // GET MIRROR REFLECTION DIR
                const vec3 lightIncomingDir = normalize(lightPos - prevLightPos);
                vec3 lightMirrorReflectDir = lightIncomingDir - lightNormal * 2.0f * dot(lightIncomingDir, lightNormal);
                vec3 rNormParallel = dot(lightMirrorReflectDir, lightNormal) * lightNormal;
                vec3 rProjection = lightMirrorReflectDir - rNormParallel;
                vec3 maxOppositeDir = normalize(-rProjection);
                vec3 maxRoughnessReflectDir = normalize(lightPointRoughness * maxOppositeDir + (1 - lightPointRoughness) * lightMirrorReflectDir);
                float reflectSpreadAngle = acos(dot(maxRoughnessReflectDir, lightMirrorReflectDir));
                vec3 lightToCamDir = normalize(position - lightPos);
                float mirror_light_cam_angle = acos(dot(lightToCamDir, lightMirrorReflectDir));

                // IF CAMERA SURFACE POINT FALLS WITHIN LIGHT REFLECTION CONE
                if (mirror_light_cam_angle < reflectSpreadAngle)
                {
                    // ATTENUATION BASED ON DISTANCE AND REFLECTION ANGLE RANGE DUE TO ROUGHNESS
                    float lightDist = length(position - lightPos);
                    float conalSurfaceArea = 3.14159f * lightDist * lightDist * tan(reflectSpreadAngle * 2.0) * tan(reflectSpreadAngle * 2.0);
                    float lightDensity = 1.0 / (conalSurfaceArea + 1.0);
                    
                    
                    vec3 dirToLight = normalize(lightPos - position);
                    float cosFactor = max(0.0f, dot(normal, dirToLight)); 

                    lightPathIllumination += cosFactor * (lightPathVertices[index].outgoingLight) * lightDensity;
                }
            }
        }

        // SAMPLE DIRECT LIGHT SOURCE
        vec3 directLight = vec3(0.0f, 0.0f, 0.0f);
        if (inside == 0 && refracted == 0)
        {
            directLight += DirectionalLightContribution(position, normal, surfaceRoughness, seed + i * 10, true);
            directLight += PointLightContribution(position, normal, surfaceRoughness, seed + i * 10, true);
            directLight += SpotlightContribution(position, normal, surfaceRoughness, seed + i * 10, true);
        }

        float cosineFactor = max(0.0f, dot(normal, reflectedDir));
        if (refracted == 1 || inside == 1) cosineFactor = 1;

        vec3 indirectLight = light * surfaceColour * cosineFactor;
        vec3 emittedLight = surfaceColour * surfaceEmission;
        light = indirectLight + directLight * surfaceColour + emittedLight + lightPathIllumination * surfaceColour;
    }

    return light;
}

vec3 EvaluatePath(int segments, uint bounces, uint pixelIndex, uint seed)
{
    uint pathIndex = pixelIndex * (bounces+1);
    vec3 light = vec3(0.0f, 0.0f, 0.0f);

    // EVALUATE LIGHTING
    for (int i=segments; i>=0; i--)
    {
        if (cameraPathVertices[pathIndex + i].hitSky == 1)
        {
            light += vec3(0.5f, 0.7f, 0.95f) * 2.0f;
            continue;
        }

        // GET CAMERA PATH VERTEX INFO
        const vec3 position = cameraPathVertices[pathIndex + i].surfacePosition;
        const vec3 normal = cameraPathVertices[pathIndex + i].surfaceNormal;
        const vec3 surfaceColour = cameraPathVertices[pathIndex + i].surfaceColour;
        const vec3 reflectedDir = cameraPathVertices[pathIndex + i].reflectedDir;
        const float surfaceEmission = cameraPathVertices[pathIndex + i].surfaceEmission;
        const float surfaceRoughness = cameraPathVertices[pathIndex + i].surfaceRoughness;
        const int inside = cameraPathVertices[pathIndex + i].inside;
        const int refractive = cameraPathVertices[pathIndex + i].refractive;
        const int refracted = cameraPathVertices[pathIndex + i].refracted;

        float cosineFactor = max(0.0f, dot(normal, reflectedDir));
        if (refracted == 1 || inside == 1) cosineFactor = 1;

        vec3 directLight = vec3(0.0f, 0.0f, 0.0f);

        if (inside == 0 && refracted == 0)
        {
            directLight += DirectionalLightContribution(position, normal, surfaceRoughness, seed + i * 10, true);
            directLight += PointLightContribution(position, normal, surfaceRoughness, seed + i * 10, true);
            directLight += SpotlightContribution(position, normal, surfaceRoughness, seed + i * 10, true);
        }

        vec3 indirectLight = light * surfaceColour * cosineFactor;
        vec3 emittedLight = surfaceColour * surfaceEmission;
        light = indirectLight + directLight * surfaceColour + emittedLight;
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
    uint width = imageSize(renderImage).x;
    uint height = imageSize(renderImage).y;

    // GET PIXEL INDEX IN FLATTENED IMAGE COORDINATES
    uint pX = gl_GlobalInvocationID.x + 32 * u_tileX;
    uint pY = gl_GlobalInvocationID.y + 32 * u_tileY; 
    uint pixelIndex = pY * width + pX;

    // EXIT EARLY IF PIXEL IS NOT VISIBLE
    if (pX > imageSize(renderImage).x || pY > imageSize(renderImage).y)
    return;

    // GENERATE A PSEUDORANDOM SEED
    uint seed = u_frameCount * width * height + pixelIndex;

    // CREATE CAMERA RAY FOR THIS PIXEL
    Ray camRay;
    camRay.origin = PixelRayPos(pX, pY, width, height, seed + 313874256, cameraInfo.antiAliasing == 1);
    camRay.dir = normalize(camRay.origin - cameraInfo.pos);

    // DEPTH OF FIELD
    if (cameraInfo.DOF == 1)
    {
        float ratio = dot(cameraInfo.forward, camRay.dir);
        float inverseRatio = 1 / ratio;
        vec3 shortCUP = cameraInfo.pos + cameraInfo.forward * ratio;
        vec3 orthogonal = (camRay.origin + camRay.dir) - shortCUP;
        vec3 unitFocalPoint = cameraInfo.pos + cameraInfo.forward + orthogonal * inverseRatio;
        vec3 focalPoint = cameraInfo.pos + cameraInfo.forward * cameraInfo.focusDistance + orthogonal * inverseRatio * cameraInfo.focusDistance;

        vec2 randCirclePos = RandomPointInCircle(seed + 82194872) * cameraInfo.aperture;
        camRay.origin += cameraInfo.right * randCirclePos.x + cameraInfo.up * randCirclePos.y;
        camRay.dir = normalize(focalPoint - camRay.origin);
    }

    
    // TRACE CAMERA TO GET PIXEL COLOUR
    int cameraSegments = CameraPath(camRay, u_bounces, pixelIndex, seed);
    vec3 colour = EvaluatePath(cameraSegments, u_bounces, pixelIndex, seed) * cameraInfo.exposure;

    // int lightVertices = LightPath(pixelIndex, seed);
    // int cameraVertices = CameraPath(camRay, u_bounces, pixelIndex, seed);
    // vec3 colour = EvaluateBidirectional(cameraVertices, lightVertices, pixelIndex, seed);

    // FRAME ACCUMULATION
    vec4 oldAvg = imageLoad(renderImage, ivec2(pX, pY)); 
    vec4 newAvg = ((oldAvg * u_accumulationFrame) + vec4(colour.xyz, 1.0f)) / (u_accumulationFrame + 1);
    imageStore(renderImage, ivec2(pX, pY), vec4(newAvg.xyz, 1.0f));   

    // SET DISPLAY IMAGE PIXEL
    vec3 outputColour = ACES(newAvg.xyz);
    imageStore(displayImage, ivec2(pX, pY), vec4(outputColour.xyz, 1.0f));  
}


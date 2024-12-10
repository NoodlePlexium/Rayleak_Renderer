#version 440 core

layout (local_size_x = 1, local_size_y = 1) in;
layout (binding = 0) uniform writeonly image2D renderImage;

struct CameraInfo{
    vec3 pos;
    vec3 forward;
    vec3 right;
    vec3 up;
    float FOV;
};
uniform CameraInfo cameraInfo;


struct Ray
{
    vec3 origin;
    vec3 dir;
};

struct RayHit
{
    vec3 pos;
    vec3 normal;
    bool hit;
};

struct Triangle
{   
    vec3 a, b, c;
};

RayHit RayTriangle(Ray ray, Triangle tri)
{
    vec3 edge1 = tri.b - tri.a;
    vec3 edge2 = tri.c - tri.a;

    vec3 p = cross(ray.dir, edge2);
    float determinant = dot(edge1, p);

    if (abs(determinant) < 0.000001f) 
    {
        RayHit noHit;
        noHit.pos = vec3(0.0f, 0.0f, 0.0f);
        noHit.normal = vec3(0.0f, 0.0f, 0.0f);
        noHit.hit = false;
        return noHit;
    }
    
    float inverseDeterminant = 1.0f / determinant;
    vec3 origin = ray.origin - tri.a;
    float u = dot(origin, p) * inverseDeterminant;

    if (u < 0.0f || u > 1.0f)
    {
        RayHit noHit;
        noHit.pos = vec3(0.0f, 0.0f, 0.0f);
        noHit.normal = vec3(0.0f, 0.0f, 0.0f);
        noHit.hit = false;
        return noHit;
    }

    vec3 q = cross(origin, edge1);
    float v = dot(ray.dir, q) * inverseDeterminant;

    if (v < 0.0f || u + v > 1.0f)
    {
        RayHit noHit;
        noHit.pos = vec3(0.0f, 0.0f, 0.0f);
        noHit.normal = vec3(0.0f, 0.0f, 0.0f);
        noHit.hit = false;
        return noHit;
    }

    float dist = dot(edge2, q) * inverseDeterminant;

    if (dist < 0.0f)
    {
        RayHit noHit;
        noHit.pos = vec3(0.0f, 0.0f, 0.0f);
        noHit.normal = vec3(0.0f, 0.0f, 0.0f);
        noHit.hit = false;
        return noHit;
    }

    RayHit hit;
    hit.pos = ray.origin + ray.dir * dist;
    hit.normal = normalize(cross(edge1, edge2)); 
    hit.hit = true;   
    return hit;
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

    vec3 localBL = vec3(-planeWidth * 0.5f, -planeHeight * 0.5f, nearPlane);
    vec3 localPoint = localBL + vec3(planeWidth * nx, planeHeight * ny, 0.0f);

    // CALCULATE PIXEL COORDINATE IN WORLD SPACE
    vec3 worldPoint = cameraInfo.pos + cameraInfo.right * localPoint.x + cameraInfo.up * localPoint.y + cameraInfo.forward * localPoint.z;
    return worldPoint;
}

void main()
{   


    vec3 colour = vec3(0.5f, 0.2f, 0.8f);

    float width = imageSize(renderImage).x;
    float height = imageSize(renderImage).y;


    Ray camRay;
    camRay.origin = PixelRayPos(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y, width, height);
    camRay.dir = normalize(camRay.origin - cameraInfo.pos);

    Triangle tri;
    tri.a = vec3(-0.5f, -0.5f, 2.0f);
    tri.b = vec3(0.0f, 0.5f, 2.0f);
    tri.c = vec3(0.5f, -0.5f, 2.0f);

    RayHit hit = RayTriangle(camRay, tri);
    if (hit.hit)
    {
        colour = vec3(0.4f, 0.9f, 0.4f);
    }

    ivec2 pixelCoords = ivec2(gl_GlobalInvocationID.xy);
    imageStore(renderImage, pixelCoords, vec4(colour, 1.0f));  
}


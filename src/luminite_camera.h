#pragma once
#include <GL/glew.h>
#include "./glm/glm.hpp"

class Camera
{
public:

    Camera(unsigned int _pathtraceShader) : pathtraceShader(_pathtraceShader)
    {
        pos     = glm::vec3(0.0f, 0.5f, -4.7f);
        forward = glm::vec3(0.0f, 0.0f, 1.0f);
        right   = glm::vec3(1.0f, 0.0f, 0.0f);
        up      = glm::vec3(0.0f, 1.0f, 0.0f);
        FOV = 85.0f;
    }

    void UpdatePathtracerUniforms()
    {
        glUniform3f(camInfoPosLocation, pos.x, pos.y, pos.z);
        glUniform3f(camInfoForwardLocation, forward.x, forward.y, forward.z);
        glUniform3f(camInfoRightLocation, right.x, right.y, right.z);
        glUniform3f(camInfoUpLocation, up.x, up.y, up.z);
        glUniform1f(camInfoFOVLocation, FOV);
    }

    glm::vec3 pos;
    glm::vec3 forward;
    glm::vec3 right;
    glm::vec3 up;
    float FOV;
    

private:
    unsigned int pathtraceShader;
    unsigned int camInfoPosLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.pos");
    unsigned int camInfoForwardLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.forward");
    unsigned int camInfoRightLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.right");
    unsigned int camInfoUpLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.up");
    unsigned int camInfoFOVLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.FOV");
};
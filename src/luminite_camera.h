#pragma once
#include <GL/glew.h>
#include "./glm/glm.hpp"

class Camera
{
public:

    Camera(unsigned int _pathtraceShader) : pathtraceShader(_pathtraceShader)
    {
        pos      = glm::vec3(0.0f, 0.5f, -4.7f);
        rotation = glm::vec3(0.0f, 0.0f, 0.0f);
        forward  = glm::vec3(0.0f, 0.0f, -1.0f);
        right    = glm::vec3(1.0f, 0.0f, 0.0f);
        up       = glm::vec3(0.0f, 1.0f, 0.0f);
        FOV = 85.0f;
    }

    void UpdatePathtracerUniforms()
    {
        UpdateCameraVectors();
        glUniform3f(camInfoPosLocation, pos.x, pos.y, pos.z);
        glUniform3f(camInfoForwardLocation, forward.x, forward.y, forward.z);
        glUniform3f(camInfoRightLocation, right.x, right.y, right.z);
        glUniform3f(camInfoUpLocation, up.x, up.y, up.z);
        glUniform1f(camInfoFOVLocation, FOV);
    }

    void UpdateCameraVectors() {
        // Convert degrees to radians
        float pitch = glm::radians(rotation.x); // Rotation around X-axis
        float yaw = glm::radians(rotation.y);   // Rotation around Y-axis

        // Calculate new forward vector
        forward = -glm::normalize(glm::vec3(
            cos(yaw) * cos(pitch),
            sin(pitch),
            sin(yaw) * cos(pitch)
        ));

        // Calculate right vector (perpendicular to forward & world up)
        right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

        // Calculate up vector (perpendicular to forward & right)
        up = glm::normalize(glm::cross(right, forward));
    }

    glm::vec3 pos;
    glm::vec3 rotation;

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
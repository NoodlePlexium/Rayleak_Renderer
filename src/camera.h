#pragma once
#include <GL/glew.h>
#include "./glm/glm.hpp"

class Camera
{
public:

    Camera(unsigned int _pathtraceShader) : pathtraceShader(_pathtraceShader)
    {
        pos      = glm::vec3(0.0f, 0.5f, -4.7f);
        rotation = glm::vec3(0.0f, -90.0f, 0.0f);
        forward  = glm::vec3(0.0f, 0.0f, -1.0f);
        right    = glm::vec3(1.0f, 0.0f, 0.0f);
        up       = glm::vec3(0.0f, 1.0f, 0.0f);
        fov = 90.0f;
        dof = false;
        focus_distance = 10.0f;
        fStop = 2.8f;
        anti_aliasing = true;
        exposure = 1.0f;
    }

    void UpdatePathtracerUniforms()
    {
        UpdateCameraVectors();
        glUniform3f(camInfoPosLocation, pos.x, pos.y, pos.z);
        glUniform3f(camInfoForwardLocation, forward.x, forward.y, forward.z);
        glUniform3f(camInfoRightLocation, right.x, right.y, right.z);
        glUniform3f(camInfoUpLocation, up.x, up.y, up.z);
        glUniform1f(camInfoFOVLocation, fov);
        glUniform1ui(cameraInfoDOFLocation, dof? 1 : 0);
        glUniform1f(cameraInfoFocusDistanceLocation, focus_distance);
        float aperture = (1 / fov) / fStop; 
        glUniform1f(cameraInfoApertureLocation, aperture);
        glUniform1ui(cameraInfoAntiAliasingLocation, anti_aliasing);
        glUniform1f(cameraInfoExposureLocation, exposure);
    }

    void UpdateRaycasterUniforms(unsigned int shader)
    {
        unsigned int camInfoPosLocation_raycast = glGetUniformLocation(shader, "cameraInfo.pos");
        unsigned int camInfoForwardLocation_raycast = glGetUniformLocation(shader, "cameraInfo.forward");
        unsigned int camInfoRightLocation_raycast = glGetUniformLocation(shader, "cameraInfo.right");
        unsigned int camInfoUpLocation_raycast= glGetUniformLocation(shader, "cameraInfo.up");
        unsigned int camInfoFOVLocation_raycast = glGetUniformLocation(shader, "cameraInfo.FOV");
        unsigned int cameraInfoDOFLocation_raycast = glGetUniformLocation(shader, "cameraInfo.DOF");
        unsigned int cameraInfoFocusDistanceLocation_raycast = glGetUniformLocation(shader, "cameraInfo.focusDistance");
        unsigned int cameraInfoApertureLocation_raycast = glGetUniformLocation(shader, "cameraInfo.aperture");
        unsigned int cameraInfoAntiAliasingLocation_raycast = glGetUniformLocation(shader, "cameraInfo.antiAliasing");
        unsigned int cameraInfoExposureLocation_raycast = glGetUniformLocation(shader, "cameraInfo.exposure");

        UpdateCameraVectors();
        glUniform3f(camInfoPosLocation_raycast, pos.x, pos.y, pos.z);
        glUniform3f(camInfoForwardLocation_raycast, forward.x, forward.y, forward.z);
        glUniform3f(camInfoRightLocation_raycast, right.x, right.y, right.z);
        glUniform3f(camInfoUpLocation_raycast, up.x, up.y, up.z);
        glUniform1f(camInfoFOVLocation_raycast, fov);
        glUniform1ui(cameraInfoDOFLocation_raycast, dof? 1 : 0);
        glUniform1f(cameraInfoFocusDistanceLocation_raycast, focus_distance);
        float aperture = (1 / fov) / fStop; 
        glUniform1f(cameraInfoApertureLocation_raycast, aperture);
        glUniform1ui(cameraInfoAntiAliasingLocation_raycast, anti_aliasing);
        glUniform1f(cameraInfoExposureLocation_raycast, exposure);
    }
    
    void UpdateCameraVectors()
    {
        float rotationX = glm::radians(rotation.x);
        float rotationY = glm::radians(rotation.y - 90);
        float rotationZ = glm::radians(rotation.z);

        // ROTATION MATRICES FROM USER legends2k https://stackoverflow.com/questions/14607640/rotating-a-vector-in-3d-space
        glm::mat3x3 xRot
        {
            1, 0, 0,
            0, std::cos(rotationX), -std::sin(rotationX),
            0, std::sin(rotationX), std::cos(rotationX)
        };

        glm::mat3x3 yRot
        {
            std::cos(rotationY), 0, std::sin(rotationY),
            0, 1, 0,
            -std::sin(rotationY), 0, std::cos(rotationY)
        };

        glm::mat3x3 zRot
        {
            std::cos(rotationZ), -std::sin(rotationZ), 0,
            std::sin(rotationZ), std::cos(rotationZ), 0,
            0, 0, 1
        };

        glm::mat3x3 rotationMat = zRot * yRot * xRot;

        forward = rotationMat * glm::vec3(0, 0, -1);
        up = zRot * rotationMat * glm::vec3(0, 1, 0);
        right = zRot * rotationMat * glm::vec3(1, 0, 0);
    }

    // TRANSFORM
    glm::vec3 pos;
    glm::vec3 rotation;
    glm::vec3 forward;
    glm::vec3 right;
    glm::vec3 up;

    // SETTINGS
    float fov;
    float focus_distance;
    float fStop;
    float exposure;
    bool  dof;
    bool  anti_aliasing;

    // PREVIOUS SETTINGS
    float prev_fov;
    float prev_focus_distance;
    float prev_fStop;


private:
    unsigned int pathtraceShader;
    unsigned int camInfoPosLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.pos");
    unsigned int camInfoForwardLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.forward");
    unsigned int camInfoRightLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.right");
    unsigned int camInfoUpLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.up");
    unsigned int camInfoFOVLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.FOV");
    unsigned int cameraInfoDOFLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.DOF");
    unsigned int cameraInfoFocusDistanceLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.focusDistance");
    unsigned int cameraInfoApertureLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.aperture");
    unsigned int cameraInfoAntiAliasingLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.antiAliasing");
    unsigned int cameraInfoExposureLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.exposure");
};


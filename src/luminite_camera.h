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

    bool SettingsChanged()
    {
        bool changed = false;
        changed = changed | (fov != prev_fov);
        changed = changed | (focus_distance != prev_focus_distance);
        changed = changed | (fStop != prev_fStop);
        changed = changed | (exposure != prev_exposure);
        changed = changed | (dof != prev_dof);
        changed = changed | (anti_aliasing != prev_anti_aliasing);
        prev_fov = fov;
        prev_focus_distance = focus_distance;
        prev_fStop = fStop;
        prev_exposure = exposure;
        prev_dof = dof;
        prev_anti_aliasing = anti_aliasing;
        return changed;
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
    float prev_exposure;
    bool  prev_dof;
    bool  prev_anti_aliasing;


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
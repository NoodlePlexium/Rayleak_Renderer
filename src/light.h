#pragma once
#include "../lib/glm/glm.hpp"

struct DirectionalLight
{
    alignas(16) glm::vec3 direction;
    alignas(16) glm::vec3 rotation;
    alignas(16) glm::vec3 colour;
    float brightness;

    DirectionalLight()
    {
        direction = glm::vec3(0.0f, -1.0f, 0.0f);
        rotation = glm::vec3(0.0f, 0.0f, 0.0f);
        colour = glm::vec3(1, 1, 1);
        brightness = 1.0f;
    }

    void TransformDirection()
    {
        float rotationX = glm::radians(rotation.x);
        float rotationY = glm::radians(rotation.y + 180);
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
        direction = rotationMat * glm::vec3(0, -1, 0);
    }
};

struct PointLight
{
    alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 colour;
    float brightness;

    PointLight()
    {
        position = glm::vec3(0.0f, 2.0f, 0.0f);
        colour = glm::vec3(1.0f, 1.0f, 1.0f);
        brightness = 1.0f;
    }
};

struct Spotlight
{
    alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 rotation;
    alignas(16) glm::vec3 direction;
    alignas(16) glm::vec3 colour;
    float brightness;
    float angle;
    float falloff;

    Spotlight()
    {
        position = glm::vec3(0.0f, 2.0f, 0.0f);
        rotation = glm::vec3(0.0f, 0.0f, 0.0f);
        direction = glm::vec3(0.0f, -1.0f, 0.0f);
        colour = glm::vec3(1.0f, 1.0f, 1.0f);
        brightness = 2.0f;
        angle = 12.0f;
        falloff = 10.0f;
    }

    void TransformDirection()
    {
        float rotationX = glm::radians(rotation.x);
        float rotationY = glm::radians(rotation.y + 180);
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
        direction = rotationMat * glm::vec3(0, -1, 0);
    }
};


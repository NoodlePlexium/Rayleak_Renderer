#pragma once
#include "glm/glm.hpp"

struct DirectionalLight
{
    alignas(16) glm::vec3 direction;
    alignas(16) glm::vec3 colour;
    float brightness;

    DirectionalLight()
    {
        direction = glm::vec3(0.0f, -1.0f, 0.0f);
        colour = glm::vec3(0.96f, 0.94f, 0.92f);
        brightness = 0.5f;
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
    alignas(16) glm::vec3 direction;
    alignas(16) glm::vec3 colour;
    float brightness;
    float angle;
    float falloff;

    Spotlight()
    {
        position = glm::vec3(0.0f, 0.0f, 0.0f);
        direction = glm::vec3(0.0f, -1.0f, 0.0f);
        brightness = 2.0f;
        angle = 12.0f;
        falloff = 10.0f;
    }
};
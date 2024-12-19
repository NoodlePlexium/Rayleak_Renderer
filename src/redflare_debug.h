#pragma once
#include <GL/glew.h>
#include <iostream>
#include <chrono>

void ClearGLErrors()
{
    GLenum error;
    while ((error = glGetError()) != GL_NO_ERROR) {}
}

void PrintGLErrors()
{
    GLenum error;
    while ((error = glGetError()) != GL_NO_ERROR) {
        std::cout << "OpenGL Error: " << error << std::endl;
    }
}

namespace Debug
{
    double processTime;
    double accumulatedTime;
    std::chrono::time_point<std::chrono::high_resolution_clock> start;
    std::chrono::time_point<std::chrono::high_resolution_clock> startAccum;


    void StartTimer()
    {
        start = std::chrono::high_resolution_clock::now();
    }

    void EndTimer()
    {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "Duration: " << duration.count() << " microseconds" << std::endl;
    }

    void ResetAccum()
    {
        accumulatedTime = 0;
    }

    void ResumeAccum()
    {
        startAccum = std::chrono::high_resolution_clock::now();
    }

    void StopAccum()
    {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - startAccum);
        accumulatedTime += duration.count();
    }

    void PrintAccum()
    {
        std::cout << "Accumulated time: " << accumulatedTime / 1e6 << "ms" << std::endl;
    }
};
#pragma once
#include <GL/glew.h>
#include <iostream>

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
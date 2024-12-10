#pragma once
#include <GL/glew.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

int GetShaderProgram()
{
    int program;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    return program;
}

std::string LoadShaderFromFile(const std::string &filepath)
{
    std::ifstream stream(filepath);
    if (!stream.is_open())
    {
        throw std::runtime_error("[LoadShaderFromFile] <Error> Failed to locate shader file \"" + filepath + "\"");
    }
    std::stringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

unsigned int CompileShader(unsigned int type, const std::string &source)
{
    unsigned int id = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(id, 1, &src, NULL);
    glCompileShader(id);

    int result;
    glGetShaderiv(id, GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE)
    {
        int length;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
        char* errorMessage = (char*)alloca(length * sizeof(char));
        glGetShaderInfoLog(id, length, &length, errorMessage);
        std::cout << "[CompileShader] <Shader Compile Error> " << errorMessage << "\n";
        glDeleteShader(id);
        return 0;
    }
    return id;
}

unsigned int CreateRasterShader(const std::string& vertexShaderSource, const std:: string& fragmentShaderSource)
{
    unsigned int vertexShader = CompileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glValidateProgram(shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return shaderProgram;
}

unsigned int CreateComputeShader(const std::string& computeShaderSource)
{
    unsigned int computeShader = CompileShader(GL_COMPUTE_SHADER, computeShaderSource);
    unsigned int computeShaderProgram = glCreateProgram();
    glAttachShader(computeShaderProgram, computeShader);
    glLinkProgram(computeShaderProgram);
    return computeShaderProgram;
}

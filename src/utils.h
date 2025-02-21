#pragma once

#include <string>
#include <functional>

std::string ExtractName(std::string filepath)
{
    int startIndex = -1;
    for (int i=filepath.size()-2; i>=0; i--)
    {
        if (filepath[i] == '/' || filepath[i] == '\\')
        {
            startIndex = i+1;
            break;
        }
    }
    return filepath.substr(startIndex, std::min(filepath.size() - startIndex - 4, (size_t)64));
}
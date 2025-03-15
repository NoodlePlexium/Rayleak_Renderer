#pragma once 

// EXTERNAL LIBRARIES
#include <GL/glew.h>
#include "glm/gtc/type_ptr.hpp"

// STANDARD LIBRARY
#include <vector>
#include <string>
#include <algorithm> 

// PROJECT HEADERS
#include "mesh.h"
#include "model_manager.h"
#include "light.h"
#include "debug.h"

class DynamicPoolBuffer
{
public: 

    struct ItemPartition
    {
        uint32_t start;
        uint32_t size;
        uint32_t id;
    };

    DynamicPoolBuffer(int binding = 0, uint32_t allocatedSpace = 0) : _binding(binding)
    {
        // SET BUFFER SIZE
        bufferSize = allocatedSpace;

        // CREATE EMPTY BUFFER
        glGenBuffers(1, &bufferID);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferID);
        glBufferStorage(GL_SHADER_STORAGE_BUFFER, bufferSize, nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);  

        // SET BINDING POINT
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, _binding, bufferID);
    }

    ~DynamicPoolBuffer()
    {
        glDeleteBuffers(1, &bufferID);
    }

    void GrowBuffer(uint32_t addSize)
    {
        // INCREASE BUFFER SIZE
        uint32_t oldBufferSize = bufferSize;
        bufferSize = std::max(bufferSize + addSize, bufferSize * 2);

        // CREATE A NEW LARGER BUFFER
        unsigned int newBufferID;
        glGenBuffers(1, &newBufferID);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, newBufferID);
        glBufferStorage(GL_SHADER_STORAGE_BUFFER, bufferSize, nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);  
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, _binding, newBufferID);

        // COPY CURRENT DATA INTO LARGER BUFFER
        glBindBuffer(GL_COPY_READ_BUFFER, bufferID);
        glBindBuffer(GL_COPY_WRITE_BUFFER, newBufferID);
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, oldBufferSize);

        // DELETE CURRENT BUFFER
        glDeleteBuffers(1, &bufferID);

        // UPDATE CURRENT BUFFER
        bufferID = newBufferID;

        // SET BINDING POINT OF NEW LARGER BUFFER
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, _binding, bufferID);
    }

    void OccupyRegion(uint32_t size, uint32_t id, bool print=false)
    {
        ItemPartition itemPartition;
        itemPartition.start = FindAvailableSpace(size);
        itemPartition.size = size;
        itemPartition.id = id;

        if (itemPartitions.size() == 0)
        {
            itemPartitions.push_back(itemPartition);
            return;
        }

        // INSERT ITEM PARTITION IN CORRECT SPOT
        for (int i=0; i<itemPartitions.size(); i++)
        {
            if (itemPartition.start < itemPartitions[i].start)
            {
                // INSERT BEFORE
                itemPartitions.insert(itemPartitions.begin() + i, itemPartition);
                return;
            }
        }

        itemPartitions.push_back(itemPartition);
    }

    int FindAvailableSpace(uint32_t size)
    {
        if (size > bufferSize) return -1;

        if (itemPartitions.size() == 0) return 0;

        for (int i = 0; i < itemPartitions.size() - 1; i++)
        {
            ItemPartition &item = itemPartitions[i];
            ItemPartition &nextItem = itemPartitions[i + 1];

            uint32_t itemGap = nextItem.start - (item.start + item.size);

            if (size <= itemGap)
            {
                return item.start + item.size;  
            }
        }

        ItemPartition &lastItem = itemPartitions[itemPartitions.size()-1];
        uint32_t nextSpace = lastItem.start + lastItem.size; 

        if (nextSpace + size > bufferSize) 
        {
            return -1;
        }
        return nextSpace;
    }

    void DeleteItem(uint32_t id)
    {
        for (int i=0; i<itemPartitions.size(); i++)
        {
            if (id == itemPartitions[i].id)
            {
                itemPartitions.erase(itemPartitions.begin() + i);
            }
        }
    }

    void* GetMappedBuffer(int offset, int size)
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferID);
        return glMapBufferRange(GL_SHADER_STORAGE_BUFFER, offset, size, GL_MAP_WRITE_BIT);
    }

    void UnmapBuffer()
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferID);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER); 
    }

    void DeleteBuffer()
    {
        glDeleteBuffers(1, &bufferID);
    }

    uint32_t BufferSize()
    {
        return bufferSize;
    }

    std::vector<ItemPartition> itemPartitions;

private:
    int _binding;
    unsigned int bufferID;
    uint32_t bufferSize;
};

class DynamicContiguousBuffer
{
public:

    DynamicContiguousBuffer(int binding = 0, uint32_t allocatedSpace = 0) : _binding(binding)
    {
        // SET BUFFER SIZE
        bufferSize = allocatedSpace;
        usedCapacity = 0;

        // CREATE EMPTY BUFFER
        glGenBuffers(1, &bufferID);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferID);
        glBufferStorage(GL_SHADER_STORAGE_BUFFER, bufferSize, nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);  

        // SET BINDING POINT
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, _binding, bufferID);
    }

    ~DynamicContiguousBuffer()
    {
        glDeleteBuffers(1, &bufferID);
    }

    void GrowBuffer(uint32_t addSize)
    {
        // GROW BUFFER (ALLOCATE LARGER BUFFER)
        if (usedCapacity + addSize > bufferSize)
        {
            // INCREASE BUFFER SIZE
            bufferSize = std::max(bufferSize + addSize, bufferSize * 2);

            // CREATE A NEW LARGER BUFFER
            unsigned int newBufferID;
            glGenBuffers(1, &newBufferID);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, newBufferID);
            glBufferStorage(GL_SHADER_STORAGE_BUFFER, bufferSize, nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);  
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, _binding, newBufferID);

            // COPY CURRENT DATA INTO LARGER BUFFER
            glBindBuffer(GL_COPY_READ_BUFFER, bufferID);
            glBindBuffer(GL_COPY_WRITE_BUFFER, newBufferID);
            glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, usedCapacity);

            // DELETE CURRENT BUFFER
            glDeleteBuffers(1, &bufferID);

            // UPDATE CURRENT BUFFER
            bufferID = newBufferID;

            // SET BINDING POINT OF NEW LARGER BUFFER
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, _binding, bufferID);
        }
        usedCapacity += addSize;
    }

    void DeleteShift(uint32_t start, uint32_t size)
    {
        // CREATE A NEW BUFFER
        unsigned int newBufferID;
        glGenBuffers(1, &newBufferID);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, newBufferID);
        glBufferStorage(GL_SHADER_STORAGE_BUFFER, bufferSize, nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, _binding, newBufferID);

        // COPY FIRST PARTITION OF CURRENT DATA INTO NEW BUFFER
        glBindBuffer(GL_COPY_READ_BUFFER, bufferID);
        glBindBuffer(GL_COPY_WRITE_BUFFER, newBufferID);
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, start);

        // COPY SECOND PARTITION OF CURRENT DATA INTO NEW BUFFER
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, start + size, start, usedCapacity - start - size);

        //std::cout << "copying first slice to position: " << 0 << " copying " << start << " bytes" << std::endl;
        //std::cout << "copying second slice to position: " << start << " copying " << usedCapacity - start - size << " bytes" <<  std::endl;

        // DELETE CURRENT BUFFER
        glDeleteBuffers(1, &bufferID);

        // UPDATE CURRENT BUFFER
        bufferID = newBufferID;

        // DECREASE USAGE
        usedCapacity -= size;

       // std::cout << "removed size: " << size << " buffer size: " << bufferSize << " used capacity: " << usedCapacity << "\n";
    }

    void* GetMappedBuffer(int offset, int size)
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferID);
        return glMapBufferRange(GL_SHADER_STORAGE_BUFFER, offset, size, GL_MAP_WRITE_BIT);
    }

    void UnmapBuffer()
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferID);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER); 
    }

    void DeleteBuffer()
    {
        glDeleteBuffers(1, &bufferID);
    }

    uint32_t BufferSize()
    {
        return bufferSize;
    }

    uint32_t UsedCapacity()
    {
        return usedCapacity;
    }

private:
    int _binding;
    unsigned int bufferID;
    uint32_t bufferSize;
    uint32_t usedCapacity;
};


#ifndef WATER_H
#define WATER_H

#include <vector>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "common.h"

struct WaterGrid {
    void createImgs(VkDevice logicalDevice, VkPhysicalDevice phyDevice, VkQueue queue, VkCommandPool commandPool);
    std::vector<VkImage> compImgs;
    std::vector<VkDeviceMemory> compImgMems;
    std::vector<VkImageView> compImgViews;
    std::vector<VkSampler> compImgSamplers;
    uint32_t compImgCount = 0;

    std::vector<Vertex> vertices;
    std::vector<int> indices;
    void genWaterGrid(int x, int y);
};

#endif // WATER_H

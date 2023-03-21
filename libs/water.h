#ifndef WATER_H
#define WATER_H

#include <vector>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "common.h"
#include "../vars.h"

struct WaterParam {
    glm::ivec2 wOffset;
    int stage;
    glm::mat3 d0;
    glm::mat4 d1;
    glm::mat4 d2;
    glm::mat4 d3;
    glm::vec3 v0;
    float v1;
};

struct WaterGrid {
    VkPipelineLayout pipLayout;
    VkDescriptorSetLayout descSetLayout;
    VkPipeline pip;
    std::vector<VkDescriptorSet> descSets;
    VkDescriptorPool descPool;
    void createImgs(VkQueue queue, VkCommandPool commandPool);
    std::vector<VkImage> compImgs;
    std::vector<VkDeviceMemory> compImgMems;
    std::vector<VkImageView> compImgViews;
    std::vector<VkSampler> compImgSamplers;
    uint32_t compImgCount = 2;
    uint32_t compImgSets = 2;
    uint32_t compImgStages = 1;
    VkImage normalImg;
    VkDeviceMemory normalImgMem;
    VkImageView normalImgView;
    VkSampler normalSampler;
    int res = 1024;
    long gridsPerDegree = res * (long)compDomainsPerDegree;
    WaterParam waterParam[1];
    std::vector<VkBuffer> uBufs;
    std::vector<VkDeviceMemory> uBufMems;
    std::vector<void*> uBufDatas;

    std::vector<Vertex> vertices;
    std::vector<int> indices;
    void init(int swapImgCount);
    void genWaterGrid(int x, int y);
    void recordCmd(VkCommandBuffer cmdBuf, int descSetNum);
    glm::dvec2 updateWOffset(glm::dvec2 word_offset_coord, double cos_lat);
};

#endif // WATER_H

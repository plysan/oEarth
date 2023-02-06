#ifndef WATER_H
#define WATER_H

#include <vector>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "common.h"

struct WaterParam {
    int stage;
    int dummy1;
    int dummy2;
    int dummy3;
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
    uint32_t compImgCount = 3;
    uint32_t compImgSets = 1;
    uint32_t compImgStages = 3;
    VkImage normalImg;
    VkDeviceMemory normalImgMem;
    VkImageView normalImgView;
    VkSampler normalSampler;
    int res = 1024;
    WaterParam waterParam[3];
    std::vector<VkBuffer> uBufs;
    std::vector<VkDeviceMemory> uBufMems;
    std::vector<void*> uBufDatas;

    std::vector<Vertex> vertices;
    std::vector<int> indices;
    void init(int swapImgCount);
    void genWaterGrid(int x, int y);
    void recordCmd(VkCommandBuffer cmdBuf, int descSetNum);
};

#endif // WATER_H

#include <iostream>
#include <vector>
#include <glm/glm.hpp>

#include "../vars.h"
#include "../utils/coord.h"
#include "../utils/vulkan_boilerplate.h"
#include "water.h"

void createComputePipeline(VkPipeline &pip, VkPipelineLayout &pipLayout, VkDescriptorSetLayout &descSetLayout) {
    auto compShaderCode = readFile("shaders/shader.comp.spv");
    VkShaderModule compShaderModule = createShaderModule(compShaderCode);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1; // Optional
    pipelineLayoutInfo.pSetLayouts = &descSetLayout; // Optional
    pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional
    if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &pipLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkComputePipelineCreateInfo computePipelineCreateInfo = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, 0, 0, {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            0, 0, VK_SHADER_STAGE_COMPUTE_BIT, compShaderModule, "main", 0
        }, pipLayout, 0, 0
    };
    if (vkCreateComputePipelines(logicalDevice, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &pip) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute pipeline!");
    }
}

void WaterGrid::init(int swapImgCount) {
    std::cout << "WaterParam size:" << sizeof(WaterParam) << ", align:" << ubAlign << '\n';
    assert(sizeof(WaterParam) % ubAlign == 0);

    createDescriptorSetLayout(descSetLayout,
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC},
            {VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_COMPUTE_BIT});
    createComputePipeline(pip, pipLayout, descSetLayout);
    createDescriptorPool(descPool, compImgSets, {
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, compImgSets * 1},
                //{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0/*comp + compNormal*/},
                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, compImgSets * 3/*image count in water shader*/}});

    VkDeviceSize bufferSize = sizeof(WaterParam) * compImgStages;
    uBufs.resize(swapImgCount);
    uBufMems.resize(swapImgCount);
    uBufDatas.resize(swapImgCount);
    for (int i = 0; i < swapImgCount; i++) {
        createBuffer(bufferSize,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                uBufs[i], uBufMems[i]);
        vkMapMemory(logicalDevice, uBufMems[i], 0, bufferSize, 0, &uBufDatas[i]);
        memcpy(uBufDatas[i], waterParam, bufferSize);
    }

    assert(compImgSets == 2);
    std::vector<VkDescriptorSetLayout> compLayouts(compImgSets, descSetLayout);
    VkDescriptorSetAllocateInfo compDescSetAllocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, 0, descPool, compImgSets, compLayouts.data()};
    descSets.resize(compImgSets);
    if (vkAllocateDescriptorSets(logicalDevice, &compDescSetAllocInfo, descSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate compute descriptor sets!");
    }
    VkDescriptorImageInfo compImgInfos[3] = {
        {compImgSamplers[0], compImgViews[0], VK_IMAGE_LAYOUT_GENERAL},
        {compImgSamplers[1], compImgViews[1], VK_IMAGE_LAYOUT_GENERAL},
    };
    VkDescriptorImageInfo compNorImgInfo{normalSampler, normalImgView, VK_IMAGE_LAYOUT_GENERAL};
    VkWriteDescriptorSet compWrite[6] = {
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, descSets[0], 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &compImgInfos[0], 0, 0},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, descSets[0], 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &compImgInfos[1], 0, 0},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, descSets[0], 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &compNorImgInfo, 0, 0},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, descSets[1], 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &compImgInfos[1], 0, 0},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, descSets[1], 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &compImgInfos[0], 0, 0},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, descSets[1], 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &compNorImgInfo, 0, 0},
    };
    vkUpdateDescriptorSets(logicalDevice, 6, compWrite, 0, 0);

    VkDescriptorBufferInfo bufInfo{uBufs[0], 0, sizeof(WaterParam)};
    VkWriteDescriptorSet bufInfoWrite[2] = {
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, descSets[0], 3, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0, &bufInfo, 0},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, descSets[1], 3, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0, &bufInfo, 0},
    };
    vkUpdateDescriptorSets(logicalDevice, 2, bufInfoWrite, 0, 0);

}

glm::dvec2 WaterGrid::updateWOffset(glm::dvec2 word_offset_coord, double cos_lat) {
    glm::dvec2 waterOffset = word_offset_coord * static_cast<double>(gridsPerDegree);
    waterOffset.y *= cos_lat;
    glm::ivec2 waterIOffset = glm::round(waterOffset);
    static glm::ivec2 waterIOffsetOld = waterIOffset;
    glm::dvec2 waterOffsetFrac = waterOffset - (glm::dvec2)waterIOffset;
    glm::ivec2 waterIOffsetDel = waterIOffset - waterIOffsetOld;
    waterIOffsetOld = waterIOffset;
    waterParam[0].wOffset = waterIOffsetDel;
    memcpy(uBufDatas[0], waterParam, sizeof(glm::ivec2));
    return waterOffsetFrac / static_cast<double>(gridsPerDegree);
}

void WaterGrid::genWaterGrid(int x, int y) {
    vertices.clear();
    indices.clear();
    float x_half = (x - 1) / 2.0f, y_half = (y - 1) / 2.0f;
    for (int i = 0; i < y; i++) {
        for (int j = 0; j < x; j++) {
            vertices.push_back({glm::vec3((j - x_half) / x_half, (i - y_half) / y_half, 0.0f)});
        }
    }
    for (int i = 0; i < y - 1; i++) {
        for (int j = 0; j < x - 1; j++) {
            indices.push_back(i * x + j);
            indices.push_back((i + 1) * x + j + 1);
            indices.push_back((i + 1) * x + j);
            indices.push_back(i * x + j);
            indices.push_back(i * x + j + 1);
            indices.push_back((i + 1) * x + j + 1);
        }
    }
}

void WaterGrid::createImgs(VkQueue queue, VkCommandPool commandPool) {
    compImgs.resize(compImgCount);
    compImgMems.resize(compImgCount);
    compImgViews.resize(compImgCount);
    compImgSamplers.resize(compImgCount);
    std::vector<float> data(res * res * 4, 0);
    createInitialImage(res, res, 1, VK_FORMAT_R16G16_SFLOAT, data.data(), normalImg, normalImgMem, queue, commandPool);
    transitionImageLayout(normalImg, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, queue, commandPool, NULL, 0);
    normalImgView = createImageView(normalImg, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 2);
    createSampler(normalSampler);
    for (size_t i = 0; i < compImgs.size(); i++) {
        for (int j = 0; j < res; j++) {
            for (int k = 0; k < res; k++) {
                static glm::vec2 p1 = glm::vec2(450.0f, 450.0f);
                static glm::vec2 p2 = glm::vec2(150.0f, 380.0f);
                float d1 = (j-p1.x)*(j-p1.x) + (k-p1.y)*(k-p1.y);
                float d2 = (j-p2.x)*(j-p2.x) + (k-p2.y)*(k-p2.y);
                if (d1 < 1600) {
                    data[(res * k + j) * 4] = 0.45f - glm::sqrt(d1/100)/10;
                }
                if (d2 < 900) {
                    data[(res * k + j) * 4] = 0.4f - glm::sqrt(d2/100)/10;
                }
            }
            if (j < 400 || j > 500) {
                for (int k = 300; k < 450; k++) {
                    data[(res * k + j) * 4 + 3] = 0.9f;
                }
            }
        }
        createInitialImage(res, res, 1, VK_FORMAT_R32G32B32A32_SFLOAT, data.data(),
                compImgs.at(i), compImgMems.at(i), queue, commandPool);
        transitionImageLayout(compImgs.at(i), VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, queue, commandPool, NULL, 0);
        compImgViews.at(i) = createImageView(compImgs.at(i), VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 2);
        createSampler(compImgSamplers.at(i));
    }
}

void pipBarrierWR(VkCommandBuffer cmdBuf, VkImage image) {
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, 0,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image,
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void WaterGrid::recordCmd(VkCommandBuffer cmdBuf, int descSetNum) {
    static uint32_t dynOffset = 0;
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pip);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipLayout, 0, 1, &descSets[descSetNum], 1, &dynOffset);
    vkCmdDispatch(cmdBuf, res/8, res/8, 1);
}

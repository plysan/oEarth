#include <iostream>
#include <vector>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/common.hpp>
#include <glm/gtx/string_cast.hpp>

#include "../vars.h"
#include "../utils/coord.h"
#include "../utils/vulkan_boilerplate.h"
#include "water.h"
#include "globe.h"

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

void WaterGrid::init() {
    std::cout << "WaterParam size:" << sizeof(WaterParam) << ", align:" << ubAlign << '\n';
    assert(sizeof(WaterParam) % ubAlign == 0);

    createDescriptorSetLayout(descSetLayout,
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC},
            {VK_SHADER_STAGE_COMPUTE_BIT,
            VK_SHADER_STAGE_COMPUTE_BIT,
            VK_SHADER_STAGE_COMPUTE_BIT,
            VK_SHADER_STAGE_COMPUTE_BIT,
            VK_SHADER_STAGE_COMPUTE_BIT});
    createComputePipeline(pip, pipLayout, descSetLayout);
    createDescriptorPool(descPool, compImgSets, {
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, compImgSets * 1},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, compImgSets * 1},
                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, compImgSets * 3/*image count in water shader*/}});

    VkDeviceSize bufferSize = sizeof(WaterParam) * compImgStages;
    waterParam.resize(compImgStages);
    waterParam[0].stage = 0;
    waterParam[1].stage = 1;
    createBuffer(bufferSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            uBuf, uBufMem);
    vkMapMemory(logicalDevice, uBufMem, 0, bufferSize, 0, &uBufData);
    memcpy(uBufData, waterParam.data(), bufferSize);

    assert(compImgSets == 2);
    std::vector<VkDescriptorSetLayout> compLayouts(compImgSets, descSetLayout);
    VkDescriptorSetAllocateInfo compDescSetAllocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, 0, descPool, compImgSets, compLayouts.data()};
    descSets.resize(compImgSets);
    if (vkAllocateDescriptorSets(logicalDevice, &compDescSetAllocInfo, descSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate compute descriptor sets!");
    }
    VkDescriptorImageInfo compImgInfos[2] = {
        {compImgSamplers[0], compImgViews[0], VK_IMAGE_LAYOUT_GENERAL},
        {compImgSamplers[1], compImgViews[1], VK_IMAGE_LAYOUT_GENERAL},
    };
    VkDescriptorImageInfo compNorImgInfo{normalSampler, normalImgView, VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo bathymetryImgInfo{bathymetrySampler, bathymetryImgView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet compWrite[8] = {
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, descSets[0], 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &compImgInfos[0], 0, 0},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, descSets[0], 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &compImgInfos[1], 0, 0},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, descSets[0], 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &compNorImgInfo, 0, 0},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, descSets[0], 3, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &bathymetryImgInfo, 0, 0},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, descSets[1], 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &compImgInfos[1], 0, 0},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, descSets[1], 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &compImgInfos[0], 0, 0},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, descSets[1], 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &compNorImgInfo, 0, 0},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, descSets[1], 3, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &bathymetryImgInfo, 0, 0},
    };
    vkUpdateDescriptorSets(logicalDevice, sizeof(compWrite) / sizeof(VkWriteDescriptorSet), compWrite, 0, 0);

    VkDescriptorBufferInfo bufInfo{uBuf, 0, sizeof(WaterParam)};
    VkWriteDescriptorSet bufInfoWrite[2] = {
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, descSets[0], 4, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0, &bufInfo, 0},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, descSets[1], 4, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0, &bufInfo, 0},
    };
    vkUpdateDescriptorSets(logicalDevice, 2, bufInfoWrite, 0, 0);

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

void WaterGrid::createImgs(VkQueue queue, VkCommandPool commandPool, glm::dvec3 cameraPos) {
    compImgs.resize(compImgCount);
    compImgMems.resize(compImgCount);
    compImgViews.resize(compImgCount);
    compImgSamplers.resize(compImgCount);
    int maxRes = glm::max(waterRes, bathyRes);
    std::vector<float> data(maxRes * maxRes * 4, 0);
    createInitialImage(waterRes, waterRes, 1, VK_FORMAT_R16G16_SFLOAT, data.data(), normalImg, normalImgMem, queue, commandPool);
    transitionImageLayout(normalImg, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, queue, commandPool, NULL, 0);
    normalImgView = createImageView(normalImg, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 2);
    createSampler(normalSampler);

    for (int j = 0; j < waterRes; j++) { //x
        for (int k = 0; k < waterRes; k++) { //y
            static glm::vec2 p1 = glm::vec2(512.0f, 512.0f);
            float d1 = (j-p1.x)*(j-p1.x) + (k-p1.y)*(k-p1.y);
            //if (d1 < 1600) data[(waterRes * k + j) * 4] += 100.0f - glm::sqrt(d1);
        }
    }

    for (size_t i = 0; i < compImgs.size(); i++) {
        createInitialImage(waterRes, waterRes, 1, VK_FORMAT_R32G32B32A32_SFLOAT, data.data(),
                compImgs.at(i), compImgMems.at(i), queue, commandPool);
        transitionImageLayout(compImgs.at(i), VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, queue, commandPool, NULL, 0);
        compImgViews.at(i) = createImageView(compImgs.at(i), VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 2);
        createSampler(compImgSamplers.at(i));
    }

    std::fill(data.begin(), data.end(), 0.0f);
    createBuffer(bathyRes * bathyRes * 16, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                bathyStagingImg, bathyStagingImgMem);
    vkMapMemory(logicalDevice, bathyStagingImgMem, 0, VK_WHOLE_SIZE, 0, &bathyData);
    createImage(bathyRes, bathyRes, 1, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, bathymetryImg, bathymetryImgMem);
    transitionImageLayout(bathymetryImg, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, queue, commandPool, NULL, 0);
    bathymetryImgView = createImageView(bathymetryImg, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 2);
    createSampler(bathymetrySampler);
}

void WaterGrid::stageBathymetry(glm::dvec3 cameraPos) {
    glm::dvec2 worldCoord = dPos2DCoord(cameraPos);
    bathyPosStaging = worldCoord;
    double cos_lat = glm::cos(worldCoord.x);
    worldCoord = worldCoord / glm::pi<double>() * 180.0;
    double waterRadius = (glm::max(0.0001/*replace me*/, glm::length(cameraPos) - 1.0)) * glm::tan(glm::pi<double>()/3);
    double bathyRadiusLat = waterRadius * 2;
    bathyRadiusStaging = glm::dvec2(bathyRadiusLat, bathyRadiusLat / cos_lat);
    fillBathymetry(worldCoord - bathyRadiusStaging / glm::pi<double>() * 180.0, worldCoord + bathyRadiusStaging / glm::pi<double>() * 180.0, static_cast<float*>(bathyData), bathyRes);
}

void WaterGrid::updateBathymetry(VkQueue graphicsQueue, VkCommandPool commandPool) {
    transitionImageLayout(bathymetryImg, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                graphicsQueue, commandPool, NULL, 0);
    copyBufferToImage(bathyStagingImg, bathymetryImg, bathyRes, bathyRes, 1, graphicsQueue, commandPool);
    transitionImageLayout(bathymetryImg, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            graphicsQueue, commandPool, NULL, 0);
    bathyPos = bathyPosStaging;
    bathyRadius = bathyRadiusStaging;
}

glm::dvec2 WaterGrid::updateWOffset(glm::dvec2 worldCoord, double &waterRadius, glm::vec2 &bathyUvMid, double cos_lat, int count) {
    static double waterRadiusOld = waterRadius / waterRes / 10;
    static double waterBathyPropo = waterRadius / bathyRadius.x;
    static double bathyUvBlOrig = 0.5 - 0.5 * waterBathyPropo, bathyUvDel = 1.0 / (waterRes-1) * waterBathyPropo;
    static glm::dvec2 waterDel = glm::dvec2(waterRadius * 2 / (waterRes-1), waterRadius * 2 / (waterRes-1) / cos_lat);
    static double dxd = waterDel.x * earthRadiusM * 2;
    double scale = waterRadius / waterRadiusOld;
    glm::dvec2 waterCoord, waterCoordFrac;
    glm::ivec2 waterICoord, waterICoordDel;
    static glm::dvec2 worldICoord;
    static glm::ivec2 waterICoordOld = glm::round(worldCoord / waterDel);
    if (scale < 1.5 && scale > 2.0/3) {
        scale = 1.0;
        waterCoord = worldCoord / waterDel;
        waterICoord = glm::round(waterCoord);
        waterRadius = waterRadiusOld;
        waterCoordFrac = waterCoord - (glm::dvec2)waterICoord;
        waterICoordDel = waterICoord - waterICoordOld;
        waterICoordDel = glm::ivec2(waterICoordDel.y, waterICoordDel.x);
    } else {
        waterRadiusOld = waterRadius;
        waterBathyPropo = waterRadius / bathyRadius.x;
        bathyUvDel = 1.0 / (waterRes-1) * waterBathyPropo;
        bathyUvBlOrig = 0.5 - 0.5 * waterBathyPropo;
        waterDel = glm::dvec2(waterRadius * 2 / (waterRes-1), waterRadius * 2 / (waterRes-1) / cos_lat);
        dxd = waterDel.x * earthRadiusM * 2;
        glm::dvec2 waterCoord = worldCoord / waterDel;
        waterICoord = glm::round(waterCoord);
        waterCoordFrac = waterCoord - (glm::dvec2)waterICoord;
        waterICoordDel = glm::ivec2(0);
    }
    waterICoordOld = waterICoord;
    waterParam[0].iOffset = waterParam[1].iOffset = waterICoordDel;
    waterParam[0].dt = waterParam[1].dt = dt;
    waterParam[0].dxd = waterParam[1].dxd = dxd;

    worldICoord = (glm::dvec2)waterICoord * waterDel;
    glm::dvec2 bathyUvBl = glm::dvec2(bathyUvBlOrig);
    glm::dvec2 bathyDeviate = glm::dvec2(bathyPos - worldICoord) / (bathyRadius * 2.0);
    bathyUvBl -= bathyDeviate;
    bathyUvBl = glm::dvec2(bathyUvBl.y, bathyUvBl.x);
    waterParam[0].bathyUvBl = waterParam[1].bathyUvBl = bathyUvBl;
    bathyUvMid = (0.5 - bathyDeviate);
    bathyUvMid = glm::vec2(bathyUvMid.y, bathyUvMid.x);
    waterParam[0].bathyUvDel = waterParam[1].bathyUvDel = bathyUvDel;
    waterParam[0].time = waterParam[1].time = dt * count;

    waterParam[0].scale = waterParam[1].scale = scale;
    static glm::dvec2 waveRectSize = glm::dvec2(waveDomainSize, waveDomainSize / cos_lat);
    static double pi2d = glm::pi<double>() * 2;
    glm::dvec2 freqRadiusXY = glm::dvec2(glm::abs(waterRadius / waveDomainSize * pi2d));
    glm::dvec2 freqCoordMod = glm::fmod((glm::dvec2)waterICoord * waterDel / waveRectSize * pi2d, pi2d);
    freqCoordMod = glm::dvec2(freqCoordMod.y, freqCoordMod.x);
    waterParam[0].freqCoordBl = waterParam[1].freqCoordBl = freqCoordMod - freqRadiusXY;
    waterParam[0].freqCoordTr = waterParam[1].freqCoordTr = freqCoordMod + freqRadiusXY;
    waterParam[0].freqCoordDel = waterParam[1].freqCoordDel = freqRadiusXY * 2.0 / (waterRes-1.0);
    memcpy(uBufData, waterParam.data(), sizeof(WaterParam) * compImgStages);
    glm::vec2 uvFrac = waterCoordFrac / static_cast<double>(waterRes-1);
    return glm::vec2(uvFrac.y, uvFrac.x);
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
    uint32_t dynOffset = 0;
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pip);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipLayout, 0, 1, &descSets[descSetNum], 1, &dynOffset);
    vkCmdDispatch(cmdBuf, waterRes/localSize, waterRes/localSize, 1);
    pipBarrierWR(cmdBuf, compImgs[(descSetNum+1)%2]);
    dynOffset += sizeof(WaterParam);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipLayout, 0, 1, &descSets[descSetNum], 1, &dynOffset);
    assert(waterRes % (localSize * localSize) == 0);
    vkCmdDispatch(cmdBuf, waterRes / (localSize * localSize) * 4, 1, 1);
}

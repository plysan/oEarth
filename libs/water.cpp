#include <iostream>
#include <vector>
#include <glm/glm.hpp>

#include "../vars.h"
#include "../utils/coord.h"
#include "../utils/vulkan_boilerplate.h"
#include "water.h"

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

void WaterGrid::createImgs(VkDevice logicalDevice, VkPhysicalDevice phyDevice, VkQueue queue, VkCommandPool commandPool) {
    compImgCount = 3;
    compImgs.resize(compImgCount);
    compImgMems.resize(compImgCount);
    compImgViews.resize(compImgCount);
    compImgSamplers.resize(compImgCount);
    std::vector<_Float16> data(1024 * 1024 * 4, 0);
    for (size_t i = 0; i < compImgs.size(); i++) {
        for (int j = 0; j < 1024; j++) {
            for (int k = 0; k < 1024; k++) {
                float d2 = (j-350)*(j-350) + (k-200)*(k-200);
                if (d2 < 100) {
                    data[(1024 * k + j) * 4] = 1.0f - glm::sqrt(d2/100);
                }
            }
        }
        createInitialImage(1024, 1024, 1, VK_FORMAT_R16G16B16A16_SFLOAT, data.data(),
                compImgs.at(i), compImgMems.at(i), logicalDevice, phyDevice, queue, commandPool);
        transitionImageLayout(compImgs.at(i), VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, logicalDevice, queue, commandPool, NULL, 0);
        compImgViews.at(i) = createImageView(logicalDevice, compImgs.at(i), VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 2);
        createSampler(phyDevice, logicalDevice, compImgSamplers.at(i));
    }
}


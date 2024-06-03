#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtx/common.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stddef.h>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <chrono>
#include <vector>
#include <array>
#include <cstring>
#include <sstream>
#include <cstdint>
#include <set>
#include <thread>

#include "utils/coord.h"
#include "utils/vulkan_boilerplate.h"
#include "libs/common.h"
#include "libs/camera.h"
#include "libs/water.h"
#include "libs/globe.h"
#include "libs/sky_dome.h"
#include "vars.h"


struct FrameParam {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 p_inv;
    glm::mat4 v_inv;
    glm::vec3 wordOffset;
    int target;
    glm::vec2 freqCoord;
    glm::vec2 waterOffset;
    glm::vec2 bathyUvMid;
    glm::vec2 bathyRadius;
    float waterRadius;
    float height;
    float time;
    union {
        VkDrawIndirectCommand terrainParam;
        VkDrawIndexedIndirectCommand skyParam;
        VkDrawIndexedIndirectCommand water_param;
    };
    glm::mat4 pad1;
    glm::mat3 pad2;
    glm::vec3 pad3;
};

struct StagingBufferStruct {
    // | terrain_vert | sky_vert | sky_idx | water_vert | water_idx |
    int terrainVertMax;
    int terrainVertSize;
    int skyVertMax;
    int skyIdxMax;
    int skyIdxSize;
    int water_vert_max;
    int water_idx_max;
    int water_idx_size;
};

static double UPDATE_DISTANCE = 0.0004;

class VKDemo {

    const uint32_t WIDTH = 1280;
    const uint32_t HEIGHT = 720;

    const std::vector<const char*> requiredValidationLayers = {
        "VK_LAYER_KHRONOS_validation",
    };

    const std::vector<const char*> requiredPhyDevExt = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#ifdef DEBUG
        VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
#endif
        //"VK_KHR_portability_subset",
    };

#ifdef DEBUG
    const bool enableValidationLayers = true;
#else
    const bool enableValidationLayers = false;
#endif

    const int MAX_FRAMES_IN_FLIGHT = 2;

public:

    void run() {
        initWindow();
        initVulkan();
        initApp();
        mainLoop();
        cleanup();
    }

private:

    // stuff need cleanup
    GLFWwindow* window;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapChain;
    std::vector<VkImageView> swapChainImageViews;
    VkRenderPass renderPass;
    VkDescriptorSetLayout gDescSetLayout;
    VkPipelineLayout gPipelineLayout;
    VkPipeline graphicsPipeline;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    VkCommandPool commandPool;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersData;
    std::vector<VkCommandBuffer> commandBuffers;
    VkDescriptorPool descriptorPool;
    VkImage terrainImage;
    VkDeviceMemory terrainImageMemory;
    VkImageView terrainImageView;
    VkSampler terrainSampler;
    VkImage scatterImage;
    VkDeviceMemory scatterImageMemory;
    VkImageView scatterImageView;
    VkSampler scatterSampler;
    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;
    TetrahedraGlobe globe{};
    SkyDome skyDome;
    WaterGrid water_grid;
    VkBuffer vertStagingBuffer;
    VkDeviceMemory vertStagingBufferMemory;
    VkBuffer terrainImageBuf;
    VkDeviceMemory terrainImageSBM;

    // stuff cleanup free
    int phyDevGraphFamilyIdx = -1, phyDevPresentFamilyIdx = -1, phyDevComputeFamilyIdx = -1;
    VkSurfaceCapabilitiesKHR phyDevSurCaps;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkQueue computeQueue;
    std::vector<VkImage> swapChainImages;
    VkExtent2D swapChainExtent;
    VkFormat swapChainImageFormat;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;
    size_t currentFrame = 0;
    bool framebufferResized = false;
    std::vector<VkDescriptorSet> renderDescSets;
    StagingBufferStruct sbs;
    //glm::vec3 initPos = coord2Pos(42.1483f, 3.72775f, 0.000005f); // ocean
    glm::vec3 initPos = coord2Pos(42.2353f, 3.26535f, 0.000005f); // cliff
    //glm::vec3 initPos = coord2Pos(42.2546f, 3.1774f, 0.000005f); // beach
    Camera camera{
        .pos = initPos,
        .dir = glm::dvec3(initPos.x, initPos.y, initPos.z+0.5f),
        .fov = 65.0f
    };
    glm::dvec3 lastCameraPos;
    bool renderUpdate = false;
    bool bathyUpdate = false;
    FrameParam fParams[3];
    long frame_count = 0, frame_count_last = 0;

    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        window = glfwCreateWindow(WIDTH, HEIGHT, "fps:", nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    }

    void initVulkan() {
        createInstance(instance, requiredValidationLayers, enableValidationLayers);
        if (enableValidationLayers) CreateDebugUtilsMessengerEXT(instance, nullptr, &debugMessenger);
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
            throw std::runtime_error("failed to create window surface!");
        pickPhysicalDevice(instance,surface, phyDevSurCaps, phyDevGraphFamilyIdx, phyDevPresentFamilyIdx,
                           phyDevComputeFamilyIdx, requiredPhyDevExt, sizeof(FrameParam));
        createLogicalDevice(graphicsQueue, presentQueue, computeQueue,
                            requiredValidationLayers, requiredPhyDevExt,
                            phyDevGraphFamilyIdx, phyDevPresentFamilyIdx, phyDevComputeFamilyIdx, enableValidationLayers);
        createSwapChain(swapChain, swapChainImages, swapChainExtent, swapChainImageFormat,
                        window, surface, phyDevSurCaps, phyDevGraphFamilyIdx, phyDevPresentFamilyIdx);
        createImageViews();
        createRenderPass(renderPass, swapChainImageFormat);
        createDescriptorSetLayout(gDescSetLayout,
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
                {VK_SHADER_STAGE_ALL, VK_SHADER_STAGE_FRAGMENT_BIT, VK_SHADER_STAGE_ALL, VK_SHADER_STAGE_ALL, VK_SHADER_STAGE_ALL, VK_SHADER_STAGE_VERTEX_BIT});
        createGraphicsPipeline(gPipelineLayout, graphicsPipeline, gDescSetLayout, renderPass, swapChainExtent, sizeof(Vertex),
                {offsetof(Vertex, pos), offsetof(Vertex, normal), offsetof(Vertex, texCoord)},
                {VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32_SFLOAT});
        createCommandPool(commandPool, phyDevGraphFamilyIdx);
        createDepthResources(swapChainExtent, depthImage, depthImageMemory, depthImageView);
        createFramebuffers(renderPass, swapChainFramebuffers, swapChainImageViews, depthImageView, swapChainExtent);
        water_grid.createImgs(computeQueue, commandPool, camera.pos);
        createStagingBufferStruct();
        terrainImageView = createImageView(terrainImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, 2);
        scatterImageView = createImageView(scatterImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, 3);
        createSampler(terrainSampler);
        createSampler(scatterSampler);
        createUniformBuffers();
        uint32_t descSetCount/*TODO*/ = swapChainImages.size() * water_grid.compImgSets;
        createDescriptorPool(descriptorPool, descSetCount, {
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, descSetCount},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descSetCount * 5/*terrain + scatter + comp + compNormal + bathy*/}});
        createDescriptorSets(descriptorPool, descSetCount, swapChainImages.size(), gDescSetLayout, renderDescSets,
                uniformBuffers, sizeof(FrameParam), {terrainImageView, scatterImageView}, {terrainSampler, scatterSampler}, water_grid);
        water_grid.init();
        createCommandBuffers();
        createSyncObjects();
        std::thread t1(&VKDemo::updateStagingBufferStruct, this);
        t1.detach();
#ifdef __linux__
        std::thread t2(&VKDemo::displayFps, this);
        t2.detach();
#endif
    }

    void recreateSwapChain() {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }
        vkDeviceWaitIdle(logicalDevice);
        cleanupSwapChain();
        createSwapChain(swapChain, swapChainImages, swapChainExtent, swapChainImageFormat,
                        window, surface, phyDevSurCaps, phyDevGraphFamilyIdx, phyDevPresentFamilyIdx);
        createImageViews();
        createRenderPass(renderPass, swapChainImageFormat);
        createGraphicsPipeline(gPipelineLayout, graphicsPipeline, gDescSetLayout, renderPass, swapChainExtent, sizeof(Vertex),
                {offsetof(Vertex, pos), offsetof(Vertex, normal), offsetof(Vertex, texCoord)},
                {VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32_SFLOAT});
        createDepthResources(swapChainExtent, depthImage, depthImageMemory, depthImageView);
        createFramebuffers(renderPass, swapChainFramebuffers, swapChainImageViews, depthImageView, swapChainExtent);
        createUniformBuffers();
        uint32_t descSetCount/*TODO*/ = swapChainImages.size() * water_grid.compImgSets;
        createDescriptorPool(descriptorPool, descSetCount, {
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, descSetCount},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descSetCount * 5/*terrain + scatter + comp + compNormal + bathy*/}});
        createDescriptorSets(descriptorPool, descSetCount, swapChainImages.size(), gDescSetLayout, renderDescSets,
                uniformBuffers, sizeof(FrameParam), {terrainImageView, scatterImageView}, {terrainSampler, scatterSampler}, water_grid);
        createCommandBuffers();
    }

    void displayFps() {
        while (true) {
            std::stringstream ss;
            glm::dvec2 curCoord = dPos2DCoord(camera.pos);
            curCoord = curCoord / glm::pi<double>() * 180.0;
            ss << "fps: " << (frame_count - frame_count_last) * 2 << ", coord: " << curCoord.x << ", " << curCoord.y;
            frame_count_last = frame_count;
            glfwSetWindowTitle(window, ss.str().c_str());
            std::this_thread::sleep_for (std::chrono::milliseconds(500));
        }
    }

    void initApp() {
        initInvParam();
    }

    void createImageViews() {
        swapChainImageViews.resize(swapChainImages.size());
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 2);
        }
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            drawFrame();
        }
        vkDeviceWaitIdle(logicalDevice);
    }

    void cleanupSwapChain() {
        vkDestroyImageView(logicalDevice, depthImageView, nullptr);
        vkDestroyImage(logicalDevice, depthImage, nullptr);
        vkFreeMemory(logicalDevice, depthImageMemory, nullptr);
        for (auto framebuffer : swapChainFramebuffers) {
            vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
        }
        vkFreeCommandBuffers(logicalDevice, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
        vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(logicalDevice, gPipelineLayout, nullptr);
        vkDestroyRenderPass(logicalDevice, renderPass, nullptr);
        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(logicalDevice, imageView, nullptr);
        }
        vkDestroySwapchainKHR(logicalDevice, swapChain, nullptr);
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            vkDestroyBuffer(logicalDevice, uniformBuffers[i], nullptr);
            vkFreeMemory(logicalDevice, uniformBuffersMemory[i], nullptr);
        }
        vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);
    }

    void cleanup() {
        cleanupSwapChain();
        vkDestroySampler(logicalDevice, terrainSampler, nullptr);
        vkDestroyImageView(logicalDevice, terrainImageView, nullptr);
        vkDestroyImage(logicalDevice, terrainImage, nullptr);
        vkFreeMemory(logicalDevice, terrainImageMemory, nullptr);
        vkDestroySampler(logicalDevice, scatterSampler, nullptr);
        vkDestroyImageView(logicalDevice, scatterImageView, nullptr);
        vkDestroyImage(logicalDevice, scatterImage, nullptr);
        vkFreeMemory(logicalDevice, scatterImageMemory, nullptr);
        vkDestroyDescriptorSetLayout(logicalDevice, gDescSetLayout, nullptr);
        vkDestroyBuffer(logicalDevice, vertexBuffer, nullptr);
        vkFreeMemory(logicalDevice, vertexBufferMemory, nullptr);
        vkDestroyBuffer(logicalDevice, indexBuffer, nullptr);
        vkFreeMemory(logicalDevice, indexBufferMemory, nullptr);
        vkDestroyBuffer(logicalDevice, vertStagingBuffer, nullptr);
        vkFreeMemory(logicalDevice, vertStagingBufferMemory, nullptr);
        vkDestroyBuffer(logicalDevice, terrainImageBuf, nullptr);
        vkFreeMemory(logicalDevice, terrainImageSBM, nullptr);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(logicalDevice, renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(logicalDevice, imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(logicalDevice, inFlightFences[i], nullptr);
        }
        vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
        vkDestroyDevice(logicalDevice, nullptr);
        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void createStagingBufferStruct() {
        sbs.terrainVertMax = 500000;
        sbs.skyVertMax = 25000;
        sbs.skyIdxMax = 25000;
        sbs.water_vert_max = 400000;
        sbs.water_idx_max = 2300000;
        lastCameraPos = camera.pos;
        globe.genGlobe(lastCameraPos);
        water_grid.stageBathymetry(lastCameraPos);
        water_grid.updateBathymetry(graphicsQueue, commandPool);
        sbs.terrainVertSize = std::min(sbs.terrainVertMax, (int)globe.vertices.size());
        skyDome.genSkyDome(lastCameraPos);
        sbs.skyIdxSize = std::min(sbs.skyIdxMax, (int)skyDome.indices.size());
        water_grid.genWaterGrid(800, 480);
        std::cout << ">>>>>>>>water_grid.indices.size: " << (int)water_grid.indices.size() << '\n';
        sbs.water_idx_size = std::min(sbs.water_idx_max, (int)water_grid.indices.size());
        createTerrainImage(globe);
        createVertBuffers();
        stageVertBuffers(true);
        skyDome.genScatterTexure();
        uint32_t scatter_texture_size = scatterTextureSunAngleSize * scatterTextureHeightSize;
        createInitialImage(scatter_texture_size, scatter_texture_size, scatter_texture_size, VK_FORMAT_R8G8B8A8_SRGB, skyDome.scatterTexture.data,
                scatterImage, scatterImageMemory, graphicsQueue, commandPool);
    }

    void updateStagingBufferStruct() {
        while (true) {
            if (renderUpdate) {
                double distanceMoved = glm::distance(lastCameraPos, camera.pos);
                if (distanceMoved > UPDATE_DISTANCE) {
                    lastCameraPos = camera.pos;
                    globe.genGlobe(lastCameraPos);
                    skyDome.genSkyDome(lastCameraPos);
                    stageVertBuffers(false);
                    //TODO atomic
                    sbs.terrainVertSize = std::min(sbs.terrainVertMax, (int)globe.vertices.size());
                    sbs.skyIdxSize = std::min(sbs.skyIdxMax, (int)skyDome.indices.size());
                    water_grid.stageBathymetry(lastCameraPos);
                    renderUpdate = false;
                }
            }
            std::this_thread::sleep_for (std::chrono::seconds(2));
        }
    }

    void stageVertBuffers(bool stage_water) {
        void* data;
        vkMapMemory(logicalDevice, vertStagingBufferMemory, 0, VK_WHOLE_SIZE, 0, &data);
        memcpy(data, globe.vertices.data(), (size_t)(sizeof(Vertex) * std::min(sbs.terrainVertMax, (int)globe.vertices.size())));
        memcpy((char*)data + sizeof(Vertex) * sbs.terrainVertMax, skyDome.vertices.data(),
                (size_t)(sizeof(Vertex) * std::min(sbs.skyVertMax, (int)skyDome.vertices.size())));
        memcpy((char*)data + sizeof(Vertex) * (sbs.terrainVertMax + sbs.skyVertMax), skyDome.indices.data(),
                (size_t)(sizeof(int) * std::min(sbs.skyIdxMax, (int)skyDome.indices.size())));
        if (stage_water) {
            memcpy((char*)data + sizeof(Vertex) * (sbs.terrainVertMax + sbs.skyVertMax) + sizeof(int) * sbs.skyIdxMax, water_grid.vertices.data(),
                    (size_t)(sizeof(Vertex) * std::min(sbs.water_vert_max, (int)water_grid.vertices.size())));
            memcpy((char*)data + sizeof(Vertex) * (sbs.terrainVertMax + sbs.skyVertMax + sbs.water_vert_max) + sizeof(int) * sbs.skyIdxMax, water_grid.indices.data(),
                    (size_t)(sizeof(int) * std::min(sbs.water_idx_max, (int)water_grid.indices.size())));
        }
        vkUnmapMemory(logicalDevice, vertStagingBufferMemory);
        vkMapMemory(logicalDevice, terrainImageSBM, 0, VK_WHOLE_SIZE, 0, &data);
        memcpy((char*)data, globe.mega_texture.data, (size_t)(globe.mega_texture.w * globe.mega_texture.h * 4));
        vkUnmapMemory(logicalDevice, terrainImageSBM);
    }

    void createVertBuffers() {
        std::cout << "Init vertex count: globe: " << globe.vertices.size() << " sky: " << skyDome.vertices.size() << '\n';
        VkDeviceSize vertBufferSize = sizeof(Vertex) * (sbs.terrainVertMax + sbs.skyVertMax + sbs.skyIdxMax + sbs.water_vert_max + sbs.water_idx_max);
        std::cout << "Allocate vertex size: " << sizeof(Vertex) << " x " << (sbs.terrainVertMax + sbs.skyVertMax + sbs.skyIdxMax) << " = " << vertBufferSize << '\n';
        createBuffer(vertBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                vertStagingBuffer, vertStagingBufferMemory);
        createBuffer(vertBufferSize,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
        VkDeviceSize indexBufferSize = sizeof(int) * (sbs.skyIdxMax + sbs.water_idx_max);
        std::cout << "Allocate index size: " << sizeof(int) << " x " << sbs.skyIdxMax << " = " << indexBufferSize << '\n';
        createBuffer(indexBufferSize,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);
    }

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, int srcOffset, int dstOffset, VkDeviceSize size) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands(commandPool);
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = srcOffset;
        copyRegion.dstOffset = dstOffset;
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
        endSingleTimeCommands(graphicsQueue, commandBuffer, commandPool);
    }

    void createUniformBuffers() {
        //TODO
        VkDeviceSize bufferSize = sizeof(FrameParam);
        bufferSize = sizeof(FrameParam) * 3;
        uniformBuffers.resize(swapChainImages.size());
        uniformBuffersData.resize(swapChainImages.size());
        uniformBuffersMemory.resize(swapChainImages.size());
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            createBuffer(bufferSize,
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    uniformBuffers[i], uniformBuffersMemory[i]);
            vkMapMemory(logicalDevice, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersData[i]);
        }
    }

    void createCommandBuffers() {
        commandBuffers.resize(swapChainFramebuffers.size() * water_grid.compImgSets);
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();
        if (vkAllocateCommandBuffers(logicalDevice, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }

        for (size_t j = 0; j < water_grid.compImgSets; j++) {
            for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
                int cmdBufIdx = swapChainFramebuffers.size() * j + i;
                VkCommandBufferBeginInfo beginInfo{};
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
                beginInfo.pInheritanceInfo = nullptr; // Optional
                if (vkBeginCommandBuffer(commandBuffers[cmdBufIdx], &beginInfo) != VK_SUCCESS) {
                    throw std::runtime_error("failed to begin recording command buffer!");
                }
                // compute
                water_grid.recordCmd(commandBuffers[cmdBufIdx], j);
                // render
                VkRenderPassBeginInfo renderPassInfo{};
                renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                renderPassInfo.renderPass = renderPass;
                renderPassInfo.framebuffer = swapChainFramebuffers[i];
                renderPassInfo.renderArea.offset = {0, 0};
                renderPassInfo.renderArea.extent = swapChainExtent;
                std::array<VkClearValue, 2> clearValues{};
                clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
                clearValues[1].depthStencil = {1.0f, 0};
                renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
                renderPassInfo.pClearValues = clearValues.data();
                vkCmdBeginRenderPass(commandBuffers[cmdBufIdx], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
                vkCmdBindPipeline(commandBuffers[cmdBufIdx], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
                VkBuffer vertexBuffers[] = {vertexBuffer};
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(commandBuffers[cmdBufIdx], 0, 1, vertexBuffers, offsets);
                vkCmdBindIndexBuffer(commandBuffers[cmdBufIdx], indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                uint32_t dynamicOffset = 0;
                vkCmdBindDescriptorSets(commandBuffers[cmdBufIdx], VK_PIPELINE_BIND_POINT_GRAPHICS, gPipelineLayout, 0, 1, &renderDescSets[cmdBufIdx], 1, &dynamicOffset);
                VkDeviceSize waterOffset = offsetof(struct FrameParam, terrainParam);
                vkCmdDrawIndirect(commandBuffers[cmdBufIdx], uniformBuffers[i], waterOffset, 1, 0);
                dynamicOffset = sizeof(FrameParam);
                vkCmdBindDescriptorSets(commandBuffers[cmdBufIdx], VK_PIPELINE_BIND_POINT_GRAPHICS, gPipelineLayout, 0, 1, &renderDescSets[cmdBufIdx], 1, &dynamicOffset);
                VkDeviceSize skyOffset = dynamicOffset + offsetof(struct FrameParam, skyParam);
                //TODO
                vkCmdDrawIndexedIndirect(commandBuffers[cmdBufIdx], uniformBuffers[i], skyOffset, 1, 0);
                dynamicOffset += sizeof(FrameParam);
                vkCmdBindDescriptorSets(commandBuffers[cmdBufIdx], VK_PIPELINE_BIND_POINT_GRAPHICS, gPipelineLayout, 0, 1, &renderDescSets[cmdBufIdx], 1, &dynamicOffset);
                VkDeviceSize water_offset = dynamicOffset + offsetof(struct FrameParam, water_param);
                vkCmdDrawIndexedIndirect(commandBuffers[cmdBufIdx], uniformBuffers[i], water_offset, 1, 0);
                vkCmdEndRenderPass(commandBuffers[cmdBufIdx]);
                if (vkEndCommandBuffer(commandBuffers[cmdBufIdx]) != VK_SUCCESS) {
                    throw std::runtime_error("failed to record command buffer!");
                }
            }
        }
    }

    void drawFrame() {
        //std::this_thread::sleep_for (std::chrono::milliseconds(100));
        static double maxCost = 0.0;
        clock_t stamp1 = clock();

        static VkFence lastFence = inFlightFences[currentFrame];
        vkWaitForFences(logicalDevice, 1, &lastFence, VK_TRUE, UINT64_MAX);
        lastFence = inFlightFences[currentFrame];
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(logicalDevice, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }
        // Check if a previous frame is using this image (i.e. there is its fence to wait on)
        if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            vkWaitForFences(logicalDevice, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }
        // Mark the image as now being in use by this frame
        imagesInFlight[imageIndex] = inFlightFences[currentFrame];
        if (!renderUpdate) {
            copyBuffer(vertStagingBuffer, vertexBuffer, 0, 0, sizeof(Vertex) * std::min((int)globe.vertices.size(), sbs.terrainVertMax));
            copyBuffer(vertStagingBuffer, vertexBuffer, sizeof(Vertex) * sbs.terrainVertMax, sizeof(Vertex) * sbs.terrainVertMax,
                    sizeof(Vertex) * std::min((int)skyDome.vertices.size(), sbs.skyVertMax));
            copyBuffer(vertStagingBuffer, indexBuffer, sizeof(Vertex) * (sbs.terrainVertMax + sbs.skyVertMax), 0, sizeof(int) * std::min((int)skyDome.indices.size(), sbs.skyIdxMax));
            static bool copy_water = true;
            if (copy_water) {
                copy_water = false;
                copyBuffer(vertStagingBuffer, vertexBuffer, sizeof(Vertex) * (sbs.terrainVertMax + sbs.skyVertMax) + sizeof(int) * sbs.skyIdxMax,
                        sizeof(Vertex) * (sbs.terrainVertMax + sbs.skyVertMax), sizeof(Vertex) * std::min((int)water_grid.vertices.size(), sbs.water_vert_max));
                copyBuffer(vertStagingBuffer, indexBuffer, sizeof(Vertex) * (sbs.terrainVertMax + sbs.skyVertMax + sbs.water_vert_max) + sizeof(int) * sbs.skyIdxMax,
                        sizeof(int) * sbs.skyIdxMax, sizeof(int) * std::min((int)water_grid.indices.size(), sbs.water_idx_max));
            }
            transitionImageLayout(terrainImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    graphicsQueue, commandPool, NULL, 0);
            copyBufferToImage(terrainImageBuf, terrainImage, globe.mega_texture.w, globe.mega_texture.h, 1, graphicsQueue, commandPool);
            transitionImageLayout(terrainImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    graphicsQueue, commandPool, NULL, 0);
            std::cout << "Copy size: " << globe.vertices.size() << '\n';
            fParams[0].wordOffset = globe.camOffset;
            fParams[1].wordOffset = glm::vec3(0);
            fParams[2].wordOffset = globe.camOffset;
            water_grid.updateBathymetry(graphicsQueue, commandPool);
            renderUpdate = true;
        }
        clock_t stamp2 = clock();
        updateUniformBuffer(imageIndex);
        clock_t stamp3 = clock();

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        static int compSwitch = 0;
        int cmdBufOffset = (compSwitch++ % water_grid.compImgSets) * swapChainImages.size();
        submitInfo.pCommandBuffers = &commandBuffers[imageIndex + cmdBufOffset];
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        vkResetFences(logicalDevice, 1, &inFlightFences[currentFrame]);
        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        VkSwapchainKHR swapChains[] = {swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = nullptr; // Optional
        result = vkQueuePresentKHR(presentQueue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = false;
            recreateSwapChain();
        } else if (result != VK_SUCCESS) {
            std::cout << "failed present swap chain image: " << result << '\n';
            throw std::runtime_error("failed to present swap chain image: " + result);
        }
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        clock_t stamp4 = clock();
        double costTmp = stamp4 - stamp1;
        if (costTmp > maxCost) {
            maxCost = costTmp;
            std::cout << "Max frame cost: " << maxCost << " cps: " << CLOCKS_PER_SEC << " details: " << stamp2-stamp1 << ' ' << stamp3-stamp2 << ' ' << stamp4-stamp3 << '\n';
        }
        frame_count++;
#ifdef __MACH__ // macos need to update window title on main thread
        if (frame_count % 60 == 0) {
            static float last_time = 0.0f;
            float cur_time = glfwGetTime();
            std::stringstream ss;
            glm::dvec2 curCoord = dPos2DCoord(camera.pos);
            curCoord = curCoord / glm::pi<double>() * 180.0;
            ss << "fps: " << (frame_count - frame_count_last) / (cur_time - last_time) << ", coord: " << curCoord.x << ", " << curCoord.y;
            frame_count_last = frame_count;
            last_time = cur_time;
            glfwSetWindowTitle(window, ss.str().c_str());
        }
#endif
    }

    void updateUniformBuffer(uint32_t currentImage) {
        camera.aspect = swapChainExtent.width / (float) swapChainExtent.height;
        updateCamera(camera, window);
        //TODO
        fParams[0].target = TARGET_TERRAIN;
        fParams[0].model = glm::mat4(1.0f);
        glm::vec3 camPos0 = lvec3(camera.pos - hvec3(fParams[0].wordOffset));
        fParams[0].view = glm::lookAt(camPos0, camPos0+camera.dir, camera.up);
        fParams[0].proj = camera.proj;
        fParams[0].terrainParam.vertexCount = sbs.terrainVertSize;
        fParams[0].terrainParam.instanceCount = 1;
        fParams[0].terrainParam.firstVertex = 0;
        fParams[0].terrainParam.firstInstance = 0;
        fParams[1].target = TARGET_SKY;
        glm::mat4 skyRot = rotByLookAt(camera.pos);
        fParams[1].model = skyRot;
        glm::vec3 camPos1 = lvec3(camera.pos);
        fParams[1].view = glm::lookAt(camPos1, camPos1+camera.dir, camera.up);
        fParams[1].proj = camera.proj;
        fParams[1].skyParam.indexCount = sbs.skyIdxSize;
        fParams[1].skyParam.instanceCount = 1;
        fParams[1].skyParam.firstIndex = 0;
        fParams[1].skyParam.vertexOffset = sbs.terrainVertMax;
        fParams[1].skyParam.firstInstance = 0;
        fParams[2].target = TARGET_WATER;
        fParams[2].model = glm::mat4(1.0f);
        fParams[2].view = fParams[0].view;
        fParams[2].proj = camera.proj;
        camera.getPVInv(fParams[2].height, fParams[2].p_inv, fParams[2].v_inv, fParams[0].wordOffset);
        static int count = 0;
        fParams[2].time = dt * count;
        fParams[2].water_param.indexCount = sbs.water_idx_size;
        fParams[2].water_param.instanceCount = 1;
        fParams[2].water_param.firstIndex = sbs.skyIdxMax;
        fParams[2].water_param.vertexOffset = sbs.terrainVertMax + sbs.skyVertMax;
        fParams[2].water_param.firstInstance = 0;
        glm::dvec2 worldCoord = dPos2DCoord(camera.pos);
        static double pi2 = glm::pi<double>() * 2;
        double cos_lat = glm::cos(worldCoord.x);

        double ele = glm::max(0.0, glm::length(camera.pos) - 1.0);
        static double spanTan = glm::tan(80.0 / 180.0 * glm::pi<double>());
        double waterRadius = glm::max(0.00005, ele * spanTan);
        static glm::dvec2 waveRectSize = glm::dvec2(waveDomainSize, waveDomainSize / cos_lat);
        fParams[2].freqCoord = glm::fmod(worldCoord / waveRectSize * pi2, pi2);
        fParams[2].freqCoord = glm::vec2(fParams[2].freqCoord.y, fParams[2].freqCoord.x);
        fParams[2].waterOffset = water_grid.updateWOffset(worldCoord, waterRadius, fParams[2].bathyUvMid, cos_lat, count++);
        fParams[2].waterRadius = waterRadius;
        fParams[2].bathyRadius = glm::vec2(water_grid.bathyRadius.x, water_grid.bathyRadius.x);
        memcpy(uniformBuffersData[currentImage], fParams, sizeof(FrameParam) * 3);
    }

    void createSyncObjects() {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        imagesInFlight.resize(swapChainImages.size(), VK_NULL_HANDLE);
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                    vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                    vkCreateFence(logicalDevice, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
        }
    }

    // RGBA
    void createTerrainImage(TetrahedraGlobe &globe) {
        uint32_t imageW = globe.mega_texture.w;
        uint32_t imageH = globe.mega_texture.h;
        VkDeviceSize imageSize = imageW * imageH * 4;

        createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                terrainImageBuf, terrainImageSBM);
        void* data;
        createImage(imageW, imageH, 1, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, terrainImage, terrainImageMemory);
    }

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<VKDemo*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
    }
};

int main() {
    VKDemo app;
    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

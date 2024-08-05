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
#include <iomanip>
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
#include "libs/control.h"
#include "libs/water.h"
#include "libs/globe.h"
#include "libs/sky_dome.h"
#include "vars.h"


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

static double UPDATE_DISTANCE = 200 / earthRadiusM;

class Main {

    const std::vector<const char*> requiredValidationLayers = {
        "VK_LAYER_KHRONOS_validation",
    };

    const std::vector<const char*> requiredPhyDevExt = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#ifdef DEBUG
        //VK_KHR_WORKGROUP_MEMORY_EXPLICIT_LAYOUT_EXTENSION_NAME,
        VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
#endif
#ifdef __MACH__ //TODO depend on device extension
        "VK_KHR_portability_subset",
#endif
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
    VkCommandPool cmdPool;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersData;
    std::vector<VkCommandBuffer> commandBuffers;
    VkDescriptorPool descPool;
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
    VkCommandBuffer copyCmds = NULL;

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
    //glm::vec3 initPos = coord2Pos(42.235336f, 3.264666f, 0.000001f); // cliff
    //glm::vec3 initPos = coord2Pos(42.240409f, 3.265893f, 0.000002f); // breaker
    glm::vec3 initPos = coord2Pos(42.25359f, 3.176437f, 0.000001f); // shallow beach
    //glm::vec3 initPos = coord2Pos(42.239286f, 3.201806f, 0.0000005f); // beach
    //glm::vec3 initPos = coord2Pos(42.241766f, 3.186012f, 0.000001f); // surf
    //glm::vec3 initPos = coord2Pos(42.301529f, 3.208839f, 0.000010f); // mont pool
    Camera camera {
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
        auto func = [](GLFWwindow* w, int key, int scancode, int action, int mods) {
            static_cast<Main*>(glfwGetWindowUserPointer(w))->keyCallback(w, key, scancode, action, mods);
        };
        glfwSetKeyCallback(window, func);
    }

    void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        updateControl(key, scancode, action, mods);
    }

    void initVulkan() {
        createInstance(instance, requiredValidationLayers, enableValidationLayers);
        if (enableValidationLayers) CreateDebugUtilsMessengerEXT(instance, nullptr, &debugMessenger);
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
            throw std::runtime_error("failed to create window surface!");
        pickPhysicalDevice(instance, surface, phyDevSurCaps, phyDevGraphFamilyIdx, phyDevPresentFamilyIdx,
                           phyDevComputeFamilyIdx, requiredPhyDevExt, sizeof(FrameParam));
        createLogicalDevice(graphicsQueue, presentQueue, computeQueue,
                            requiredValidationLayers, requiredPhyDevExt,
                            phyDevGraphFamilyIdx, phyDevPresentFamilyIdx, phyDevComputeFamilyIdx, enableValidationLayers);
        createDescriptorSetLayout(gDescSetLayout,
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
            {VK_SHADER_STAGE_ALL, VK_SHADER_STAGE_FRAGMENT_BIT, VK_SHADER_STAGE_ALL, VK_SHADER_STAGE_ALL, VK_SHADER_STAGE_ALL, VK_SHADER_STAGE_VERTEX_BIT});
        createCommandPool(cmdPool, phyDevGraphFamilyIdx);
        copyCmds = beginCmdBuf(cmdPool);
        water_grid.init(computeQueue, cmdPool, camera.pos);
        createStagingBufferStruct();
        terrainImageView = createImageView(terrainImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, 2);
        scatterImageView = createImageView(scatterImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, 3);
        createSampler(terrainSampler);
        createSampler(scatterSampler);
        initRender(WIDTH, HEIGHT);
        createSyncObjects();
        std::thread t1(&Main::updateStagingBufferStruct, this);
        t1.detach();
#ifdef __linux__
        std::thread t2(&Main::displayFps, this);
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
        water_grid.cleanupRender();
        cleanupRender();
        initRender(width, height);
    }

    void initRender(int width, int height) {
        createSwapChain(swapChain, swapChainImages, swapChainExtent, swapChainImageFormat,
                        window, surface, phyDevSurCaps, phyDevGraphFamilyIdx, phyDevPresentFamilyIdx);
        camera.aspect = swapChainExtent.width / (float) swapChainExtent.height;
        createImageViews();
        createRenderPass(renderPass, swapChainImageFormat);
        createGraphicsPipeline(gPipelineLayout, graphicsPipeline, gDescSetLayout, renderPass, swapChainExtent,
                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, "shader", sizeof(Vertex),
                {offsetof(Vertex, pos),      offsetof(Vertex, normal),   offsetof(Vertex, texCoord)},
                {VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32_SFLOAT});
        createDepthResources(swapChainExtent, depthImage, depthImageMemory, depthImageView);
        createFramebuffers(renderPass, swapChainFramebuffers, swapChainImageViews, depthImageView, swapChainExtent);
        createUniformBuffers();
        uint32_t grahDsCount/*TODO*/ = swapChainImages.size() * pingPongCount;
        createDescriptorPool(descPool, grahDsCount, {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, grahDsCount},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, grahDsCount * 5/*terrain + scatter + comp + compNormal + bathy*/},
        });
        createDescriptorSets(descPool, grahDsCount, swapChainImages.size(), gDescSetLayout, renderDescSets,
                uniformBuffers, sizeof(FrameParam), {terrainImageView, scatterImageView}, {terrainSampler, scatterSampler}, water_grid);
        water_grid.initRender(width, height, renderPass, swapChainExtent, gDescSetLayout);
        createCommandBuffers();
    }

    void createDepthResources(VkExtent2D swapChainExtent, VkImage &depthImage, VkDeviceMemory &depthImageMemory, VkImageView &depthImageView) {
        VkFormat depthFormat = findDepthFormat();
        createInitialImage(swapChainExtent.width, swapChainExtent.height, 1, depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory, NULL, NULL, NULL);
        depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 2);
    }

    void displayFpsLinux() {
        while (true) {
            displayFps();
            std::this_thread::sleep_for (std::chrono::milliseconds(500));
        }
    }

    void displayFps() {
        static float last_time = 0.0f;
        float cur_time = glfwGetTime();
        std::stringstream ss;
        glm::dvec2 curCoord = dPos2DCoord(camera.pos);
        curCoord = curCoord / glm::pi<double>() * 180.0;
        ss << "fps: " << (frame_count - frame_count_last) / (cur_time - last_time) << std::fixed << std::setprecision(6)
           << ", coord: " << curCoord.x << ", " << curCoord.y << ", att: " << glm::length(camera.pos) - 1;
        frame_count_last = frame_count;
        last_time = cur_time;
        glfwSetWindowTitle(window, ss.str().c_str());
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
            //std::this_thread::sleep_for (std::chrono::milliseconds(200));
        }
        vkDeviceWaitIdle(logicalDevice);
    }

    void cleanupRender() {
        vkDestroyImageView(logicalDevice, depthImageView, nullptr);
        vkDestroyImage(logicalDevice, depthImage, nullptr);
        vkFreeMemory(logicalDevice, depthImageMemory, nullptr);
        for (auto framebuffer : swapChainFramebuffers) {
            vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
        }
        vkFreeCommandBuffers(logicalDevice, cmdPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
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
        vkDestroyDescriptorPool(logicalDevice, descPool, nullptr);
    }

    void cleanup() {
        cleanupRender();
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
        water_grid.cleanup();
        vkDestroyCommandPool(logicalDevice, cmdPool, nullptr);
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
        water_grid.updateBathymetry(graphicsQueue, cmdPool, NULL);
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
        createInitialImage(scatter_texture_size, scatter_texture_size, scatter_texture_size, VK_FORMAT_R8G8B8A8_SRGB,
                VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                scatterImage, scatterImageMemory, graphicsQueue, cmdPool, skyDome.scatterTexture.data);
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
                    vkFreeCommandBuffers(logicalDevice, cmdPool, 1, &copyCmds);
                    copyCmds = beginCmdBuf(cmdPool);
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
        commandBuffers.resize(swapChainFramebuffers.size() * pingPongCount);
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = cmdPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();
        if (vkAllocateCommandBuffers(logicalDevice, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }

        for (size_t j = 0; j < pingPongCount; j++) {
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
                //
                offsets[0] = sizeof(DrawParam) + sizeof(uint32_t) * lwgCount * 2 + sizeof(LwgMap) * lwgCount;
                vkCmdBindVertexBuffers(commandBuffers[cmdBufIdx], 0, 1, &water_grid.ptclBuf[j], offsets);
                vkCmdBindPipeline(commandBuffers[cmdBufIdx], VK_PIPELINE_BIND_POINT_GRAPHICS, water_grid.pipDebug);
                vkCmdDrawIndirect(commandBuffers[cmdBufIdx], water_grid.ptclBuf[j], 0, 1, 0);
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
            uint32_t vertCopyCnt = 2, idxCopyCnt = 1;
            VkBufferCopy vertCopies[] = {
                {0,                                   0,                                   sizeof(Vertex) * std::min((int)globe.vertices.size(), sbs.terrainVertMax)},
                {sizeof(Vertex) * sbs.terrainVertMax, sizeof(Vertex) * sbs.terrainVertMax, sizeof(Vertex) * std::min((int)skyDome.vertices.size(), sbs.skyVertMax)},
                {}};
            VkBufferCopy idxCoppies[] = {
                {sizeof(Vertex) * (sbs.terrainVertMax + sbs.skyVertMax), 0, sizeof(int) * std::min((int)skyDome.indices.size(), sbs.skyIdxMax)},
                {}};
            static bool copy_water = true;
            if (copy_water) {
                copy_water = false;
                vertCopies[2] = {sizeof(Vertex) * (sbs.terrainVertMax + sbs.skyVertMax) + sizeof(int) * sbs.skyIdxMax, sizeof(Vertex) * (sbs.terrainVertMax + sbs.skyVertMax),
                        sizeof(Vertex) * std::min((int)water_grid.vertices.size(), sbs.water_vert_max)};
                idxCoppies[1] = {sizeof(Vertex) * (sbs.terrainVertMax + sbs.skyVertMax + sbs.water_vert_max) + sizeof(int) * sbs.skyIdxMax, sizeof(int) * sbs.skyIdxMax,
                        sizeof(int) * std::min((int)water_grid.indices.size(), sbs.water_idx_max)};
                vertCopyCnt++;
                idxCopyCnt++;
            }
            vkCmdCopyBuffer(copyCmds, vertStagingBuffer, vertexBuffer, vertCopyCnt, vertCopies);
            vkCmdCopyBuffer(copyCmds, vertStagingBuffer, indexBuffer, idxCopyCnt, idxCoppies);
            pipBarrier(copyCmds, terrainImage,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT);
            copyBufferToImage(copyCmds, terrainImageBuf, terrainImage, globe.mega_texture.w, globe.mega_texture.h, 1, graphicsQueue, cmdPool);
            pipBarrier(copyCmds, terrainImage,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            std::cout << "Copy size: " << globe.vertices.size() << '\n';
            fParams[0].wordOffset = fParams[2].wordOffset = globe.camOffset = globe._camOffset;
            fParams[1].wordOffset = glm::vec3(0);
            water_grid.updateBathymetry(graphicsQueue, cmdPool, copyCmds);
            endCmdBuf(graphicsQueue, cmdPool, copyCmds, false);
            renderUpdate = true;
        }
        static int compSwitch = 0;
        int pingpongIdx = compSwitch++ % pingPongCount;
        clock_t stamp2 = clock();
        updateUniformBuffer(imageIndex, pingpongIdx);
        clock_t stamp3 = clock();

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        int cmdBufOffset = (pingpongIdx) * swapChainImages.size();
        submitInfo.pCommandBuffers = &commandBuffers[cmdBufOffset + imageIndex];
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
        if (frame_count % 60 == 0) displayFps();
#endif
    }

    void updateUniformBuffer(uint32_t currentImage, int pingpongIdx) {
        camera.update(window, glm::dvec2(swapChainExtent.width / 2, swapChainExtent.height / 2));
        static bool cursorDidabled = false;
        if (!cursorDidabled) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            cursorDidabled = true;
        }
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
        fParams[2].model[0].z = glm::length(globe.camOffset) - earthRadius;;
        fParams[2].view = fParams[0].view;
        fParams[2].proj = camera.proj;
        camera.getPVInv(fParams[2].height, fParams[2].p_inv, fParams[2].v_inv, fParams[0].wordOffset);
        static int count = 0;
        fParams[0].time = fParams[1].time = fParams[2].time = control.dt * count;
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
        double waterRadius = 0.00002;
        //double waterRadius = glm::max(0.00002, ele * spanTan);
        static glm::dvec2 waveRectSize = glm::dvec2(waveDomainSize, waveDomainSize / cos_lat);
        fParams[2].freqCoord = glm::fmod(worldCoord / waveRectSize * pi2, pi2);
        fParams[2].freqCoord = glm::vec2(fParams[2].freqCoord.y, fParams[2].freqCoord.x);
        water_grid.updateWOffset(worldCoord, waterRadius, fParams[2], cos_lat, count++, pingpongIdx);
        fParams[2].waterRadius = waterRadius;
        fParams[2].waterRadiusM = waterRadius * earthRadiusM;
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
        createInitialImage(imageW, imageH, 1, VK_FORMAT_R8G8B8A8_SRGB,  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_TILING_OPTIMAL,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, terrainImage, terrainImageMemory, NULL, NULL, NULL);
    }

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<Main*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
    }
};

int main() {
    Main app;
    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

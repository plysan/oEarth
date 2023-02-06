#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cstring>

#include "../libs/water.h"

extern VkPhysicalDevice phyDevice;
extern VkDeviceSize ubAlign;
extern VkDevice logicalDevice;

void createInitialImage(int width, int height, int depth, VkFormat format, void *data, VkImage &image, VkDeviceMemory& imageMemory,
        VkQueue graphicsQueue, VkCommandPool commandPool);

void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

void createImage(uint32_t width, uint32_t height, uint32_t depth, VkFormat format, VkImageLayout imageLayout, VkImageTiling tiling, VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);

void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout,
        VkQueue graphicsQueue, VkCommandPool commandPool, VkCommandBuffer cmdBuf, bool r2w);

void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t depth,
        VkQueue graphicsQueue, VkCommandPool commandPool);

VkCommandBuffer beginSingleTimeCommands(VkCommandPool commandPool);

void endSingleTimeCommands(VkQueue graphicsQueue, VkCommandBuffer commandBuffer, VkCommandPool commandPool);

void createInstance(VkInstance &instance, const std::vector<const char*> requiredValidationLayers, bool enableValidationLayers);

void CreateDebugUtilsMessengerEXT(VkInstance instance, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

void pickPhysicalDevice(VkInstance instance, VkSurfaceKHR &surface, VkSurfaceCapabilitiesKHR &phyDevSurCaps,
        int &phyDevGraphFamilyIdx, int &phyDevPresentFamilyIdx, int &phyDevComputeFamilyIdx,
        const std::vector<const char*> &requiredPhyDevExt, int uniformBufBlockSize);

void createLogicalDevice(VkQueue &graphicsQueue, VkQueue &presentQueue,
        VkQueue &computeQueue, const std::vector<const char*> &requiredValidationLayers, const std::vector<const char*> &requiredPhyDevExt,
        int phyDevGraphFamilyIdx, int phyDevPresentFamilyIdx, int phyDevComputeFamilyIdx, bool enableValidationLayers);

void createSwapChain(VkSwapchainKHR &swapChain, std::vector<VkImage> &swapChainImages, VkExtent2D &swapChainExtent, VkFormat &swapChainImageFormat,
        GLFWwindow* window, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR phyDevSurCaps, int phyDevGraphFamilyIdx, int phyDevPresentFamilyIdx);

VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, int dimension);

void createRenderPass(VkRenderPass &renderPass, VkFormat swapChainImageFormat);

void createDescriptorSetLayout(VkDescriptorSetLayout &descSetLayout,
        std::vector<VkDescriptorType> descriptorTypes, std::vector<VkShaderStageFlags> shaderStageFlags);

std::vector<char> readFile(const std::string& filename);
VkShaderModule createShaderModule(const std::vector<char>& code);

void createGraphicsPipeline(VkPipelineLayout &pipelineLayout, VkPipeline &graphicsPipeline,
        VkDescriptorSetLayout &descSetLayout, VkRenderPass renderPass, VkExtent2D swapChainExtent, int vertInStride,
        std::vector<int> vertInAttrOffset, std::vector<VkFormat> vertInAttrFormat);

void createComputePipeline(VkPipelineLayout &pipelineLayout, VkPipeline &computePipeline, VkDescriptorSetLayout descSetLayout);

void createCommandPool(VkCommandPool &commandPool, int phyDevGraphFamilyIdx);

void createDepthResources(VkExtent2D swapChainExtent,
        VkImage &depthImage, VkDeviceMemory &depthImageMemory, VkImageView &depthImageView);

void createFramebuffers(VkRenderPass renderPass, std::vector<VkFramebuffer> &swapChainFramebuffers,
        std::vector<VkImageView> swapChainImageViews, VkImageView depthImageView, VkExtent2D swapChainExtent);

void createSampler(VkSampler &sampler);

void createDescriptorPool(VkDescriptorPool &descriptorPool, int maxSets, std::vector<VkDescriptorPoolSize> poolSizes);

void createDescriptorSets(VkDescriptorPool descriptorPool, uint32_t descSetCount, uint32_t swapchainCount,
        VkDescriptorSetLayout gDescSetLayout, std::vector<VkDescriptorSet> &renderDescSets,
        std::vector<VkBuffer> &uniformBuffers, int uniformBufferSize, const std::vector<VkImageView> &imageViews,
        const std::vector<VkSampler> &samplers, WaterGrid waterGrid);

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cstring>

void createInitialImage(int width, int height, int depth, VkFormat format, void *data, VkImage &image, VkDeviceMemory& imageMemory,
        VkDevice logicalDevice, VkPhysicalDevice phyDevice, VkQueue graphicsQueue, VkCommandPool commandPool);

void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory,
        VkDevice logicalDevice, VkPhysicalDevice phyDevice);

void createImage(uint32_t width, uint32_t height, uint32_t depth, VkFormat format, VkImageLayout imageLayout, VkImageTiling tiling, VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory, VkDevice logicalDevice, VkPhysicalDevice phyDevice);

void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout,
        VkDevice logicalDevice, VkQueue graphicsQueue, VkCommandPool commandPool, VkCommandBuffer cmdBuf, bool r2w);

void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t depth,
        VkDevice logicalDevice, VkQueue graphicsQueue, VkCommandPool commandPool);

VkCommandBuffer beginSingleTimeCommands(VkDevice logicalDevice, VkCommandPool commandPool);

void endSingleTimeCommands(VkDevice logicalDevice, VkQueue graphicsQueue, VkCommandBuffer commandBuffer, VkCommandPool commandPool);

void createInstance(VkInstance &instance, const std::vector<const char*> requiredValidationLayers, bool enableValidationLayers);

void CreateDebugUtilsMessengerEXT(VkInstance instance, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

void pickPhysicalDevice(VkInstance instance, VkPhysicalDevice &phyDevice, VkSurfaceKHR &surface, VkSurfaceCapabilitiesKHR &phyDevSurCaps,
        int &phyDevGraphFamilyIdx, int &phyDevPresentFamilyIdx, int &phyDevComputeFamilyIdx,
        const std::vector<const char*> &requiredPhyDevExt, int uniformBufBlockSize);

void createLogicalDevice(VkPhysicalDevice phyDevice, VkDevice &logicalDevice, VkQueue &graphicsQueue, VkQueue &presentQueue,
        VkQueue &computeQueue, const std::vector<const char*> &requiredValidationLayers, const std::vector<const char*> &requiredPhyDevExt,
        int phyDevGraphFamilyIdx, int phyDevPresentFamilyIdx, int phyDevComputeFamilyIdx, bool enableValidationLayers);

void createSwapChain(VkPhysicalDevice phyDevice, VkDevice logicalDevice,
        VkSwapchainKHR &swapChain, std::vector<VkImage> &swapChainImages, VkExtent2D &swapChainExtent, VkFormat &swapChainImageFormat,
        GLFWwindow* window, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR phyDevSurCaps, int phyDevGraphFamilyIdx, int phyDevPresentFamilyIdx);

VkImageView createImageView(VkDevice logicalDevice, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, int dimension);

void createRenderPass(VkPhysicalDevice phyDevice, VkDevice logicalDevice, VkRenderPass &renderPass, VkFormat swapChainImageFormat);

void createDescriptorSetLayout(VkDevice logicalDevice, VkDescriptorSetLayout &descSetLayout,
        std::vector<VkDescriptorType> descriptorTypes, std::vector<VkShaderStageFlags> shaderStageFlags);

void createGraphicsPipeline(VkDevice logicalDevice, VkPipelineLayout &pipelineLayout, VkPipeline &graphicsPipeline,
        VkDescriptorSetLayout &descSetLayout, VkRenderPass renderPass, VkExtent2D swapChainExtent, int vertInStride,
        std::vector<int> vertInAttrOffset, std::vector<VkFormat> vertInAttrFormat);

void createComputePipeline(VkDevice logicalDevice, VkPipelineLayout &pipelineLayout, VkPipeline &computePipeline, VkDescriptorSetLayout descSetLayout);

void createCommandPool(VkDevice logicalDevice, VkCommandPool &commandPool, int phyDevGraphFamilyIdx);

void createDepthResources(VkPhysicalDevice phyDevice, VkDevice logicalDevice, VkExtent2D swapChainExtent,
        VkImage &depthImage, VkDeviceMemory &depthImageMemory, VkImageView &depthImageView);

void createFramebuffers(VkDevice logicalDevice, VkRenderPass renderPass, std::vector<VkFramebuffer> &swapChainFramebuffers,
        std::vector<VkImageView> swapChainImageViews, VkImageView depthImageView, VkExtent2D swapChainExtent);

void createSampler(VkPhysicalDevice phyDevice, VkDevice logicalDevice, VkSampler &sampler);

void createDescriptorPool(VkDevice logicalDevice, VkDescriptorPool &descriptorPool, int maxSets, std::vector<VkDescriptorPoolSize> poolSizes);

void createDescriptorSets(VkDevice logicalDevice, VkDescriptorPool descriptorPool, uint32_t descSetCount, uint32_t swapchainCount,
        VkDescriptorSetLayout gDescSetLayout, VkDescriptorSetLayout cDescSetLayout,
        std::vector<VkDescriptorSet> &renderDescSets, std::vector<VkDescriptorSet> &compDescSets,
        std::vector<VkBuffer> &uniformBuffers, int uniformBufferSize, const std::vector<VkImageView> &imageViews,
        const std::vector<VkSampler> &samplers, std::vector<VkImageView> compImgViews, std::vector<VkSampler> compImgSamplers);

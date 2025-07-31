#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cstring>

#include "../libs/water.h"

extern VkPhysicalDevice phyDevice;
extern VkDeviceSize ubAlign;
extern VkDevice logicalDevice;

void createInitialImage(uint32_t width, uint32_t height, uint32_t depth, VkFormat format, VkImageUsageFlags usage, VkImageLayout layout, VkImageTiling tiling,
        VkMemoryPropertyFlags memProps, VkImage &image, VkDeviceMemory& imageMemory, VkQueue queue, VkCommandPool commandPool, void *src_data);

void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

void pipBarrier(VkCommandBuffer &cmdBuf, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
        VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
        VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);

void copyBufferToImage(VkCommandBuffer cmdBuf, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t depth,
        VkQueue graphicsQueue, VkCommandPool commandPool);

VkCommandBuffer beginCmdBuf(VkCommandPool commandPool);

void endCmdBuf(VkQueue graphicsQueue, VkCommandPool cmdPool, VkCommandBuffer commandBuffer, bool cleanup);

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

VkFormat findDepthFormat();

void createRenderPass(VkRenderPass &renderPass, VkFormat swapChainImageFormat, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp depthStoreOp,
                      VkImageLayout depthLayoutInit, VkImageLayout depthLayoutFinal);

void createDescriptorSetLayout(VkDescriptorSetLayout &descSetLayout,
        std::vector<VkDescriptorType> descriptorTypes, std::vector<VkShaderStageFlags> shaderStageFlags);

std::vector<char> readFile(const std::string& filename);
VkShaderModule createShaderModule(const std::vector<char>& code);

void createGraphicsPipeline(VkPipelineLayout &pipelineLayout, VkPipeline &graphicsPipeline, VkDescriptorSetLayout &descSetLayout,
        VkRenderPass renderPass, VkExtent2D swapChainExtent,
        VkPrimitiveTopology topology, const char* shaderName,
        uint32_t vertInStride, std::vector<uint32_t> vertInAttrOffset, std::vector<VkFormat> vertInAttrFormat,
        bool depthWrite);

void createComputePipeline(VkPipelineLayout &pipelineLayout, VkPipeline &computePipeline, VkDescriptorSetLayout descSetLayout);

void createCommandPool(VkCommandPool &commandPool, int phyDevGraphFamilyIdx);

void createFramebuffers(VkRenderPass renderPass, std::vector<VkFramebuffer> &swapChainFramebuffers,
        std::vector<VkImageView> swapChainImageViews, VkImageView depthImageView, VkExtent2D swapChainExtent);

void createSampler(VkSampler &sampler);

void createDescriptorPool(VkDescriptorPool &descriptorPool, int maxSets, std::vector<VkDescriptorPoolSize> poolSizes);

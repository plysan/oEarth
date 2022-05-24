#include <cstring>
#include <vulkan/vulkan.h>

void createStaticImage4(int width, int height, int depth, void *data, VkImage &image, VkDeviceMemory& imageMemory,
        VkDevice logicalDevice, VkPhysicalDevice phyDevice, VkQueue graphicsQueue, VkCommandPool commandPool);

void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory,
        VkDevice logicalDevice, VkPhysicalDevice phyDevice);

void createImage(uint32_t width, uint32_t height, uint32_t depth, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory, VkDevice logicalDevice, VkPhysicalDevice phyDevice);

void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout,
        VkDevice logicalDevice, VkQueue graphicsQueue, VkCommandPool commandPool);

void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t depth,
        VkDevice logicalDevice, VkQueue graphicsQueue, VkCommandPool commandPool);

VkCommandBuffer beginSingleTimeCommands(VkDevice logicalDevice, VkCommandPool commandPool);

void endSingleTimeCommands(VkDevice logicalDevice, VkQueue graphicsQueue, VkCommandBuffer commandBuffer, VkCommandPool commandPool);

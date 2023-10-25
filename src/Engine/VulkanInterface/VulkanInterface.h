#ifndef VULKANINTERFACE_H
#define VULKANINTERFACE_H

#include "cbase.h"

#include <vulkan/vulkan.h>

class ConVar;

class VulkanInterface {
public:
	static ConVar* debug_vulkan;

public:
	VulkanInterface();
	~VulkanInterface();

	void finish();

	inline uint32_t getQueueFamilyIndex() const { return m_iQueueFamilyIndex; }
	inline bool isReady() const { return m_bReady; }
	inline VkInstance getInstance() const { return m_instance; }
	inline VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
	inline VkDevice getDevice() const { return m_device; }


private:
	bool		m_bReady;
	uint32_t	m_iQueueFamilyIndex;
	VkInstance m_instance;
	VkPhysicalDevice m_physicalDevice;
	VkDevice m_device;
	VkCommandPool m_commandPool;
};
extern VulkanInterface* vulkan;

#endif // !VULKANINTERFACE_H

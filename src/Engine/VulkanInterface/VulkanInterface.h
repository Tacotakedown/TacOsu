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



private:
	bool		m_bReady;
	uint32_t	m_iQueueFamilyIndex;
};


#endif // !VULKANINTERFACE_H

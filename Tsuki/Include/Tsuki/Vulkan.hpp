#pragma once

#include <set>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "IntrusivePtr.hpp"

namespace tk {
class Context;
class WSI;
class WSIPlatform;

using HandleCounter = SingleThreadCounter;

using ContextHandle = IntrusivePtr<Context>;

template <typename T>
static const char* VulkanEnumToString(const T value) {
	return nullptr;
}

enum class QueueType { Graphics, Transfer, Compute };
constexpr static const int QueueTypeCount = 3;
template <>
const char* VulkanEnumToString<QueueType>(const QueueType value) {
	switch (value) {
		case QueueType::Graphics:
			return "Graphics";
		case QueueType::Transfer:
			return "Transfer";
		case QueueType::Compute:
			return "Compute";
	}

	return "Unknown";
}

struct ExtensionInfo {
	bool CalibratedTimestamps    = false;
	bool DebugUtils              = false;
	bool GetSurfaceCapabilities2 = false;
	bool Maintenance4            = false;
	bool Surface                 = false;
	bool Synchronization2        = false;
	bool TimelineSemaphore       = false;
	bool ValidationFeatures      = false;
};
struct GPUFeatures {
	vk::PhysicalDeviceFeatures Features;
#ifdef VK_ENABLE_BETA_EXTENSIONS
	vk::PhysicalDevicePortabilitySubsetFeaturesKHR PortabilitySubset;
#endif
	vk::PhysicalDeviceSynchronization2FeaturesKHR Synchronization2;
	vk::PhysicalDeviceTimelineSemaphoreFeatures TimelineSemaphore;
};
struct GPUProperties {
	vk::PhysicalDeviceProperties Properties;
	vk::PhysicalDeviceDriverProperties Driver;
#ifdef VK_ENABLE_BETA_EXTENSIONS
	vk::PhysicalDevicePortabilitySubsetPropertiesKHR PortabilitySubset;
#endif
	vk::PhysicalDeviceTimelineSemaphoreProperties TimelineSemaphore;
};
struct GPUInfo {
	std::vector<vk::ExtensionProperties> AvailableExtensions;
	GPUFeatures AvailableFeatures = {};
	std::vector<vk::LayerProperties> Layers;
	vk::PhysicalDeviceMemoryProperties Memory;
	GPUProperties Properties = {};
	std::vector<vk::QueueFamilyProperties> QueueFamilies;

	GPUFeatures EnabledFeatures = {};
};
struct QueueInfo {
	std::array<uint32_t, QueueTypeCount> Families;
	std::array<uint32_t, QueueTypeCount> Indices;
	std::array<vk::Queue, QueueTypeCount> Queues;

	QueueInfo() {
		std::fill(Families.begin(), Families.end(), VK_QUEUE_FAMILY_IGNORED);
		std::fill(Indices.begin(), Indices.end(), VK_QUEUE_FAMILY_IGNORED);
	}

	bool SameIndex(QueueType a, QueueType b) const {
		return Indices[static_cast<int>(a)] == Indices[static_cast<int>(b)];
	}
	bool SameFamily(QueueType a, QueueType b) const {
		return Families[static_cast<int>(a)] == Families[static_cast<int>(b)];
	}
	bool SameQueue(QueueType a, QueueType b) const {
		return Queues[static_cast<int>(a)] == Queues[static_cast<int>(b)];
	}
	std::vector<uint32_t> UniqueFamilies() const {
		std::set<uint32_t> unique;
		for (const auto& family : Families) {
			if (family != VK_QUEUE_FAMILY_IGNORED) { unique.insert(family); }
		}

		return std::vector<uint32_t>(unique.begin(), unique.end());
	}

	uint32_t& Family(QueueType type) {
		return Families[static_cast<int>(type)];
	}
	const uint32_t& Family(QueueType type) const {
		return Families[static_cast<int>(type)];
	}
	uint32_t& Index(QueueType type) {
		return Indices[static_cast<int>(type)];
	}
	const uint32_t& Index(QueueType type) const {
		return Indices[static_cast<int>(type)];
	}
	vk::Queue& Queue(QueueType type) {
		return Queues[static_cast<int>(type)];
	}
	const vk::Queue& Queue(QueueType type) const {
		return Queues[static_cast<int>(type)];
	}
};

// Simple Helper Functions
inline std::string FormatSize(vk::DeviceSize size) {
	std::ostringstream oss;
	if (size < 1024) {
		oss << size << " B";
	} else if (size < 1024 * 1024) {
		oss << size / 1024.f << " KB";
	} else if (size < 1024 * 1024 * 1024) {
		oss << size / (1024.0f * 1024.0f) << " MB";
	} else {
		oss << size / (1024.0f * 1024.0f * 1024.0f) << " GB";
	}

	return oss.str();
}
}  // namespace tk

#pragma once

#include <set>
#include <sstream>
#include <vulkan/vulkan.hpp>

#include "IntrusiveHashMap.hpp"
#include "IntrusivePtr.hpp"
#include "ObjectPool.hpp"
#include "TemporaryHashMap.hpp"

#define TSUKI_VULKAN_DEBUG
#define TSUKI_VULKAN_MT

namespace tk {
// Forward declarations.
class BindlessDescriptorPool;
struct BindlessDescriptorPoolDeleter;
class Buffer;
struct BufferCreateInfo;
struct BufferDeleter;
class CommandBuffer;
struct CommandBufferDeleter;
class CommandPool;
class Context;
class DescriptorSetAllocator;
struct DescriptorSetLayout;
class Device;
class Fence;
struct FenceDeleter;
class Framebuffer;
class FramebufferAllocator;
class Image;
struct ImageCreateInfo;
struct ImageDeleter;
class ImageView;
struct ImageViewCreateInfo;
struct ImageViewDeleter;
class PipelineLayout;
class Program;
class ProgramBuilder;
struct ProgramResourceLayout;
class RenderPass;
struct RenderPassInfo;
class Sampler;
struct SamplerCreateInfo;
class Semaphore;
struct SemaphoreDeleter;
class Shader;
class ShaderCompiler;
class TransientAttachmentAllocator;
class WSI;
class WSIPlatform;

// Typedefs and usings.
#ifdef TSUKI_VULKAN_MT
using HandleCounter = MultiThreadCounter;
template <typename T>
using VulkanCache = ThreadSafeIntrusiveHashMapReadCached<T>;
template <typename T>
using VulkanCacheReadWrite = ThreadSafeIntrusiveHashMap<T>;
template <typename T>
using VulkanObjectPool = ThreadSafeObjectPool<T>;
#else
using HandleCounter = SingleThreadCounter;
template <typename T>
using VulkanCache = IntrusiveHashMap<T>;
template <typename T>
using VulkanCacheReadWrite = IntrusiveHashMap<T>;
template <typename T>
using VulkanObjectPool = ObjectPool<T>;
#endif
template <typename T>
using HashedObject = IntrusiveHashMapEnabled<T>;

// Handle declarations.
using BindlessDescriptorPoolHandle = IntrusivePtr<BindlessDescriptorPool>;
using BufferHandle                 = IntrusivePtr<Buffer>;
using CommandBufferHandle          = IntrusivePtr<CommandBuffer>;
using ContextHandle                = IntrusivePtr<Context>;
using DeviceHandle                 = IntrusivePtr<Device>;
using FenceHandle                  = IntrusivePtr<Fence>;
using ImageHandle                  = IntrusivePtr<Image>;
using ImageViewHandle              = IntrusivePtr<ImageView>;
using SemaphoreHandle              = IntrusivePtr<Semaphore>;

// Enums and constants.
constexpr static const int DescriptorSetsPerPool      = 16;
constexpr static const int MaxColorAttachments        = 8;
constexpr static const int MaxDescriptorBindings      = 32;
constexpr static const int MaxDescriptorSets          = 4;
constexpr static const int MaxPushConstantSize        = 128;
constexpr static const int MaxSpecializationConstants = 8;
constexpr static const int MaxVertexAttributes        = 16;
constexpr static const int MaxVertexBuffers           = 8;

template <typename T>
static const char* VulkanEnumToString(const T value) {
	return nullptr;
}

enum class BindlessResourceType { ImageFP, ImageInt };
constexpr static const int BindlessResourceTypeCount = 2;
template <>
const char* VulkanEnumToString<BindlessResourceType>(const BindlessResourceType value) {
	switch (value) {
		case BindlessResourceType::ImageFP:
			return "ImageFP";
		case BindlessResourceType::ImageInt:
			return "ImageInt";
	}

	return "Unknown";
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

enum class CommandBufferType {
	Generic       = static_cast<int>(QueueType::Graphics),
	AsyncTransfer = static_cast<int>(QueueType::Transfer),
	AsyncCompute  = static_cast<int>(QueueType::Compute),
	AsyncGraphics = QueueTypeCount
};
constexpr static const int CommandBufferTypeCount = 4;
template <>
const char* VulkanEnumToString<CommandBufferType>(const CommandBufferType value) {
	switch (value) {
		case CommandBufferType::Generic:
			return "Generic";
		case CommandBufferType::AsyncCompute:
			return "AsyncCompute";
		case CommandBufferType::AsyncTransfer:
			return "AsyncTransfer";
		case CommandBufferType::AsyncGraphics:
			return "AsyncGraphics";
	}

	return "Unknown";
}

enum class FormatCompressionType { Uncompressed, BC, ETC, ASTC };
constexpr static const int FormatCompressionTypeCount = 4;
template <>
const char* VulkanEnumToString<FormatCompressionType>(const FormatCompressionType value) {
	switch (value) {
		case FormatCompressionType::Uncompressed:
			return "Uncompressed";
		case FormatCompressionType::BC:
			return "BC";
		case FormatCompressionType::ETC:
			return "ETC";
		case FormatCompressionType::ASTC:
			return "ASTC";
	}

	return "Unknown";
}

enum class ImageLayoutType { Optimal, General };
constexpr static const int ImageLayoutTypeCount = 2;
template <>
const char* VulkanEnumToString<ImageLayoutType>(const ImageLayoutType value) {
	switch (value) {
		case ImageLayoutType::Optimal:
			return "Optimal";
		case ImageLayoutType::General:
			return "General";
	}

	return "Unknown";
}

// Enum is made to line up with the bits in vk::ShaderStageFlagBits.
enum class ShaderStage {
	Vertex                 = 0,
	TessellationControl    = 1,
	TessellationEvaluation = 2,
	Geometry               = 3,
	Fragment               = 4,
	Compute                = 5
};
constexpr static const int ShaderStageCount = 6;
template <>
const char* VulkanEnumToString<ShaderStage>(const ShaderStage value) {
	switch (value) {
		case ShaderStage::Vertex:
			return "Vertex";
		case ShaderStage::TessellationControl:
			return "TessellationControl";
		case ShaderStage::TessellationEvaluation:
			return "TessellationEvaluation";
		case ShaderStage::Geometry:
			return "Geometry";
		case ShaderStage::Fragment:
			return "Fragment";
		case ShaderStage::Compute:
			return "Compute";
	}

	return "Unknown";
}

enum class StockRenderPass { ColorOnly, Depth, DepthStencil };
constexpr static const int StockRenderPassCount = 3;
template <>
const char* VulkanEnumToString<StockRenderPass>(const StockRenderPass value) {
	switch (value) {
		case StockRenderPass::ColorOnly:
			return "ColorOnly";
		case StockRenderPass::Depth:
			return "Depth";
		case StockRenderPass::DepthStencil:
			return "DepthStencil";
	}

	return "Unknown";
}

enum class StockSampler {
	NearestClamp,
	LinearClamp,
	TrilinearClamp,
	NearestWrap,
	LinearWrap,
	TrilinearWrap,
	NearestShadow,
	LinearShadow,
	DefaultGeometryFilterClamp,
	DefaultGeometryFilterWrap
};
constexpr static const int StockSamplerCount = 10;
template <>
const char* VulkanEnumToString<StockSampler>(const StockSampler value) {
	switch (value) {
		case StockSampler::NearestClamp:
			return "NearestClamp";
		case StockSampler::LinearClamp:
			return "LinearClamp";
		case StockSampler::TrilinearClamp:
			return "TrilinearClamp";
		case StockSampler::NearestWrap:
			return "NearestWrap";
		case StockSampler::LinearWrap:
			return "LinearWrap";
		case StockSampler::TrilinearWrap:
			return "TrilinearWrap";
		case StockSampler::NearestShadow:
			return "NearestShadow";
		case StockSampler::LinearShadow:
			return "LinearShadow";
		case StockSampler::DefaultGeometryFilterClamp:
			return "DefaultGeometryFilterClamp";
		case StockSampler::DefaultGeometryFilterWrap:
			return "DefaultGeometryFilterWrap";
	}

	return "Unknown";
}

// Structures
struct ExtensionInfo {
	bool CalibratedTimestamps    = false;
	bool DebugUtils              = false;
	bool GetSurfaceCapabilities2 = false;
	bool Maintenance4            = false;
	bool Surface                 = false;
	bool Synchronization2        = false;
	bool ValidationFeatures      = false;
};
struct GPUFeatures {
	vk::PhysicalDeviceFeatures Features;
	vk::PhysicalDeviceDescriptorIndexingFeatures DescriptorIndexing;
	vk::PhysicalDeviceMaintenance4FeaturesKHR Maintenance4;
#ifdef VK_ENABLE_BETA_EXTENSIONS
	vk::PhysicalDevicePortabilitySubsetFeaturesKHR PortabilitySubset;
#endif
	vk::PhysicalDeviceShaderDrawParametersFeatures ShaderDrawParameters;
	vk::PhysicalDeviceSynchronization2FeaturesKHR Synchronization2;
	vk::PhysicalDeviceTimelineSemaphoreFeatures TimelineSemaphore;
};
struct GPUProperties {
	vk::PhysicalDeviceProperties Properties;
	vk::PhysicalDeviceDescriptorIndexingProperties DescriptorIndexing;
	vk::PhysicalDeviceDriverProperties Driver;
	vk::PhysicalDeviceMaintenance4PropertiesKHR Maintenance4;
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

	bool EnabledBindless        = false;
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

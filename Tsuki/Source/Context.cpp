#include <Tsuki/Context.hpp>
#include <unordered_map>

#include "Log.hpp"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace tk {
static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                          VkDebugUtilsMessageTypeFlagsEXT type,
                                                          const VkDebugUtilsMessengerCallbackDataEXT* data,
                                                          void* userData) {
	switch (severity) {
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			Log::Error("Vulkan", "Vulkan ERROR: {}", data->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			Log::Warning("Vulkan", "Vulkan Warning: {}", data->pMessage);
			break;
		default:
			Log::Debug("Vulkan", "Vulkan: {}", data->pMessage);
			break;
	}

	return VK_FALSE;
}

Context::Context(const std::vector<const char*>& instanceExtensions, const std::vector<const char*>& deviceExtensions) {
	if (!_loader.success()) { throw std::runtime_error("Failed to load Vulkan loader!"); }

	VULKAN_HPP_DEFAULT_DISPATCHER.init(_loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));

	DumpInstanceInformation();
	CreateInstance(instanceExtensions);

	SelectPhysicalDevice(deviceExtensions);
	DumpDeviceInformation();

	CreateDevice(deviceExtensions);
}

Context::~Context() noexcept {
	if (_device) {
		_device.waitIdle();
		_device.destroy();
	}
	if (_instance) {
#ifdef TSUKI_VULKAN_DEBUG
		if (_debugMessenger) { _instance.destroyDebugUtilsMessengerEXT(_debugMessenger); }
#endif
		_instance.destroy();
	}
}

void Context::CreateInstance(const std::vector<const char*>& requiredExtensions) {
	struct Extension {
		std::string Name;
		uint32_t Version;
		std::string Layer;  // Layer will be an empty string if the extension is in the base layer.
	};

	const auto availableLayers = vk::enumerateInstanceLayerProperties();
	std::unordered_map<std::string, Extension> availableExtensions;
	std::vector<const char*> enabledExtensions;
	std::vector<const char*> enabledLayers;

	// Find all of our instance extensions. This will look through all of the available layers, and if an extension is
	// available in multiple ways, it will prefer whatever layer has the highest spec version.
	{
		const auto EnumerateExtensions = [&](const vk::LayerProperties* layer) -> void {
			std::vector<vk::ExtensionProperties> extensions;
			if (layer == nullptr) {
				extensions = vk::enumerateInstanceExtensionProperties(nullptr);
			} else {
				extensions = vk::enumerateInstanceExtensionProperties(std::string(layer->layerName.data()));
			}

			for (const auto& extension : extensions) {
				const std::string name = std::string(extension.extensionName.data());
				Extension ext{name, extension.specVersion, layer ? std::string(layer->layerName.data()) : ""};
				auto it = availableExtensions.find(name);
				if (it == availableExtensions.end() || it->second.Version < ext.Version) { availableExtensions[name] = ext; }
			}
		};
		EnumerateExtensions(nullptr);
		for (const auto& layer : availableLayers) { EnumerateExtensions(&layer); }
	}

	// Enable all of the required extensions and a handful of preferred extensions or layers. If we can't find any of the
	// required extensions, we fail.
	{
		const auto HasLayer = [&availableLayers](const char* layerName) -> bool {
			for (const auto& layer : availableLayers) {
				if (strcmp(layer.layerName, layerName) == 0) { return true; }
			}

			return false;
		};
		const auto TryLayer = [&](const char* layerName) -> bool {
			if (!HasLayer(layerName)) { return false; }
			for (const auto& name : enabledLayers) {
				if (strcmp(name, layerName) == 0) { return true; }
			}
			Log::Trace("Vulkan::Context", "Enabling instance layer '{}'.", layerName);
			enabledLayers.push_back(layerName);
			return true;
		};
		const auto HasExtension = [&availableExtensions](const char* extensionName) -> bool {
			const std::string name(extensionName);
			const auto it = availableExtensions.find(name);
			return it != availableExtensions.end();
		};
		const auto TryExtension = [&](const char* extensionName) -> bool {
			for (const auto& name : enabledExtensions) {
				if (strcmp(name, extensionName) == 0) { return true; }
			}
			if (!HasExtension(extensionName)) { return false; }
			const std::string name(extensionName);
			const auto it = availableExtensions.find(name);
			if (it->second.Layer.length() != 0) { TryLayer(it->second.Layer.c_str()); }
			Log::Trace("Vulkan::Context", "Enabling instance extension '{}'.", extensionName);
			enabledExtensions.push_back(extensionName);
			return true;
		};

		for (const auto& ext : requiredExtensions) {
			if (!TryExtension(ext)) {
				Log::Fatal("Vulkan::Context", "Required instance extension {} could not be enabled!", ext);
				throw std::runtime_error("[Vulkan::Context] Failed to enable required instance extensions!");
			}
		}

#ifdef VK_ENABLE_BETA_EXTENSIONS
		TryExtension("VK_KHR_portability_enumeration");
#endif

		_extensions.DebugUtils = TryExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		_extensions.Surface    = TryExtension(VK_KHR_SURFACE_EXTENSION_NAME);
		if (_extensions.Surface) {
			_extensions.GetSurfaceCapabilities2 = TryExtension(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
		}

#ifdef TSUKI_VULKAN_DEBUG
		TryLayer("VK_LAYER_KHRONOS_validation");

		_extensions.ValidationFeatures = TryExtension(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
#endif
	}

	const vk::ApplicationInfo appInfo(
		"Tsuki", VK_MAKE_API_VERSION(0, 0, 1, 0), "Tsuki", VK_MAKE_API_VERSION(0, 0, 1, 0), VK_API_VERSION_1_2);
	const vk::InstanceCreateInfo instanceCI({}, &appInfo, enabledLayers, enabledExtensions);

#ifdef TSUKI_VULKAN_DEBUG
	const vk::DebugUtilsMessengerCreateInfoEXT debugCI(
		{},
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning,
		vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
			vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
		VulkanDebugCallback,
		this);
	const std::vector<vk::ValidationFeatureEnableEXT> validationEnable = {
		vk::ValidationFeatureEnableEXT::eBestPractices, vk::ValidationFeatureEnableEXT::eSynchronizationValidation};
	const std::vector<vk::ValidationFeatureDisableEXT> validationDisable;
	const vk::ValidationFeaturesEXT validationCI(validationEnable, validationDisable);

	vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT, vk::ValidationFeaturesEXT> chain(
		instanceCI, debugCI, validationCI);
	if (!_extensions.DebugUtils) { chain.unlink<vk::DebugUtilsMessengerCreateInfoEXT>(); }
	if (!_extensions.ValidationFeatures) { chain.unlink<vk::ValidationFeaturesEXT>(); }

	_instance = vk::createInstance(chain.get());
#else
	_instance = vk::createInstance(instanceCI);
#endif

	Log::Debug("Vulkan", "Instance created.");

	VULKAN_HPP_DEFAULT_DISPATCHER.init(_instance);

#ifdef TSUKI_VULKAN_DEBUG
	if (_extensions.DebugUtils) {
		_debugMessenger = _instance.createDebugUtilsMessengerEXT(debugCI);
		Log::Debug("Vulkan", "Debug Messenger created.");
	}
#endif
}

void Context::SelectPhysicalDevice(const std::vector<const char*>& requiredDeviceExtensions) {
	const auto gpus = _instance.enumeratePhysicalDevices();
	for (const auto& gpu : gpus) {
		GPUInfo gpuInfo;

		// Enumerate basic information.
		gpuInfo.AvailableExtensions = gpu.enumerateDeviceExtensionProperties(nullptr);
		gpuInfo.Layers              = gpu.enumerateDeviceLayerProperties();
		gpuInfo.Memory              = gpu.getMemoryProperties();
		gpuInfo.QueueFamilies       = gpu.getQueueFamilyProperties();

		// Find any extensions hidden within enabled layers.
		for (const auto& layer : gpuInfo.Layers) {
			const auto layerExtensions = gpu.enumerateDeviceExtensionProperties(std::string(layer.layerName.data()));
			for (const auto& ext : layerExtensions) {
				auto it = std::find_if(gpuInfo.AvailableExtensions.begin(),
				                       gpuInfo.AvailableExtensions.end(),
				                       [ext](const vk::ExtensionProperties& props) -> bool {
																 return strcmp(props.extensionName, ext.extensionName) == 0;
															 });
				if (it == gpuInfo.AvailableExtensions.end()) {
					gpuInfo.AvailableExtensions.push_back(ext);
				} else if (it->specVersion < ext.specVersion) {
					it->specVersion = ext.specVersion;
				}
			}
		}
		std::sort(gpuInfo.AvailableExtensions.begin(),
		          gpuInfo.AvailableExtensions.end(),
		          [](const vk::ExtensionProperties& a, const vk::ExtensionProperties& b) {
								return std::string(a.extensionName.data()) < std::string(b.extensionName.data());
							});

		const auto HasExtension = [&](const char* extensionName) -> bool {
			return std::find_if(gpuInfo.AvailableExtensions.begin(),
			                    gpuInfo.AvailableExtensions.end(),
			                    [extensionName](const vk::ExtensionProperties& props) -> bool {
														return strcmp(extensionName, props.extensionName) == 0;
													}) != gpuInfo.AvailableExtensions.end();
		};

		// Enumerate all of the properties and features.
		vk::StructureChain<vk::PhysicalDeviceFeatures2,
		                   vk::PhysicalDeviceMaintenance4FeaturesKHR,
#ifdef VK_ENABLE_BETA_EXTENSIONS
		                   vk::PhysicalDevicePortabilitySubsetFeaturesKHR,
#endif
		                   vk::PhysicalDeviceSynchronization2FeaturesKHR,
		                   vk::PhysicalDeviceTimelineSemaphoreFeatures,
		                   vk::PhysicalDeviceShaderDrawParametersFeatures>
			features;
		vk::StructureChain<vk::PhysicalDeviceProperties2,
		                   vk::PhysicalDeviceDriverProperties,
		                   vk::PhysicalDeviceMaintenance4PropertiesKHR,
#ifdef VK_ENABLE_BETA_EXTENSIONS
		                   vk::PhysicalDevicePortabilitySubsetPropertiesKHR,
#endif
		                   vk::PhysicalDeviceTimelineSemaphoreProperties>
			properties;

		if (!HasExtension(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME)) {
			properties.unlink<vk::PhysicalDeviceDriverProperties>();
		}
		if (!HasExtension(VK_KHR_MAINTENANCE_4_EXTENSION_NAME)) {
			features.unlink<vk::PhysicalDeviceMaintenance4FeaturesKHR>();
			properties.unlink<vk::PhysicalDeviceMaintenance4PropertiesKHR>();
		}
#ifdef VK_ENABLE_BETA_EXTENSIONS
		if (!HasExtension(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME)) {
			features.unlink<vk::PhysicalDevicePortabilitySubsetFeaturesKHR>();
			properties.unlink<vk::PhysicalDevicePortabilitySubsetPropertiesKHR>();
		}
#endif
		if (!HasExtension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)) {
			features.unlink<vk::PhysicalDeviceSynchronization2FeaturesKHR>();
		}

		gpu.getFeatures2(&features.get());
		gpu.getProperties2(&properties.get());

		gpuInfo.AvailableFeatures.Features     = features.get().features;
		gpuInfo.AvailableFeatures.Maintenance4 = features.get<vk::PhysicalDeviceMaintenance4FeaturesKHR>();
#ifdef VK_ENABLE_BETA_EXTENSIONS
		gpuInfo.AvailableFeatures.PortabilitySubset = features.get<vk::PhysicalDevicePortabilitySubsetFeaturesKHR>();
#endif
		gpuInfo.AvailableFeatures.Synchronization2     = features.get<vk::PhysicalDeviceSynchronization2FeaturesKHR>();
		gpuInfo.AvailableFeatures.TimelineSemaphore    = features.get<vk::PhysicalDeviceTimelineSemaphoreFeatures>();
		gpuInfo.AvailableFeatures.ShaderDrawParameters = features.get<vk::PhysicalDeviceShaderDrawParametersFeatures>();

		gpuInfo.Properties.Properties   = properties.get().properties;
		gpuInfo.Properties.Driver       = properties.get<vk::PhysicalDeviceDriverProperties>();
		gpuInfo.Properties.Maintenance4 = properties.get<vk::PhysicalDeviceMaintenance4PropertiesKHR>();
#ifdef VK_ENABLE_BETA_EXTENSIONS
		gpuInfo.Properties.PortabilitySubset = properties.get<vk::PhysicalDevicePortabilitySubsetPropertiesKHR>();
#endif
		gpuInfo.Properties.TimelineSemaphore = properties.get<vk::PhysicalDeviceTimelineSemaphoreProperties>();

		// Validate that the device meets requirements.
		bool extensions = true;
		for (const auto& ext : requiredDeviceExtensions) {
			if (!HasExtension(ext)) {
				extensions = false;
				break;
			}
		}
		if (!extensions) { continue; }

		bool graphicsQueue = false;
		for (size_t q = 0; q < gpuInfo.QueueFamilies.size(); ++q) {
			const auto& family = gpuInfo.QueueFamilies[q];
			if (family.queueFlags & vk::QueueFlagBits::eGraphics && family.queueFlags & vk::QueueFlagBits::eCompute) {
				graphicsQueue = true;
				break;
			}
		}
		if (!graphicsQueue) { continue; }

		// We have a winner!
		_gpu     = gpu;
		_gpuInfo = gpuInfo;
		return;
	}

	throw std::runtime_error("Failed to find a compatible physical device!");
}

void Context::CreateDevice(const std::vector<const char*>& requiredExtensions) {
	std::vector<const char*> enabledExtensions;
	// Find and enable all required extensions, and any extensions we would like to have but are not required.
	{
		const auto HasExtension = [&](const char* extensionName) -> bool {
			for (const auto& ext : _gpuInfo.AvailableExtensions) {
				if (strcmp(ext.extensionName, extensionName) == 0) { return true; }
			}
			return false;
		};
		const auto TryExtension = [&](const char* extensionName) -> bool {
			if (!HasExtension(extensionName)) { return false; }
			for (const auto& name : enabledExtensions) {
				if (strcmp(name, extensionName) == 0) { return true; }
			}
			Log::Trace("Vulkan::Context", "Enabling device extension '{}'.", extensionName);
			enabledExtensions.push_back(extensionName);

			return true;
		};
		for (const auto& name : requiredExtensions) {
			if (!TryExtension(name)) {
				Log::Fatal("Vulkan::Context", "Required device extension {} could not be enabled!", name);
				throw std::runtime_error("[Vulkan::Context] Failed to enable required device extensions!");
			}
		}

#ifdef VK_ENABLE_BETA_EXTENSIONS
		// If this extension is available, it is REQUIRED to enable.
		TryExtension(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

		_extensions.CalibratedTimestamps = TryExtension(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);
		_extensions.Maintenance4         = TryExtension(VK_KHR_MAINTENANCE_4_EXTENSION_NAME);
		_extensions.Synchronization2     = TryExtension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
	}

	// Find and assign all of our queues.
	auto familyProps = _gpuInfo.QueueFamilies;
	std::vector<std::vector<float>> familyPriorities(familyProps.size());
	std::vector<vk::DeviceQueueCreateInfo> queueCIs(QueueTypeCount);
	{
		std::vector<uint32_t> nextFamilyIndex(familyProps.size(), 0);

		// Assign each of our Graphics, Compute, and Transfer queues. Prefer finding separate queues for
		// each if at all possible.
		const auto AssignQueue = [&](QueueType type, vk::QueueFlags required, vk::QueueFlags ignored) -> bool {
			for (size_t q = 0; q < familyProps.size(); ++q) {
				auto& family = familyProps[q];
				if ((family.queueFlags & required) != required) { continue; }
				if (family.queueFlags & ignored || family.queueCount == 0) { continue; }

				_queues.Family(type) = q;
				_queues.Index(type)  = nextFamilyIndex[q]++;
				--family.queueCount;
				familyPriorities[q].push_back(1.0f);

				Log::Trace("Vulkan::Context",
				           "Using queue {}.{} for {}.",
				           _queues.Family(type),
				           _queues.Index(type),
				           VulkanEnumToString(type));

				return true;
			}

			return false;
		};

		// First find our main Graphics queue.
		if (!AssignQueue(QueueType::Graphics, vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute, {})) {
			throw vk::IncompatibleDriverError("Could not find a suitable graphics/compute queue!");
		}

		// Then, attempt to find a dedicated compute queue, or any unused queue with compute. Fall back
		// to sharing with graphics.
		if (!AssignQueue(QueueType::Compute, vk::QueueFlagBits::eCompute, vk::QueueFlagBits::eGraphics) &&
		    !AssignQueue(QueueType::Compute, vk::QueueFlagBits::eCompute, {})) {
			_queues.Family(QueueType::Compute) = _queues.Family(QueueType::Graphics);
			_queues.Index(QueueType::Compute)  = _queues.Index(QueueType::Graphics);
			Log::Trace("Vulkan::Context", "Sharing Compute queue with Graphics.");
		}

		// Finally, attempt to find a dedicated transfer queue. Try to avoid graphics/compute, then
		// compute, then just take what we can. Fall back to sharing with compute.
		if (!AssignQueue(QueueType::Transfer,
		                 vk::QueueFlagBits::eTransfer,
		                 vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute) &&
		    !AssignQueue(QueueType::Transfer, vk::QueueFlagBits::eTransfer, vk::QueueFlagBits::eCompute) &&
		    !AssignQueue(QueueType::Transfer, vk::QueueFlagBits::eTransfer, {})) {
			_queues.Family(QueueType::Transfer) = _queues.Family(QueueType::Compute);
			_queues.Index(QueueType::Transfer)  = _queues.Index(QueueType::Compute);
			Log::Trace("Vulkan::Context", "Sharing Transfer queue with Compute.");
		}

		uint32_t familyCount = 0;
		uint32_t queueCount  = 0;
		for (uint32_t i = 0; i < familyProps.size(); ++i) {
			if (nextFamilyIndex[i] > 0) {
				queueCount += nextFamilyIndex[i];
				queueCIs[familyCount++] = vk::DeviceQueueCreateInfo({}, i, nextFamilyIndex[i], familyPriorities[i].data());
			}
		}
		queueCIs.resize(familyCount);
		Log::Trace("Vulkan::Context", "Creating {} queues on {} unique families.", queueCount, familyCount);
	}

	// Determine what features we want to enable.
	vk::StructureChain<vk::PhysicalDeviceFeatures2,
	                   vk::PhysicalDeviceMaintenance4FeaturesKHR,
	                   vk::PhysicalDeviceTimelineSemaphoreFeatures,
	                   vk::PhysicalDeviceShaderDrawParametersFeatures>
		enabledFeaturesChain;
	{
		auto& features = enabledFeaturesChain.get<vk::PhysicalDeviceFeatures2>().features;
		if (_gpuInfo.AvailableFeatures.Features.samplerAnisotropy == VK_TRUE) {
			Log::Trace("Vulkan::Context",
			           "Enabling Sampler Anisotropy (x{}).",
			           _gpuInfo.Properties.Properties.limits.maxSamplerAnisotropy);
			features.samplerAnisotropy = VK_TRUE;
		}
		if (_gpuInfo.AvailableFeatures.Features.depthClamp == VK_TRUE) {
			Log::Trace("Vulkan::Context", "Enabling Depth Clamp.");
			features.depthClamp = VK_TRUE;
		}
		if (_gpuInfo.AvailableFeatures.Features.geometryShader == VK_TRUE) {
			Log::Trace("Vulkan::Context", "Enabling Geometry Shaders.");
			features.geometryShader = VK_TRUE;
		}
		if (_gpuInfo.AvailableFeatures.Features.tessellationShader == VK_TRUE) {
			Log::Trace("Vulkan::Context", "Enabling Tessellation Shaders.");
			features.tessellationShader = VK_TRUE;
		}
		if (_gpuInfo.AvailableFeatures.Features.fillModeNonSolid == VK_TRUE) {
			Log::Trace("Vulkan::Context", "Enabling non-solid fill mode.");
			features.fillModeNonSolid = VK_TRUE;
		}
		if (_gpuInfo.AvailableFeatures.Features.multiDrawIndirect == VK_TRUE) {
			Log::Trace("Vulkan::Context", "Enabling multi-draw indirect.");
			features.multiDrawIndirect = VK_TRUE;
		}

		auto& timelineSemaphore = enabledFeaturesChain.get<vk::PhysicalDeviceTimelineSemaphoreFeatures>();
		if (_gpuInfo.AvailableFeatures.TimelineSemaphore.timelineSemaphore == VK_TRUE) {
			Log::Trace("Vulkan::Context", "Enabling Timeline Semaphores.");
			timelineSemaphore.timelineSemaphore = VK_TRUE;
		}

		auto& shaderDrawParameters = enabledFeaturesChain.get<vk::PhysicalDeviceShaderDrawParametersFeatures>();
		if (_gpuInfo.AvailableFeatures.ShaderDrawParameters.shaderDrawParameters == VK_TRUE) {
			Log::Trace("Vulkan::Context", "Enabling shader draw parameters.");
			shaderDrawParameters.shaderDrawParameters = VK_TRUE;
		}

		auto& maintenance4 = enabledFeaturesChain.get<vk::PhysicalDeviceMaintenance4FeaturesKHR>();
		if (_gpuInfo.AvailableFeatures.Maintenance4.maintenance4 == VK_TRUE) {
			Log::Trace("Vulkan::Context", "Enabling Maintenance 4.");
			maintenance4.maintenance4 = VK_TRUE;
		} else {
			enabledFeaturesChain.unlink<vk::PhysicalDeviceMaintenance4FeaturesKHR>();
		}

		_gpuInfo.EnabledFeatures.Features = features;
		_gpuInfo.EnabledFeatures.TimelineSemaphore =
			enabledFeaturesChain.get<vk::PhysicalDeviceTimelineSemaphoreFeatures>();
	}

	// Create our device.
	const vk::DeviceCreateInfo deviceCI({}, queueCIs, nullptr, enabledExtensions, nullptr);
	vk::StructureChain<vk::DeviceCreateInfo, vk::PhysicalDeviceFeatures2> chain(deviceCI, enabledFeaturesChain.get());

	_device = _gpu.createDevice(chain.get());
	VULKAN_HPP_DEFAULT_DISPATCHER.init(_device);

	// Fetch our created queues.
	for (int q = 0; q < QueueTypeCount; ++q) {
		if (_queues.Families[q] != VK_QUEUE_FAMILY_IGNORED && _queues.Indices[q] != VK_QUEUE_FAMILY_IGNORED) {
			_queues.Queues[q] = _device.getQueue(_queues.Families[q], _queues.Indices[q]);
		}
	}
}

void Context::DumpInstanceInformation() const {
	Log::Trace("Vulkan::Context", "----- Vulkan Global Information -----");

	const auto instanceVersion = vk::enumerateInstanceVersion();
	Log::Trace("Vulkan::Context",
	           "Instance Version: {}.{}.{}.{}",
	           VK_API_VERSION_VARIANT(instanceVersion),
	           VK_API_VERSION_MAJOR(instanceVersion),
	           VK_API_VERSION_MINOR(instanceVersion),
	           VK_API_VERSION_PATCH(instanceVersion));

	const auto instanceExtensions = vk::enumerateInstanceExtensionProperties(nullptr);
	Log::Trace("Vulkan::Context", "Instance Extensions ({}):", instanceExtensions.size());
	for (const auto& ext : instanceExtensions) {
		Log::Trace("Vulkan::Context", " - {} v{}", ext.extensionName, ext.specVersion);
	}

	const auto instanceLayers = vk::enumerateInstanceLayerProperties();
	Log::Trace("Vulkan::Context", "Instance Layers ({}):", instanceLayers.size());
	for (const auto& layer : instanceLayers) {
		Log::Trace("Vulkan::Context",
		           " - {} v{} (Vulkan {}.{}.{}) - {}",
		           layer.layerName,
		           layer.implementationVersion,
		           VK_API_VERSION_MAJOR(layer.specVersion),
		           VK_API_VERSION_MINOR(layer.specVersion),
		           VK_API_VERSION_PATCH(layer.specVersion),
		           layer.description);
		const auto layerExtensions = vk::enumerateInstanceExtensionProperties(std::string(layer.layerName.data()));
		for (const auto& ext : layerExtensions) {
			Log::Trace("Vulkan::Context", "  - {} v{}", ext.extensionName, ext.specVersion);
		}
	}

	Log::Trace("Vulkan::Context", "----- End Vulkan Global Information -----");
}

void Context::DumpDeviceInformation() const {
	const auto HasExtension = [&](const char* name) {
		return std::find_if(_gpuInfo.AvailableExtensions.begin(),
		                    _gpuInfo.AvailableExtensions.end(),
		                    [&](const vk::ExtensionProperties& ext) { return strcmp(name, ext.extensionName) == 0; }) !=
		       _gpuInfo.AvailableExtensions.end();
	};

	Log::Trace("Vulkan::Context", "----- Vulkan Physical Device Info -----");

	Log::Trace("Vulkan::Context", "- Device Name: {}", _gpuInfo.Properties.Properties.deviceName);
	Log::Trace("Vulkan::Context", "- Device Type: {}", vk::to_string(_gpuInfo.Properties.Properties.deviceType));
	Log::Trace("Vulkan::Context",
	           "- Device API Version: {}.{}.{}",
	           VK_API_VERSION_MAJOR(_gpuInfo.Properties.Properties.apiVersion),
	           VK_API_VERSION_MINOR(_gpuInfo.Properties.Properties.apiVersion),
	           VK_API_VERSION_PATCH(_gpuInfo.Properties.Properties.apiVersion));
	Log::Trace("Vulkan::Context",
	           "- Device Driver Version: {}.{}.{}",
	           VK_API_VERSION_MAJOR(_gpuInfo.Properties.Properties.driverVersion),
	           VK_API_VERSION_MINOR(_gpuInfo.Properties.Properties.driverVersion),
	           VK_API_VERSION_PATCH(_gpuInfo.Properties.Properties.driverVersion));

	Log::Trace("Vulkan::Context", "- Layers ({}):", _gpuInfo.Layers.size());
	for (const auto& layer : _gpuInfo.Layers) {
		Log::Trace("Vulkan::Context",
		           " - {} v{} (Vulkan {}.{}.{}) - {}",
		           layer.layerName,
		           layer.implementationVersion,
		           VK_API_VERSION_MAJOR(layer.specVersion),
		           VK_API_VERSION_MINOR(layer.specVersion),
		           VK_API_VERSION_PATCH(layer.specVersion),
		           layer.description);
	}

	Log::Trace("Vulkan::Context", "- Device Extensions ({}):", _gpuInfo.AvailableExtensions.size());
	for (const auto& ext : _gpuInfo.AvailableExtensions) {
		Log::Trace("Vulkan::Context", "  - {} v{}", ext.extensionName, ext.specVersion);
	}

	Log::Trace("Vulkan::Context", "- Memory Heaps ({}):", _gpuInfo.Memory.memoryHeapCount);
	for (size_t i = 0; i < _gpuInfo.Memory.memoryHeapCount; ++i) {
		const auto& heap = _gpuInfo.Memory.memoryHeaps[i];
		Log::Trace("Vulkan::Context", "  - {} {}", FormatSize(heap.size), vk::to_string(heap.flags));
	}
	Log::Trace("Vulkan::Context", "- Memory Types ({}):", _gpuInfo.Memory.memoryTypeCount);
	for (size_t i = 0; i < _gpuInfo.Memory.memoryTypeCount; ++i) {
		const auto& type = _gpuInfo.Memory.memoryTypes[i];
		Log::Trace("Vulkan::Context", "  - Heap {} {}", type.heapIndex, vk::to_string(type.propertyFlags));
	}

	Log::Trace("Vulkan::Context", "- Queue Families ({}):", _gpuInfo.QueueFamilies.size());
	for (size_t i = 0; i < _gpuInfo.QueueFamilies.size(); ++i) {
		const auto& family = _gpuInfo.QueueFamilies[i];
		Log::Trace("Vulkan::Context",
		           "  - Family {}: {} Queues {} Granularity {}x{}x{} TimestampBits {}",
		           i,
		           family.queueCount,
		           vk::to_string(family.queueFlags),
		           family.minImageTransferGranularity.width,
		           family.minImageTransferGranularity.height,
		           family.minImageTransferGranularity.depth,
		           family.timestampValidBits);
	}

	if (static_cast<uint32_t>(_gpuInfo.Properties.Driver.driverID) != 0) {
		Log::Trace("Vulkan::Context", "- Driver:");
		Log::Trace("Vulkan::Context", "  - ID: {}", vk::to_string(_gpuInfo.Properties.Driver.driverID));
		Log::Trace("Vulkan::Context", "  - Name: {}", _gpuInfo.Properties.Driver.driverName);
		Log::Trace("Vulkan::Context", "  - Info: {}", _gpuInfo.Properties.Driver.driverInfo);
		Log::Trace("Vulkan::Context",
		           "  - Conformance Version: {}.{}.{}.{}",
		           _gpuInfo.Properties.Driver.conformanceVersion.major,
		           _gpuInfo.Properties.Driver.conformanceVersion.minor,
		           _gpuInfo.Properties.Driver.conformanceVersion.patch,
		           _gpuInfo.Properties.Driver.conformanceVersion.subminor);
	}

	Log::Trace("Vulkan::Context", "- Features:");
	Log::Trace(
		"Vulkan::Context", "  - Geometry Shader: {}", _gpuInfo.AvailableFeatures.Features.geometryShader == VK_TRUE);
	Log::Trace(
		"Vulkan::Context", "  - Sampler Anisotropy: {}", _gpuInfo.AvailableFeatures.Features.samplerAnisotropy == VK_TRUE);
	Log::Trace("Vulkan::Context",
	           "  - Synchronization 2: {}",
	           _gpuInfo.AvailableFeatures.Synchronization2.synchronization2 == VK_TRUE);
	Log::Trace(
		"Vulkan::Context", "  - Tesselation Shader: {}", _gpuInfo.AvailableFeatures.Features.tessellationShader == VK_TRUE);
	Log::Trace("Vulkan::Context",
	           "  - Timeline Semaphores: {}",
	           _gpuInfo.AvailableFeatures.TimelineSemaphore.timelineSemaphore == VK_TRUE);
	Log::Trace("Vulkan::Context", "  - Wide Lines: {}", _gpuInfo.AvailableFeatures.Features.wideLines == VK_TRUE);

	Log::Trace("Vulkan::Context", "- Properties:");
	if (_gpuInfo.AvailableFeatures.Features.samplerAnisotropy) {
		Log::Trace("Vulkan::Context", "  - Max Anisotropy: {}", _gpuInfo.Properties.Properties.limits.maxSamplerAnisotropy);
	}

#ifdef VK_ENABLE_BETA_EXTENSIONS
	if (HasExtension(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME)) {
		Log::Trace("Vulkan::Context", "- Portability Subset:");
		Log::Trace("Vulkan::Context",
		           "  - Minimum Vertex Binding Stride Alignment: {}",
		           _gpuInfo.Properties.PortabilitySubset.minVertexInputBindingStrideAlignment);

		Log::Trace("Vulkan::Context", "  - The following features are UNAVAILABLE:");
#	define FEAT(flag, name)                                                                                      \
		do {                                                                                                        \
			if (!_gpuInfo.AvailableFeatures.PortabilitySubset.flag) { Log::Trace("Vulkan::Context", "    - " name); } \
		} while (0)
		FEAT(constantAlphaColorBlendFactors, "Constant Alpha Color Blend Factors");
		FEAT(events, "Events");
		FEAT(imageViewFormatReinterpretation, "Image View Format Reinterpretation");
		FEAT(imageViewFormatSwizzle, "Image View Format Swizzle");
		FEAT(imageView2DOn3DImage, "Image View 2D on 3D Image");
		FEAT(multisampleArrayImage, "Multisample Array Image");
		FEAT(mutableComparisonSamplers, "Mutable Comparison Samplers");
		FEAT(pointPolygons, "Point Polygons");
		FEAT(samplerMipLodBias, "Sampler Mip LOD Bias");
		FEAT(separateStencilMaskRef, "Separate Stencil Mask Ref");
		FEAT(shaderSampleRateInterpolationFunctions, "Shader Sample Rate Interpolation Functions");
		FEAT(tessellationIsolines, "Tesselation Isolines");
		FEAT(tessellationPointMode, "Tesselation Point Mode");
		FEAT(vertexAttributeAccessBeyondStride, "Vertex Attribute Access Beyond Stride");
#	undef FEAT
	}
#endif

	Log::Trace("Vulkan::Context", "----- End Vulkan Physical Device Info -----");
}
}  // namespace tk

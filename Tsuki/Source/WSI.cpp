#include <Tsuki/Context.hpp>
#include <Tsuki/Device.hpp>
#include <Tsuki/Semaphore.hpp>
#include <Tsuki/WSI.hpp>

#include "Log.hpp"

namespace tk {
WSI* WSI::_instance = nullptr;

WSI::WSI(std::unique_ptr<WSIPlatform>&& platform, bool srgb) : _platform(std::move(platform)) {
	if (_instance) { throw std::runtime_error("WSI can only be initialized once!"); }
	_instance = this;

	Log::Initialize();
	Log::SetLevel(Log::Level::Trace);
	auto instanceExtensions = _platform->GetInstanceExtensions();
	auto deviceExtensions   = _platform->GetDeviceExtensions();

	_context = MakeHandle<Context>(instanceExtensions, deviceExtensions);
	_device  = MakeHandle<Device>(*_context);
	_surface = _platform->CreateSurface(_context->GetInstance(), _context->GetGPU());
	Log::Trace("Vulkan", "Surface created.");

	auto gpu                = _context->GetGPU();
	uint32_t presentSupport = 0;
	for (auto& family : _context->GetQueueInfo().Families) {
		if (family != VK_QUEUE_FAMILY_IGNORED) {
			if (gpu.getSurfaceSupportKHR(family, _surface)) { presentSupport |= 1u << family; }
		}
	}

	if ((presentSupport & (1u << _context->GetQueueInfo().Family(QueueType::Graphics))) == 0) {
		throw std::runtime_error("Could not find a supported presentation queue!");
	}

	auto formats      = gpu.getSurfaceFormatsKHR(_surface);
	auto presentModes = gpu.getSurfacePresentModesKHR(_surface);

	_format.format = vk::Format::eUndefined;
	for (auto& format : formats) {
		if (format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
			_format = format;
			break;
		}
	}
	if (_format.format == vk::Format::eUndefined) { _format = formats[0]; }

	_presentMode = vk::PresentModeKHR::eFifo;
	for (auto& presentMode : presentModes) {
		if (presentMode == vk::PresentModeKHR::eMailbox) { _presentMode = presentMode; }
	}

	RecreateSwapchain();
}

WSI::~WSI() noexcept {
	_context->GetDevice().waitIdle();
	_context->GetDevice().destroySwapchainKHR(_swapchain);
	_platform->DestroySurface(_context->GetInstance(), _surface);
}

void WSI::BeginFrame() {
	_device->NextFrame();

	if (_suboptimal) {
		RecreateSwapchain();
		_suboptimal = false;
	}
	if (_acquiredImage != std::numeric_limits<uint32_t>::max()) {
		_platform->Update();
		return;
	}

	auto device = _device->GetDevice();

	constexpr static const int retryMax = 3;
	int retry                           = 0;

	_acquiredImage = std::numeric_limits<uint32_t>::max();
	while (retry < retryMax) {
		auto acquire = _device->RequestSemaphore();
		try {
			const auto acquireResult =
				device.acquireNextImageKHR(_swapchain, std::numeric_limits<uint64_t>::max(), acquire->GetSemaphore(), nullptr);

			if (acquireResult.result == vk::Result::eSuboptimalKHR) {
				_suboptimal = true;
				Log::Debug("Vulkan::Swapchain", "Swapchain is suboptimal, will recreate.");
			}

			acquire->SignalExternal();
			_platform->Update();
			_acquiredImage = acquireResult.value;
			_releaseSemaphores[_acquiredImage].Reset();
			_device->SetAcquireSemaphore(_acquiredImage, acquire);
			break;
		} catch (const vk::OutOfDateKHRError& e) {
			RecreateSwapchain();
			++retry;
			continue;
		}
	}
}

void WSI::EndFrame() {
	if (_acquiredImage == std::numeric_limits<uint32_t>::max()) { return; }

	auto device = _context->GetDevice();
	auto queues = _context->GetQueueInfo();
	auto queue  = queues.Queue(QueueType::Graphics);

	_device->EndFrame();
	if (!_device->_swapchainAcquireConsumed) { return; }

	auto release          = _device->ConsumeReleaseSemaphore();
	auto releaseSemaphore = release->GetSemaphore();
	const vk::PresentInfoKHR presentInfo(releaseSemaphore, _swapchain, _acquiredImage);
	vk::Result presentResult = vk::Result::eSuccess;
	try {
		presentResult = queue.presentKHR(presentInfo);
		if (presentResult == vk::Result::eSuboptimalKHR) {
			Log::Debug("Vulkan::Swapchain", "Swapchain is suboptimal, will recreate.");
			_suboptimal = true;
		}
		release->WaitExternal();
		// We have to keep this semaphore handle alive until this swapchain image comes around again.
		_releaseSemaphores[_acquiredImage] = release;
	} catch (const vk::OutOfDateKHRError& e) {
		Log::Debug("Vulkan::Swapchain", "Failed to present out of date swapchain. Recreating.");
		RecreateSwapchain();
	}

	_acquiredImage = std::numeric_limits<uint32_t>::max();
}

void WSI::RequestShutdown() {
	_platform->RequestShutdown();
}

void WSI::RecreateSwapchain() {
	auto gpu    = _context->GetGPU();
	auto device = _context->GetDevice();

	auto capabilities = gpu.getSurfaceCapabilitiesKHR(_surface);

	if (capabilities.maxImageExtent.width == 0 && capabilities.maxImageExtent.height == 0) { return; }

	const auto surfaceSize = _platform->GetSurfaceSize();
	const vk::Extent2D swapchainExtent(surfaceSize.x, surfaceSize.y);
	_extent     = vk::Extent2D(std::clamp(static_cast<uint32_t>(swapchainExtent.width),
                                    capabilities.minImageExtent.width,
                                    capabilities.maxImageExtent.width),
                         std::clamp(static_cast<uint32_t>(swapchainExtent.height),
                                    capabilities.minImageExtent.height,
                                    capabilities.maxImageExtent.height));
	_imageCount = std::clamp(3u, capabilities.minImageCount, capabilities.maxImageCount);

	const vk::SwapchainCreateInfoKHR swapchainCI({},
	                                             _surface,
	                                             _imageCount,
	                                             _format.format,
	                                             _format.colorSpace,
	                                             _extent,
	                                             1,
	                                             vk::ImageUsageFlagBits::eColorAttachment,
	                                             vk::SharingMode::eExclusive,
	                                             {},
	                                             vk::SurfaceTransformFlagBitsKHR::eIdentity,
	                                             vk::CompositeAlphaFlagBitsKHR::eOpaque,
	                                             _presentMode,
	                                             VK_TRUE,
	                                             _swapchain);
	auto newSwapchain = device.createSwapchainKHR(swapchainCI);
	if (_swapchain) { device.destroySwapchainKHR(_swapchain); }
	_acquiredImage = std::numeric_limits<uint32_t>::max();
	_swapchain     = newSwapchain;
	_images        = device.getSwapchainImagesKHR(_swapchain);
	_imageCount    = static_cast<uint32_t>(_images.size());
	_releaseSemaphores.clear();
	_releaseSemaphores.resize(_imageCount);

	_device->SetupSwapchain(*this);
}

void WSI::MaximizeWindow() {
	_platform->MaximizeWindow();
}

void WSI::SetWindowDecorated(bool decorated) {
	_platform->SetWindowDecorated(decorated);
}

void WSI::SetWindowPosition(const glm::ivec2& pos) {
	_platform->SetWindowPosition(pos);
}

void WSI::SetWindowResizable(bool resizable) {
	_platform->SetWindowResizable(resizable);
}

void WSI::SetWindowSize(const glm::uvec2& size) {
	_platform->SetWindowSize(size);
}

void WSI::SetWindowTitle(const std::string& title) {
	_platform->SetWindowTitle(title);
}

InputAction WSI::GetButton(MouseButton button) {
	return _platform->GetButton(button);
}

InputAction WSI::GetKey(Key key) {
	return _platform->GetKey(key);
}
}  // namespace tk

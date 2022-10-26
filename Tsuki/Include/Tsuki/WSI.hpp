#pragma once

#include <glm/glm.hpp>

#include "Common.hpp"
#include "Input.hpp"

namespace tk {
class WSIPlatform {
 public:
	virtual ~WSIPlatform() noexcept = default;

	virtual void Update() = 0;

	virtual vk::SurfaceKHR CreateSurface(vk::Instance instance, vk::PhysicalDevice gpu) const = 0;
	virtual void DestroySurface(vk::Instance instance, vk::SurfaceKHR surface) const          = 0;
	virtual InputAction GetButton(MouseButton) const                                          = 0;
	virtual std::vector<const char*> GetInstanceExtensions() const                            = 0;
	virtual std::vector<const char*> GetDeviceExtensions() const                              = 0;
	virtual InputAction GetKey(Key) const                                                     = 0;
	virtual glm::uvec2 GetSurfaceSize() const                                                 = 0;
	virtual double GetTime() const                                                            = 0;
	virtual glm::uvec2 GetWindowSize() const                                                  = 0;
	virtual bool IsAlive() const                                                              = 0;

	virtual void MaximizeWindow()                         = 0;
	virtual void RequestShutdown()                        = 0;
	virtual void SetWindowDecorated(bool decorated)       = 0;
	virtual void SetWindowPosition(const glm::ivec2& pos) = 0;
	virtual void SetWindowResizable(bool resizable)       = 0;
	virtual void SetWindowSize(const glm::uvec2& size)    = 0;
	virtual void SetWindowTitle(const std::string& title) = 0;
};

class WSI {
	friend class Input;

 public:
	WSI(std::unique_ptr<WSIPlatform>&& platform, bool srgb = true);
	WSI(const WSI&)            = delete;
	WSI& operator=(const WSI&) = delete;
	~WSI() noexcept;

	static WSI* Get() {
		return _instance;
	}

	uint32_t GetAcquiredIndex() const {
		return _acquiredImage;
	}
	Context& GetContext() {
		return *_context;
	}
	Device& GetDevice() {
		return *_device;
	}
	const vk::Extent2D& GetExtent() const {
		return _extent;
	}
	vk::Format GetFormat() const {
		return _format.format;
	}
	glm::uvec2 GetFramebufferSize() const {
		return _platform->GetSurfaceSize();
	}
	size_t GetImageCount() const {
		return _images.size();
	}
	const std::vector<vk::Image>& GetImages() const {
		return _images;
	}
	vk::Image GetImage(uint32_t index) const {
		return _images[index];
	}
	double GetTime() const {
		return _platform->GetTime();
	}
	glm::uvec2 GetWindowSize() const {
		return _platform->GetWindowSize();
	}
	bool IsAlive() const {
		return _platform->IsAlive();
	}

	void BeginFrame();
	void EndFrame();
	void RequestShutdown();

	void MaximizeWindow();
	void SetWindowDecorated(bool decorated);
	void SetWindowPosition(const glm::ivec2& pos);
	void SetWindowResizable(bool resizable);
	void SetWindowSize(const glm::uvec2& size);
	void SetWindowTitle(const std::string& title);

 protected:
	InputAction GetButton(MouseButton);
	InputAction GetKey(Key);

 private:
	static WSI* _instance;

	void RecreateSwapchain();

	std::unique_ptr<WSIPlatform> _platform;
	ContextHandle _context;
	DeviceHandle _device;
	vk::SurfaceKHR _surface;

	uint32_t _acquiredImage = std::numeric_limits<uint32_t>::max();
	vk::SwapchainKHR _swapchain;
	vk::Extent2D _extent;
	vk::SurfaceFormatKHR _format;
	uint32_t _imageCount = 0;
	std::vector<vk::Image> _images;
	vk::PresentModeKHR _presentMode;
	std::vector<SemaphoreHandle> _releaseSemaphores;
	bool _suboptimal = false;
};
}  // namespace tk

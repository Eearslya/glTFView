#ifdef TSUKI_INCLUDE_GLFW

#	define GLFW_INCLUDE_VULKAN
#	include <GLFW/glfw3.h>
#	include <Tsuki/GlfwPlatform.hpp>
#	include <Tsuki/Input.hpp>

namespace tk {
GlfwPlatform::GlfwPlatform() {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

	_window = glfwCreateWindow(1600, 900, "Luna", nullptr, nullptr);
	glfwGetWindowSize(_window, &_windowSize.x, &_windowSize.y);
	glfwGetFramebufferSize(_window, &_framebufferSize.x, &_framebufferSize.y);
	if (glfwGetWindowAttrib(_window, GLFW_MAXIMIZED) != GLFW_TRUE) {
		GLFWmonitor* monitor         = glfwGetPrimaryMonitor();
		const GLFWvidmode* videoMode = glfwGetVideoMode(monitor);
		glfwSetWindowPos(_window, (videoMode->width - _windowSize.x) / 2, (videoMode->height - _windowSize.y) / 2);
	}
	glfwSetWindowUserPointer(_window, this);

	glfwSetMouseButtonCallback(_window, CallbackButton);
	glfwSetCharCallback(_window, CallbackChar);
	glfwSetFramebufferSizeCallback(_window, CallbackFramebufferSize);
	glfwSetKeyCallback(_window, CallbackKey);
	glfwSetCursorPosCallback(_window, CallbackPosition);
	glfwSetScrollCallback(_window, CallbackScroll);
	glfwSetWindowSizeCallback(_window, CallbackWindowSize);
	glfwSetDropCallback(_window, CallbackDrop);

	glfwShowWindow(_window);
}

void GlfwPlatform::Update() {
	glfwPollEvents();
}

vk::SurfaceKHR GlfwPlatform::CreateSurface(vk::Instance instance, vk::PhysicalDevice gpu) const {
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	glfwCreateWindowSurface(instance, _window, nullptr, &surface);

	return surface;
}

void GlfwPlatform::DestroySurface(vk::Instance instance, vk::SurfaceKHR surface) const {
	instance.destroySurfaceKHR(surface);
}

InputAction GlfwPlatform::GetButton(MouseButton button) const {
	return static_cast<InputAction>(glfwGetMouseButton(_window, int(button)));
}

std::vector<const char*> GlfwPlatform::GetDeviceExtensions() const {
	return {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
}

std::vector<const char*> GlfwPlatform::GetInstanceExtensions() const {
	uint32_t extensionCount = 0;
	const char** extensions = glfwGetRequiredInstanceExtensions(&extensionCount);

	return {extensions, extensions + extensionCount};
}

InputAction GlfwPlatform::GetKey(Key key) const {
	return static_cast<InputAction>(glfwGetKey(_window, int(key)));
}

glm::uvec2 GlfwPlatform::GetSurfaceSize() const {
	return _framebufferSize;
}

double GlfwPlatform::GetTime() const {
	return glfwGetTime();
}

glm::uvec2 GlfwPlatform::GetWindowSize() const {
	return _windowSize;
}

bool GlfwPlatform::IsAlive() const {
	return !glfwWindowShouldClose(_window);
}

void GlfwPlatform::MaximizeWindow() {
	glfwMaximizeWindow(_window);
	glfwGetWindowSize(_window, &_windowSize.x, &_windowSize.y);
}

void GlfwPlatform::RequestShutdown() {
	glfwSetWindowShouldClose(_window, GLFW_TRUE);
}

void GlfwPlatform::SetWindowDecorated(bool decorated) {
	glfwSetWindowAttrib(_window, GLFW_DECORATED, decorated ? GLFW_TRUE : GLFW_FALSE);
}

void GlfwPlatform::SetWindowPosition(const glm::ivec2& pos) {
	if (pos.x == -1 && pos.y == -1) {
		if (glfwGetWindowAttrib(_window, GLFW_MAXIMIZED) == GLFW_FALSE) {
			glfwGetWindowSize(_window, &_windowSize.x, &_windowSize.y);
			GLFWmonitor* monitor         = glfwGetPrimaryMonitor();
			const GLFWvidmode* videoMode = glfwGetVideoMode(monitor);
			glfwSetWindowPos(_window, (videoMode->width - _windowSize.x) / 2, (videoMode->height - _windowSize.y) / 2);
		}
	} else {
		glfwSetWindowPos(_window, pos.x, pos.y);
	}
}

void GlfwPlatform::SetWindowResizable(bool resizable) {
	glfwSetWindowAttrib(_window, GLFW_RESIZABLE, resizable ? GLFW_TRUE : GLFW_FALSE);
}

void GlfwPlatform::SetWindowSize(const glm::uvec2& size) {
	glfwRestoreWindow(_window);
	glfwSetWindowSize(_window, size.x, size.y);
	glfwGetFramebufferSize(_window, &_framebufferSize.x, &_framebufferSize.y);
	glfwGetWindowSize(_window, &_windowSize.x, &_windowSize.y);
}

void GlfwPlatform::SetWindowTitle(const std::string& title) {
	glfwSetWindowTitle(_window, title.c_str());
}

void GlfwPlatform::CallbackButton(GLFWwindow* window, int32_t button, int32_t action, int32_t mods) {
	Input::MouseButtonEvent(static_cast<MouseButton>(button), static_cast<InputAction>(action), InputMods(mods));
}

void GlfwPlatform::CallbackChar(GLFWwindow* window, uint32_t codepoint) {
	Input::CharEvent(codepoint);
}

void GlfwPlatform::CallbackDrop(GLFWwindow* window, int count, const char** paths) {
	std::vector<std::filesystem::path> files;
	for (int i = 0; i < count; ++i) { files.push_back(paths[i]); }
	Input::DropEvent(files);
}

void GlfwPlatform::CallbackFramebufferSize(GLFWwindow* window, int w, int h) {
	GlfwPlatform* platform = reinterpret_cast<GlfwPlatform*>(glfwGetWindowUserPointer(window));
	if (platform) { platform->_framebufferSize = {w, h}; }
}

void GlfwPlatform::CallbackKey(GLFWwindow* window, int32_t key, int32_t scancode, int32_t action, int32_t mods) {
	Input::KeyEvent(static_cast<Key>(key), static_cast<InputAction>(action), InputMods(mods));
}

void GlfwPlatform::CallbackPosition(GLFWwindow* window, double x, double y) {
	Input::MouseMovedEvent({x, y});
}

void GlfwPlatform::CallbackScroll(GLFWwindow* window, double xOffset, double yOffset) {
	Input::MouseScrolledEvent({xOffset, yOffset});
}

void GlfwPlatform::CallbackWindowSize(GLFWwindow* window, int w, int h) {
	GlfwPlatform* platform = reinterpret_cast<GlfwPlatform*>(glfwGetWindowUserPointer(window));
	if (platform) { platform->_windowSize = {w, h}; }
}
}  // namespace tk

#endif /* TSUKI_INCLUDE_GLFW */

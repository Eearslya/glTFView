#pragma once

#include <Tsuki/WSI.hpp>

struct GLFWwindow;

namespace tk {
class GlfwPlatform : public WSIPlatform {
 public:
	GlfwPlatform();

	virtual void Update() override;

	virtual vk::SurfaceKHR CreateSurface(vk::Instance instance, vk::PhysicalDevice gpu) const override;
	virtual void DestroySurface(vk::Instance instance, vk::SurfaceKHR surface) const override;
	virtual InputAction GetButton(MouseButton button) const override;
	virtual std::vector<const char*> GetDeviceExtensions() const override;
	virtual std::vector<const char*> GetInstanceExtensions() const override;
	virtual InputAction GetKey(Key key) const override;
	virtual glm::uvec2 GetSurfaceSize() const override;
	virtual double GetTime() const override;
	virtual glm::uvec2 GetWindowSize() const override;
	virtual bool IsAlive() const override;

	virtual void MaximizeWindow() override;
	virtual void RequestShutdown() override;
	virtual void SetWindowDecorated(bool decorated) override;
	virtual void SetWindowPosition(const glm::ivec2& pos) override;
	virtual void SetWindowResizable(bool resizable) override;
	virtual void SetWindowSize(const glm::uvec2& size) override;
	virtual void SetWindowTitle(const std::string& title) override;

 private:
	static void CallbackButton(GLFWwindow* window, int32_t button, int32_t action, int32_t mods);
	static void CallbackChar(GLFWwindow* window, uint32_t codepoint);
	static void CallbackDrop(GLFWwindow* window, int count, const char** paths);
	static void CallbackFramebufferSize(GLFWwindow* window, int w, int h);
	static void CallbackKey(GLFWwindow* window, int32_t key, int32_t scancode, int32_t action, int32_t mods);
	static void CallbackPosition(GLFWwindow* window, double x, double y);
	static void CallbackScroll(GLFWwindow* window, double xOffset, double yOffset);
	static void CallbackWindowSize(GLFWwindow* window, int w, int h);

	GLFWwindow* _window = nullptr;

	glm::ivec2 _framebufferSize = glm::ivec2(0, 0);
	glm::ivec2 _windowSize      = glm::ivec2(1600, 900);
};
}  // namespace tk

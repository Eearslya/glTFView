#include <imgui.h>

#include <Tsuki.hpp>
#include <Tsuki/GlfwPlatform.hpp>
#include <Tsuki/ImGuiRenderer.hpp>
#include <Tsuki/Input.hpp>
#include <iostream>
#include <memory>

#include "Camera.hpp"
#include "Environment.hpp"
#include "Files.hpp"
#include "IconsFontAwesome6.h"
#include "Model.hpp"

template <typename T>
class PerFrameBuffer {
 public:
	PerFrameBuffer(tk::WSI& wsi, vk::BufferUsageFlags usage = vk::BufferUsageFlagBits::eUniformBuffer)
			: _wsi(wsi), _usage(usage) {}

	tk::BufferHandle Buffer() {
		const auto frameIndex = _wsi.GetDevice().GetFrameIndex();
		const tk::BufferCreateInfo bufferCI(tk::BufferDomain::Host, sizeof(T), _usage);
		while (frameIndex >= _buffers.size()) { _buffers.push_back(_wsi.GetDevice().CreateBuffer(bufferCI)); }
		return _buffers[frameIndex];
	}

	T& Data() {
		auto buffer = Buffer();
		return *reinterpret_cast<T*>(buffer->Map());
	}

 private:
	tk::WSI& _wsi;
	const vk::BufferUsageFlags _usage;
	std::vector<tk::BufferHandle> _buffers;
};

class PerFrameImage {
 public:
	PerFrameImage(tk::WSI& wsi, vk::Format format, vk::ImageUsageFlags usage)
			: _wsi(wsi), _format(format), _usage(usage) {}

	const vk::Extent2D& GetExtent() const {
		return _extent;
	}

	tk::ImageHandle Image() {
		assert(_extent.width != 0 && _extent.height != 0);

		const auto frameIndex = _wsi.GetDevice().GetFrameIndex();

		const tk::ImageCreateInfo imageCI{.Domain        = tk::ImageDomain::Physical,
		                                  .Format        = _format,
		                                  .InitialLayout = vk::ImageLayout::eUndefined,
		                                  .Samples       = vk::SampleCountFlagBits::e1,
		                                  .Type          = vk::ImageType::e2D,
		                                  .Usage         = _usage,
		                                  .Width         = _extent.width,
		                                  .Height        = _extent.height,
		                                  .Depth         = 1,
		                                  .ArrayLayers   = 1,
		                                  .MipLevels     = 1};
		while (frameIndex >= _images.size()) { _images.push_back(_wsi.GetDevice().CreateImage(imageCI)); }

		return _images[frameIndex];
	}

	bool Resize(vk::Extent2D extent) {
		if (_extent.width != extent.width || _extent.height != extent.height) {
			_extent = extent;
			_images.clear();

			return true;
		}

		return false;
	}

 private:
	tk::WSI& _wsi;
	vk::Extent2D _extent = {0, 0};
	const vk::Format _format;
	const vk::ImageUsageFlags _usage;
	std::vector<tk::ImageHandle> _images;
};

struct SceneUBO {
	glm::mat4 Projection;
	glm::mat4 View;
	glm::mat4 ViewProjection;
	glm::vec4 ViewPosition;
	glm::vec4 LightPosition;
	float Exposure;
	float Gamma;
	float PrefilterMipLevels;
	float IBLStrength;
};

struct PushConstant {
	glm::mat4 Node   = glm::mat4(1.0f);
	uint32_t Skinned = 0;
};

int main(int argc, const char** argv) {
	auto wsi     = std::make_unique<tk::WSI>(std::make_unique<tk::GlfwPlatform>());
	auto imgui   = std::make_unique<tk::ImGuiRenderer>(*wsi);
	auto& device = wsi->GetDevice();
	ImGuiIO& io  = ImGui::GetIO();

	auto bindlessImages = device.CreateBindlessDescriptorPool(tk::BindlessResourceType::ImageFP, 1, 1024);

	Camera camera;
	camera.SetPerspective(45.0f, 1.0f, 0.01f, 100.0f);
	camera.SetPosition({0, 0, 1});
	camera.SetRotation({0, 0, 0});

	bool viewportActive = false;
	glm::dvec2 mousePos;
	tk::Input::OnMouseMoved += [&](const glm::dvec2& pos) {
		if (!viewportActive) {
			mousePos = pos;
			return;
		}

		const int dX         = mousePos.x - pos.x;
		const int dY         = mousePos.y - pos.y;
		const float rotSpeed = 0.5f;

		if (tk::Input::GetButton(tk::MouseButton::Left) == tk::InputAction::Press) {
			camera.Rotate(glm::vec3(-dY * rotSpeed, -dX * rotSpeed, 0.0f));
		} else if (tk::Input::GetButton(tk::MouseButton::Right) == tk::InputAction::Press) {
			camera.Translate(glm::vec3(0.0f, 0.0f, dY * 0.005f));
		} else if (tk::Input::GetButton(tk::MouseButton::Middle) == tk::InputAction::Press) {
			camera.Translate(glm::vec3(-dX * 0.005f, dY * 0.005f, 0.0f));
		}
		mousePos = pos;
	};
	tk::Input::OnMouseScrolled += [&](const glm::dvec2& offset) {
		if (!viewportActive) { return; }
		camera.Translate(glm::vec3(0.0f, 0.0f, offset.y * -0.1f));
	};

	// ImGui Styling
	{
		io.ConfigWindowsMoveFromTitleBarOnly = true;

		// Style
		{
			auto& style = ImGui::GetStyle();

			// Main
			style.WindowPadding = ImVec2(8.0f, 8.0f);
			style.FramePadding  = ImVec2(5.0f, 3.0f);
			style.CellPadding   = ImVec2(4.0f, 2.0f);

			// Rounding
			style.WindowRounding    = 8.0f;
			style.ChildRounding     = 8.0f;
			style.FrameRounding     = 8.0f;
			style.PopupRounding     = 2.0f;
			style.ScrollbarRounding = 12.0f;
			style.GrabRounding      = 0.0f;
			style.LogSliderDeadzone = 4.0f;
			style.TabRounding       = 4.0f;
		}

		// Fonts
		{
			io.Fonts->Clear();

			io.Fonts->AddFontFromFileTTF("Resources/Fonts/Roboto-SemiMedium.ttf", 16.0f);

			ImFontConfig jpConfig;
			jpConfig.MergeMode = true;
			io.Fonts->AddFontFromFileTTF(
				"Resources/Fonts/NotoSansJP-Medium.otf", 18.0f, &jpConfig, io.Fonts->GetGlyphRangesJapanese());

			ImFontConfig faConfig;
			faConfig.MergeMode                 = true;
			faConfig.PixelSnapH                = true;
			static const ImWchar fontAwesome[] = {ICON_MIN_FA, ICON_MAX_16_FA, 0};
			io.Fonts->AddFontFromFileTTF("Resources/Fonts/FontAwesome6Free-Regular-400.otf", 16.0f, &faConfig, fontAwesome);
			io.Fonts->AddFontFromFileTTF("Resources/Fonts/FontAwesome6Free-Solid-900.otf", 16.0f, &faConfig, fontAwesome);
		}

		imgui->UpdateFontAtlas();
	}

	PushConstant pushConstant             = {};
	SceneUBO sceneData                    = {};
	tk::ImageHandle blackImage            = {};
	tk::ImageHandle whiteImage            = {};
	tk::BufferHandle defaultJointMatrices = {};

	// Default Images
	{
		constexpr uint32_t width    = 4;
		constexpr uint32_t height   = 4;
		constexpr size_t pixelCount = width * height;
		uint32_t pixels[pixelCount];
		tk::ImageInitialData initialImages[6];
		for (int i = 0; i < 6; ++i) { initialImages[i] = tk::ImageInitialData{.Data = &pixels}; }
		const tk::ImageCreateInfo imageCI2D = {
			.Domain        = tk::ImageDomain::Physical,
			.Format        = vk::Format::eR8G8B8A8Unorm,
			.InitialLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			.Samples       = vk::SampleCountFlagBits::e1,
			.Type          = vk::ImageType::e2D,
			.Usage         = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eInputAttachment,
			.Width         = width,
			.Height        = height,
			.Depth         = 1,
			.ArrayLayers   = 1,
			.MipLevels     = 1,
		};

		std::fill(pixels, pixels + pixelCount, 0xff000000);
		blackImage = device.CreateImage(imageCI2D, initialImages);

		std::fill(pixels, pixels + pixelCount, 0xffffffff);
		whiteImage = device.CreateImage(imageCI2D, initialImages);
	}

	// Default Buffers
	{
		const glm::mat4 jointMatrix(1.0f);
		defaultJointMatrices = device.CreateBuffer(
			tk::BufferCreateInfo(tk::BufferDomain::Device, sizeof(glm::mat4), vk::BufferUsageFlagBits::eStorageBuffer),
			&jointMatrix);
	}

	PerFrameBuffer<SceneUBO> sceneBuffers(*wsi);
	PerFrameImage sceneImages(
		*wsi, vk::Format::eR8G8B8A8Srgb, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);

	tk::Program* program    = nullptr;
	tk::Program* progSkybox = nullptr;
	auto LoadShaders        = [&]() {
    tk::Program* basic =
      device.RequestProgram(ReadFile("Resources/Shaders/PBR.vert.glsl"), ReadFile("Resources/Shaders/PBR.frag.glsl"));
    if (basic) { program = basic; }

    tk::Program* skybox = device.RequestProgram(ReadFile("Resources/Shaders/Skybox.vert.glsl"),
                                                ReadFile("Resources/Shaders/Skybox.frag.glsl"));
    if (skybox) { progSkybox = skybox; }
	};
	LoadShaders();
	tk::Input::OnKey += [&](tk::Key key, tk::InputAction action, tk::InputMods mods) {
		if (action == tk::InputAction::Press && key == tk::Key::F5) { LoadShaders(); }
	};

	bool showSkeleton = false;
	std::unique_ptr<Model> model;
	auto LoadModel = [&](const std::filesystem::path& gltfPath) {
		try {
			std::cout << "Loading glTF model " << gltfPath.string() << std::endl;
			auto newModel = std::make_unique<Model>(wsi->GetDevice(), gltfPath);
			model         = std::move(newModel);
		} catch (const std::exception& e) {
			std::cerr << "Failed to load model from '" << gltfPath.string() << "': " << e.what() << std::endl;
			return;
		}

		model->ActiveAnimation = 0;
		const float modelScale =
			(1.0f / std::max(model->AABB[0][0], std::max(model->AABB[1][1], model->AABB[2][2]))) * 0.5f;
		glm::vec3 modelTrans = -glm::vec3(model->AABB[3]);
		modelTrans += -0.5f * glm::vec3(model->AABB[0][0], model->AABB[1][1], model->AABB[2][2]);
		for (auto* node : model->RootNodes) {
			node->Translation += modelTrans * modelScale;
			node->AnimTranslation += modelTrans * modelScale;
			node->Scale *= modelScale;
			node->AnimScale *= modelScale;
		}
		camera.SetPosition({0, 0, 1});
		camera.SetRotation({0, 0, 0});
	};
	LoadModel("Assets/Models/Fox/Fox.gltf");

	std::unique_ptr<Environment> environment;
	auto LoadEnvironment = [&](const std::filesystem::path& envPath) {
		try {
			auto newEnv = std::make_unique<Environment>(wsi->GetDevice(), envPath);
			environment = std::move(newEnv);
		} catch (const std::exception& e) {
			std::cerr << "Failed to load environment from '" << envPath.string() << "': " << e.what() << std::endl;
		}
	};
	LoadEnvironment("Assets/Environments/TokyoBigSight.hdr");

	tk::Input::OnFilesDropped += [&](const std::vector<std::filesystem::path>& paths) {
		const auto& file = paths[0];
		if (file.extension() == ".gltf" || file.extension() == ".glb") {
			LoadModel(file);
		} else if (file.extension() == ".hdr") {
			LoadEnvironment(file);
		}
	};

	while (wsi->IsAlive()) {
		wsi->BeginFrame();
		imgui->BeginFrame();
		imgui->BeginDockspace();
		const auto frameIndex = device.GetFrameIndex();
		const double time     = wsi->GetTime();

		auto cmd = device.RequestCommandBuffer();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		if (ImGui::Begin("Scene")) {
			const auto viewportPos   = ImGui::GetWindowPos();
			const auto viewportBegin = ImGui::GetWindowContentRegionMin();
			const auto viewportSize  = ImGui::GetContentRegionAvail();
			const bool viewportHover = ImGui::IsWindowHovered();

			const auto cursorPos = ImGui::GetCursorPos();
			viewportActive       = viewportHover && cursorPos.x >= viewportBegin.x && cursorPos.x < viewportSize.x &&
			                 cursorPos.y >= viewportBegin.y && cursorPos.y < viewportSize.y;

			if (sceneImages.Resize(vk::Extent2D(viewportSize.x, viewportSize.y))) {
				const auto imageExtent = sceneImages.GetExtent();
				camera.SetAspectRatio(float(imageExtent.width) / float(imageExtent.height));
			}
			sceneData.Projection     = camera.Perspective;
			sceneData.View           = camera.View;
			sceneData.ViewProjection = sceneData.Projection * sceneData.View;
			sceneData.ViewPosition   = glm::vec4(
        -camera.Position.z * glm::sin(glm::radians(camera.Rotation.y)) * glm::cos(glm::radians(camera.Rotation.x)),
        camera.Position.z * glm::sin(glm::radians(camera.Rotation.x)),
        camera.Position.z * glm::cos(glm::radians(camera.Rotation.y)) * glm::cos(glm::radians(camera.Rotation.x)),
        1.0f);
			sceneData.LightPosition      = glm::vec4(10.0f, 10.0f, 10.0f, 1.0f);
			sceneData.Exposure           = 4.5f;
			sceneData.Gamma              = 2.2f;
			sceneData.PrefilterMipLevels = environment ? environment->Prefiltered->GetCreateInfo().MipLevels : 1;
			sceneData.IBLStrength        = environment ? 1.0f : 0.0f;

			const auto WorldToPixel =
				[](const glm::mat4& viewProjection, const glm::vec3& worldPos, const glm::vec2& imageSize) -> glm::vec2 {
				glm::vec4 ndc = viewProjection * glm::vec4(worldPos, 1.0f);
				ndc /= ndc.w;
				glm::vec2 remap01 = glm::clamp((ndc + 1.0f) * 0.5f, 0.0f, 1.0f);
				remap01.y         = 1.0f - remap01.y;
				return remap01 * imageSize;
			};

			auto DrawLine = [&](const glm::vec3& start,
			                    const glm::vec3& end,
			                    ImColor color = ImColor(255, 255, 255, 255),
			                    float width   = 1.0f) {
				const glm::vec2 offset(viewportPos.x + viewportBegin.x, viewportPos.y + viewportBegin.y);
				const glm::vec2 imageSize(viewportSize.x, viewportSize.y);

				const auto startPixel = WorldToPixel(sceneData.ViewProjection, start, imageSize) + offset;
				const auto endPixel   = WorldToPixel(sceneData.ViewProjection, end, imageSize) + offset;

				auto* drawList = ImGui::GetForegroundDrawList();
				drawList->AddLine(ImVec2(startPixel.x, startPixel.y), ImVec2(endPixel.x, endPixel.y), color, width);
			};

			auto sceneImage = sceneImages.Image();
			const vk::ImageMemoryBarrier startBarrier({},
			                                          vk::AccessFlagBits::eColorAttachmentWrite,
			                                          vk::ImageLayout::eUndefined,
			                                          vk::ImageLayout::eColorAttachmentOptimal,
			                                          VK_QUEUE_FAMILY_IGNORED,
			                                          VK_QUEUE_FAMILY_IGNORED,
			                                          sceneImage->GetImage(),
			                                          vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
			cmd->Barrier(vk::PipelineStageFlagBits::eTopOfPipe,
			             vk::PipelineStageFlagBits::eColorAttachmentOutput,
			             {},
			             {},
			             {startBarrier});

			auto depthImage = device.RequestTransientAttachment(sceneImage->GetExtent(), device.GetDefaultDepthFormat());

			auto sceneBuffer    = sceneBuffers.Buffer();
			sceneBuffers.Data() = sceneData;

			tk::RenderPassInfo rp     = {};
			rp.ColorAttachmentCount   = 1;
			rp.ColorAttachments[0]    = sceneImage->GetView().Get();
			rp.ClearAttachments       = 1 << 0;
			rp.StoreAttachments       = 1 << 0;
			rp.DSOps                  = tk::DepthStencilOpBits::ClearDepthStencil;
			rp.DepthStencilAttachment = depthImage->GetView().Get();
			cmd->BeginRenderPass(rp);
			cmd->SetProgram(program);
			cmd->SetUniformBuffer(0, 0, *sceneBuffer);
			cmd->SetTexture(0,
			                1,
			                environment ? *environment->Irradiance->GetView() : *blackImage->GetView(),
			                tk::StockSampler::LinearClamp);
			cmd->SetTexture(0,
			                2,
			                environment ? *environment->Prefiltered->GetView() : *blackImage->GetView(),
			                tk::StockSampler::LinearClamp);
			cmd->SetTexture(
				0, 3, environment ? *environment->BrdfLut->GetView() : *blackImage->GetView(), tk::StockSampler::LinearClamp);
			cmd->SetVertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, Position));
			cmd->SetVertexAttribute(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, Normal));
			cmd->SetVertexAttribute(2, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Vertex, Tangent));
			cmd->SetVertexAttribute(3, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, Texcoord0));
			cmd->SetVertexAttribute(4, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, Texcoord1));
			cmd->SetVertexAttribute(5, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Vertex, Color0));
			cmd->SetVertexAttribute(6, 0, vk::Format::eR32G32B32A32Uint, offsetof(Vertex, Joints0));
			cmd->SetVertexAttribute(7, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Vertex, Weights0));

			std::function<void(const Model&, const Node*)> DrawBone = [&](const Model& model, const Node* node) {
				if (!node->Children.empty()) {
					const auto startTransform = model.Animate ? node->GetAnimGlobalTransform() : node->GetGlobalTransform();
					const glm::vec3 start     = startTransform[3];
					for (const auto* child : node->Children) {
						const auto endTransform = model.Animate ? child->GetAnimGlobalTransform() : child->GetGlobalTransform();
						const glm::vec3 end     = endTransform[3];

						DrawLine(start, end);
						DrawBone(model, child);
					}
				}
			};

			std::function<void(Model&, const Node*)> IterateNode = [&](Model& model, const Node* node) {
				if (node->Mesh) {
					const auto mesh      = node->Mesh;
					const auto skinId    = node->Skin;
					const auto* skin     = skinId >= 0 ? model.Skins[skinId].get() : nullptr;
					pushConstant.Node    = model.Animate ? node->GetAnimGlobalTransform() : node->GetGlobalTransform();
					pushConstant.Skinned = skin ? 1 : 0;

					if (skin) {
						const glm::mat4 invTransform = glm::inverse(pushConstant.Node);
						const size_t jointCount      = skin->Joints.size();
						glm::mat4* jointMatrices     = reinterpret_cast<glm::mat4*>(skin->Buffer->Map());

						for (size_t i = 0; i < jointCount; ++i) {
							const auto jointMat =
								model.Animate ? skin->Joints[i]->GetAnimGlobalTransform() : skin->Joints[i]->GetGlobalTransform();
							jointMatrices[i] = jointMat * skin->InverseBindMatrices[i];
							jointMatrices[i] = invTransform * jointMatrices[i];
						}

						if (showSkeleton) { DrawBone(model, skin->RootNode); }
					}

					cmd->SetVertexBinding(0, *mesh->Buffer, 0, sizeof(Vertex), vk::VertexInputRate::eVertex);
					cmd->SetStorageBuffer(1, 0, skin ? *skin->Buffer : *defaultJointMatrices);
					if (mesh->TotalIndexCount > 0) {
						cmd->SetIndexBuffer(*mesh->Buffer, mesh->IndexOffset, vk::IndexType::eUint32);
					}

					const size_t submeshCount = mesh->Submeshes.size();
					for (size_t i = 0; i < submeshCount; ++i) {
						const auto& submesh  = mesh->Submeshes[i];
						const auto* material = submesh.Material;
						material->Update(device);
						cmd->PushConstants(&pushConstant, 0, sizeof(PushConstant));

						cmd->SetUniformBuffer(2, 0, *material->DataBuffer);
						cmd->SetTexture(2,
						                1,
						                material->Albedo ? *material->Albedo->Image->Image->GetView() : *whiteImage->GetView(),
						                material->Albedo ? material->Albedo->Sampler->Sampler
						                                 : device.RequestSampler(tk::StockSampler::NearestWrap));
						cmd->SetTexture(2,
						                2,
						                material->Normal ? *material->Normal->Image->Image->GetView() : *whiteImage->GetView(),
						                material->Normal ? device.RequestSampler(tk::StockSampler::LinearClamp)
						                                 : device.RequestSampler(tk::StockSampler::NearestWrap));
						cmd->SetTexture(
							2,
							3,
							material->PBR ? *material->PBR->Image->Image->GetView() : *whiteImage->GetView(),
							material->PBR ? material->PBR->Sampler->Sampler : device.RequestSampler(tk::StockSampler::NearestWrap));
						cmd->SetTexture(
							2,
							4,
							material->Occlusion ? *material->Occlusion->Image->Image->GetView() : *whiteImage->GetView(),
							material->Occlusion ? material->Occlusion->Sampler->Sampler
																	: device.RequestSampler(tk::StockSampler::NearestWrap));
						cmd->SetTexture(2,
						                5,
						                material->Emissive ? *material->Emissive->Image->Image->GetView() : *whiteImage->GetView(),
						                material->Emissive ? material->Emissive->Sampler->Sampler
						                                   : device.RequestSampler(tk::StockSampler::NearestWrap));

						cmd->SetCullMode(material->Sidedness == Sidedness::Both ? vk::CullModeFlagBits::eNone
						                                                        : vk::CullModeFlagBits::eBack);

						if (submesh.IndexCount == 0) {
							cmd->Draw(submesh.VertexCount, 1, submesh.FirstVertex, 0);
						} else {
							cmd->DrawIndexed(submesh.IndexCount, 1, submesh.FirstIndex, submesh.FirstVertex, 0);
						}
					}
				}

				for (const auto* child : node->Children) { IterateNode(model, child); }
			};

			auto RenderModel = [&](Model& model) {
				if (model.ActiveAnimation < model.Animations.size()) {
					auto& animation           = model.Animations[model.ActiveAnimation];
					const float animationTime = std::fmod(time, animation->EndTime);

					for (const auto& channel : animation->Channels) {
						const auto& sampler = animation->Samplers[channel.Sampler];
						if (sampler.Interpolation == AnimationInterpolation::CubicSpline) { continue; }

						for (size_t i = 0; i < sampler.Inputs.size() - 1; ++i) {
							if ((animationTime >= sampler.Inputs[i]) && (animationTime <= sampler.Inputs[i + 1])) {
								const float t = (animationTime - sampler.Inputs[i]) / (sampler.Inputs[i + 1] - sampler.Inputs[i]);
								switch (channel.Path) {
									case AnimationPath::Translation: {
										switch (sampler.Interpolation) {
											case AnimationInterpolation::Linear:
												channel.Target->AnimTranslation = glm::mix(sampler.Outputs[i], sampler.Outputs[i + 1], t);
												break;

											case AnimationInterpolation::Step:
												channel.Target->AnimTranslation = sampler.Outputs[i];
												break;

											default:
												break;
										}
									} break;

									case AnimationPath::Rotation: {
										glm::quat q1;
										q1.x = sampler.Outputs[i].x;
										q1.y = sampler.Outputs[i].y;
										q1.z = sampler.Outputs[i].z;
										q1.w = sampler.Outputs[i].w;

										glm::quat q2;
										q2.x = sampler.Outputs[i + 1].x;
										q2.y = sampler.Outputs[i + 1].y;
										q2.z = sampler.Outputs[i + 1].z;
										q2.w = sampler.Outputs[i + 1].w;

										switch (sampler.Interpolation) {
											case AnimationInterpolation::Linear:
												channel.Target->AnimRotation = glm::normalize(glm::slerp(q1, q2, t));
												break;

											case AnimationInterpolation::Step:
												channel.Target->AnimRotation = q1;
												break;

											default:
												break;
										}
									} break;

									case AnimationPath::Scale: {
										switch (sampler.Interpolation) {
											case AnimationInterpolation::Linear:
												channel.Target->AnimScale = glm::mix(sampler.Outputs[i], sampler.Outputs[i + 1], t);
												break;

											case AnimationInterpolation::Step:
												channel.Target->AnimScale = sampler.Outputs[i];
												break;

											default:
												break;
										}
									} break;

									default:
										break;
								}
							}
						}
					}
				}

				for (const auto* node : model.RootNodes) { IterateNode(model, node); }
			};
			if (model) { RenderModel(*model); }

			if (environment) {
				cmd->SetOpaqueState();
				cmd->SetProgram(progSkybox);
				cmd->SetDepthCompareOp(vk::CompareOp::eLessOrEqual);
				cmd->SetDepthWrite(false);
				cmd->SetCullMode(vk::CullModeFlagBits::eFront);
				cmd->SetUniformBuffer(0, 0, *sceneBuffer);
				cmd->SetTexture(1, 0, *environment->Skybox->GetView(), tk::StockSampler::LinearClamp);
				cmd->Draw(36);
			}

			cmd->EndRenderPass();

			const vk::ImageMemoryBarrier endBarrier(vk::AccessFlagBits::eColorAttachmentWrite,
			                                        vk::AccessFlagBits::eShaderRead,
			                                        vk::ImageLayout::eColorAttachmentOptimal,
			                                        vk::ImageLayout::eShaderReadOnlyOptimal,
			                                        VK_QUEUE_FAMILY_IGNORED,
			                                        VK_QUEUE_FAMILY_IGNORED,
			                                        sceneImage->GetImage(),
			                                        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
			cmd->Barrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
			             vk::PipelineStageFlagBits::eFragmentShader,
			             {},
			             {},
			             {endBarrier});

			auto sceneTexture = imgui->Texture(sceneImage->GetView());
			ImGui::Image(reinterpret_cast<ImTextureID>(sceneTexture), viewportSize);
		}
		ImGui::End();
		ImGui::PopStyleVar();

		if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("Tools")) {
				if (ImGui::MenuItem("Reload Shaders", "F5")) { LoadShaders(); }
			}
			ImGui::EndMainMenuBar();
		}

		if (ImGui::Begin("glTF Model")) {
			if (model) {
				ImGui::Text("Model: %s", model->Name.c_str());

				if (model->Animations.size() > 0) {
					ImGui::Checkbox("Animate", &model->Animate);

					const char* currentAnim = model->Animations[model->ActiveAnimation]->Name.c_str();
					int activeAnimation     = model->ActiveAnimation;
					if (ImGui::BeginCombo("Animation", currentAnim)) {
						for (size_t i = 0; i < model->Animations.size(); ++i) {
							const auto& animation = model->Animations[i];
							if (ImGui::Selectable(animation->Name.c_str(), model->ActiveAnimation == i)) { activeAnimation = i; }
						}
						ImGui::EndCombo();
					}
					if (activeAnimation != model->ActiveAnimation) {
						model->ActiveAnimation = activeAnimation;
						model->ResetAnimation();
					}
				}

				ImGui::Checkbox("Show Skeletons", &showSkeleton);
			} else {
				ImGui::Text("No Model Loaded...");
			}
		}
		ImGui::End();

		ImGui::ShowDemoWindow();

		imgui->EndDockspace();
		imgui->Render(cmd, true);

		device.Submit(cmd);
		wsi->EndFrame();
	}

	return 0;
}

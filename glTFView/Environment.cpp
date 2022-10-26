#include "Environment.hpp"

#include <stb_image.h>

#include <Tsuki/Buffer.hpp>
#include <Tsuki/CommandBuffer.hpp>
#include <Tsuki/Device.hpp>
#include <Tsuki/Image.hpp>
#include <Tsuki/RenderPass.hpp>
#include <Tsuki/Shader.hpp>
#include <Tsuki/TextureFormat.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

#include "Files.hpp"

Environment::Environment(tk::Device& device, const std::filesystem::path& envPath) {
	std::cout << "Loading HDR environment map " << envPath.string() << std::endl;

	tk::Program* progCubemap    = nullptr;
	tk::Program* progIrradiance = nullptr;
	tk::Program* progPrefilter  = nullptr;
	tk::Program* progBrdf       = nullptr;
	try {
		progCubemap    = device.RequestProgram(ReadFile("Resources/Shaders/CubeMap.vert.glsl"),
                                        ReadFile("Resources/Shaders/CubeMap.frag.glsl"));
		progIrradiance = device.RequestProgram(ReadFile("Resources/Shaders/CubeMap.vert.glsl"),
		                                       ReadFile("Resources/Shaders/EnvIrradiance.frag.glsl"));
		progPrefilter  = device.RequestProgram(ReadFile("Resources/Shaders/CubeMap.vert.glsl"),
                                          ReadFile("Resources/Shaders/EnvPrefilter.frag.glsl"));
		progBrdf       = device.RequestProgram(ReadFile("Resources/Shaders/EnvBrdf.vert.glsl"),
                                     ReadFile("Resources/Shaders/EnvBrdf.frag.glsl"));

		if (progCubemap == nullptr || progIrradiance == nullptr || progPrefilter == nullptr || progBrdf == nullptr) {
			throw std::runtime_error("Failed to load shaders!");
		}
	} catch (const std::exception& e) { throw std::runtime_error("Failed to load environment shaders!"); }

	tk::ImageHandle baseHdr;
	{
		const auto envData = ReadFileBinary(envPath);
		if (envData.empty()) { throw std::runtime_error("Failed to load environment map!"); }

		int width, height, components;
		stbi_set_flip_vertically_on_load(0);
		float* envPixels =
			stbi_loadf_from_memory(envData.data(), envData.size(), &width, &height, &components, STBI_rgb_alpha);
		if (!envPixels) { throw std::runtime_error("Failed to load environment map!"); }

		const tk::ImageInitialData initialData{.Data = envPixels};
		const auto imageCI = tk::ImageCreateInfo::Immutable2D(width, height, vk::Format::eR32G32B32A32Sfloat, false);
		baseHdr            = device.CreateImage(imageCI, &initialData);

		stbi_image_free(envPixels);
	}

	{
		tk::ImageCreateInfo imageCI{.Domain        = tk::ImageDomain::Physical,
		                            .Format        = vk::Format::eR16G16B16A16Sfloat,
		                            .InitialLayout = vk::ImageLayout::eTransferDstOptimal,
		                            .Samples       = vk::SampleCountFlagBits::e1,
		                            .Type          = vk::ImageType::e2D,
		                            .Usage       = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
		                            .Width       = 1,
		                            .Height      = 1,
		                            .ArrayLayers = 6,
		                            .MipLevels   = 1,
		                            .Flags       = vk::ImageCreateFlagBits::eCubeCompatible};

		imageCI.Width     = 1024;
		imageCI.Height    = 1024;
		imageCI.MipLevels = tk::TextureFormatLayout::MipLevels(imageCI.Width, imageCI.Height, imageCI.Depth);
		Skybox            = device.CreateImage(imageCI);

		imageCI.Width     = 64;
		imageCI.Height    = 64;
		imageCI.MipLevels = tk::TextureFormatLayout::MipLevels(imageCI.Width, imageCI.Height, imageCI.Depth);
		Irradiance        = device.CreateImage(imageCI);

		imageCI.Width     = 512;
		imageCI.Height    = 512;
		imageCI.MipLevels = tk::TextureFormatLayout::MipLevels(imageCI.Width, imageCI.Height, imageCI.Depth);
		Prefiltered       = device.CreateImage(imageCI);
	}

	tk::ImageHandle renderTarget;
	{
		auto imageCI = tk::ImageCreateInfo::RenderTarget(
			Skybox->GetCreateInfo().Width, Skybox->GetCreateInfo().Height, vk::Format::eR16G16B16A16Sfloat);
		imageCI.Usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc;
		renderTarget  = device.CreateImage(imageCI);
	}

	glm::mat4 captureProjection    = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
	const glm::mat4 captureViews[] = {glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(1, 0, 0), glm::vec3(0, -1, 0)),
	                                  glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0)),
	                                  glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, -1, 0), glm::vec3(0, 0, -1)),
	                                  glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)),
	                                  glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, -1, 0)),
	                                  glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, -1, 0))};
	struct PushConstant {
		glm::mat4 ViewProjection;
		float Roughness;
	};

	auto cmd                  = device.RequestCommandBuffer();
	const auto ProcessCubeMap = [&](tk::Program* program, tk::ImageHandle& src, tk::ImageHandle& dst) {
		auto rpInfo                 = tk::RenderPassInfo{};
		rpInfo.ColorAttachmentCount = 1;
		rpInfo.ColorAttachments[0]  = renderTarget->GetView().Get();
		rpInfo.StoreAttachments     = 1 << 0;

		const uint32_t mips = dst->GetCreateInfo().MipLevels;
		const uint32_t dim  = dst->GetCreateInfo().Width;

		for (uint32_t mip = 0; mip < mips; ++mip) {
			const uint32_t mipDim = static_cast<float>(dim * std::pow(0.5f, mip));

			for (uint32_t i = 0; i < 6; ++i) {
				const PushConstant pc{.ViewProjection = captureProjection * captureViews[i],
				                      .Roughness      = static_cast<float>(mip) / static_cast<float>(mips - 1)};
				rpInfo.RenderArea = vk::Rect2D({0, 0}, {mipDim, mipDim});
				cmd->BeginRenderPass(rpInfo);
				cmd->SetProgram(program);
				cmd->SetCullMode(vk::CullModeFlagBits::eNone);
				cmd->SetTexture(0, 0, *src->GetView(), tk::StockSampler::LinearClamp);
				cmd->PushConstants(&pc, 0, sizeof(pc));
				cmd->Draw(36);
				cmd->EndRenderPass();

				const vk::ImageMemoryBarrier barrier(vk::AccessFlagBits::eColorAttachmentWrite,
				                                     vk::AccessFlagBits::eTransferRead,
				                                     vk::ImageLayout::eColorAttachmentOptimal,
				                                     vk::ImageLayout::eTransferSrcOptimal,
				                                     VK_QUEUE_FAMILY_IGNORED,
				                                     VK_QUEUE_FAMILY_IGNORED,
				                                     renderTarget->GetImage(),
				                                     vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
				cmd->Barrier(
					vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eTransfer, {}, {}, {barrier});

				cmd->CopyImage(*dst,
				               *renderTarget,
				               {},
				               {},
				               vk::Extent3D(mipDim, mipDim, 1),
				               vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, mip, i, 1),
				               vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1));

				const vk::ImageMemoryBarrier barrier2(vk::AccessFlagBits::eTransferWrite,
				                                      vk::AccessFlagBits::eColorAttachmentWrite,
				                                      vk::ImageLayout::eTransferSrcOptimal,
				                                      vk::ImageLayout::eColorAttachmentOptimal,
				                                      VK_QUEUE_FAMILY_IGNORED,
				                                      VK_QUEUE_FAMILY_IGNORED,
				                                      renderTarget->GetImage(),
				                                      vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
				cmd->Barrier(
					vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {barrier2});
			}
		}

		const vk::ImageMemoryBarrier barrier(
			vk::AccessFlagBits::eTransferWrite,
			vk::AccessFlagBits::eShaderRead,
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			dst->GetImage(),
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, dst->GetCreateInfo().MipLevels, 0, 6));
		cmd->Barrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {barrier});
	};
	ProcessCubeMap(progCubemap, baseHdr, Skybox);
	ProcessCubeMap(progIrradiance, Skybox, Irradiance);
	ProcessCubeMap(progPrefilter, Skybox, Prefiltered);

	{
		auto imageCI          = tk::ImageCreateInfo::RenderTarget(512, 512, vk::Format::eR16G16Sfloat);
		imageCI.Usage         = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
		imageCI.InitialLayout = vk::ImageLayout::eColorAttachmentOptimal;
		BrdfLut               = device.CreateImage(imageCI);

		auto rpInfo                 = tk::RenderPassInfo{};
		rpInfo.ColorAttachmentCount = 1;
		rpInfo.ColorAttachments[0]  = BrdfLut->GetView().Get();
		rpInfo.StoreAttachments     = 1 << 0;

		cmd->BeginRenderPass(rpInfo);
		cmd->SetProgram(progBrdf);
		cmd->SetCullMode(vk::CullModeFlagBits::eNone);
		cmd->Draw(3);
		cmd->EndRenderPass();

		const vk::ImageMemoryBarrier barrier(vk::AccessFlagBits::eColorAttachmentWrite,
		                                     vk::AccessFlagBits::eShaderRead,
		                                     vk::ImageLayout::eColorAttachmentOptimal,
		                                     vk::ImageLayout::eShaderReadOnlyOptimal,
		                                     VK_QUEUE_FAMILY_IGNORED,
		                                     VK_QUEUE_FAMILY_IGNORED,
		                                     BrdfLut->GetImage(),
		                                     vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
		cmd->Barrier(
			vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {barrier});
	}

	device.Submit(cmd);
}

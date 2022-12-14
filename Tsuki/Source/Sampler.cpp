#include <Tsuki/Device.hpp>
#include <Tsuki/Sampler.hpp>

#include "Log.hpp"

namespace tk {
Sampler::Sampler(Hash hash, Device& device, const SamplerCreateInfo& info)
		: Cookie(device), HashedObject<Sampler>(hash), _device(device), _createInfo(info) {
	const vk::SamplerCreateInfo samplerCI({},
	                                      info.MagFilter,
	                                      info.MinFilter,
	                                      info.MipmapMode,
	                                      info.AddressModeU,
	                                      info.AddressModeV,
	                                      info.AddressModeW,
	                                      info.MipLodBias,
	                                      info.AnisotropyEnable,
	                                      info.MaxAnisotropy,
	                                      info.CompareEnable,
	                                      info.CompareOp,
	                                      info.MinLod,
	                                      info.MaxLod,
	                                      info.BorderColor,
	                                      info.UnnormalizedCoordinates);
	_sampler = _device.GetDevice().createSampler(samplerCI);

	Log::Trace("Vulkan", "Sampler created.");
}

Sampler::~Sampler() noexcept {
	if (_sampler) { _device.GetDevice().destroySampler(_sampler); }
}
}  // namespace tk

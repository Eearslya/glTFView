#pragma once

#include <cstdint>

namespace tk {
class Device;

class Cookie {
 public:
	Cookie(Device& device);

	uint64_t GetCookie() const {
		return _cookie;
	}

 private:
	const uint64_t _cookie;
};
}  // namespace tk

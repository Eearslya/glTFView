#include <Tsuki/Cookie.hpp>
#include <Tsuki/Device.hpp>

namespace tk {
Cookie::Cookie(Device& device) : _cookie(device.AllocateCookie()) {}
}  // namespace tk

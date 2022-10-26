#pragma once

#include "Common.hpp"
#include "InternalSync.hpp"

namespace tk {
struct SemaphoreDeleter {
	void operator()(Semaphore* semaphore);
};

class Semaphore : public IntrusivePtrEnabled<Semaphore, SemaphoreDeleter, HandleCounter>, public InternalSync {
	friend class ObjectPool<Semaphore>;
	friend struct SemaphoreDeleter;

 public:
	~Semaphore() noexcept;

	vk::Semaphore GetSemaphore() const {
		return _semaphore;
	}
	uint64_t GetTimelineValue() const {
		return _value;
	}
	bool IsSignalled() const {
		return _signalled;
	}

	vk::Semaphore Consume();
	vk::Semaphore Release();
	void SignalExternal();
	void SignalPendingWait();
	void WaitExternal();

 private:
	Semaphore(Device& device);
	Semaphore(Device& device, vk::Semaphore semaphore, bool signalled = true, const std::string& debugName = "");
	Semaphore(Device& device, vk::Semaphore semaphore, uint64_t value, const std::string& debugName = "");

	Device& _device;
	vk::Semaphore _semaphore;
	uint64_t _value = 0;
	bool _pending   = false;
	bool _signalled = true;
};
}  // namespace tk

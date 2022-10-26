#pragma once

namespace tk {
class InternalSync {
 public:
	void SetInternalSync() {
		_internalSync = true;
	}

 protected:
	bool _internalSync = false;
};
}  // namespace tk

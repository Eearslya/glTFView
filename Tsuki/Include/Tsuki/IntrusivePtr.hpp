#pragma once

#include <atomic>
#include <memory>

namespace tk {
class SingleThreadCounter {
 public:
	inline void AddReference() {
		_count++;
	}

	inline bool ReleaseReference() {
		return --_count == 0;
	}

 private:
	size_t _count = 1;
};

class MultiThreadCounter {
 public:
	MultiThreadCounter() {
		_count.store(1, std::memory_order_relaxed);
	}

	inline void AddReference() {
		_count.fetch_add(1, std::memory_order_relaxed);
	}

	inline bool ReleaseReference() {
		return _count.fetch_sub(1, std::memory_order_acq_rel) == 1;
	}

 private:
	std::atomic_size_t _count;
};

template <typename T>
class IntrusivePtr;

template <typename T, typename Deleter = std::default_delete<T>, typename ReferenceOps = SingleThreadCounter>
class IntrusivePtrEnabled {
 public:
	using IntrusivePtrT        = IntrusivePtr<T>;
	using EnabledBaseT         = T;
	using EnabledDeleterT      = Deleter;
	using EnabledReferenceOpsT = ReferenceOps;

	IntrusivePtrEnabled()                                      = default;
	IntrusivePtrEnabled(const IntrusivePtrEnabled&)            = delete;
	IntrusivePtrEnabled& operator=(const IntrusivePtrEnabled&) = delete;

	void AddReference() {
		_refCount.AddReference();
	}
	void ReleaseReference() {
		if (_refCount.ReleaseReference()) { Deleter()(static_cast<T*>(this)); }
	}

 protected:
	IntrusivePtrT ReferenceFromThis();

 private:
	ReferenceOps _refCount;
};

template <typename T>
class IntrusivePtr {
 public:
	template <typename U>
	friend class IntrusivePtr;

	IntrusivePtr() = default;
	explicit IntrusivePtr(T* handle) : _data(handle) {}
	IntrusivePtr(const IntrusivePtr& other) {
		*this = other;
	}
	template <typename U>
	IntrusivePtr(const IntrusivePtr<U>& other) {
		*this = other;
	}
	IntrusivePtr(IntrusivePtr&& other) noexcept {
		*this = std::move(other);
	}
	template <typename U>
	IntrusivePtr(IntrusivePtr<U>&& other) noexcept {
		*this = std::move(other);
	}
	~IntrusivePtr() noexcept {
		Reset();
	}

	T* Get() {
		return _data;
	}
	const T* Get() const {
		return _data;
	}

	T* Release() & {
		T* ret = _data;
		_data  = nullptr;

		return ret;
	}
	T* Release() && {
		T* ret = _data;
		_data  = nullptr;

		return ret;
	}
	void Reset() {
		using ReferenceBaseT =
			IntrusivePtrEnabled<typename T::EnabledBaseT, typename T::EnabledDeleterT, typename T::EnabledReferenceOpsT>;

		if (_data) { static_cast<ReferenceBaseT*>(_data)->ReleaseReference(); }
		_data = nullptr;
	}

	IntrusivePtr& operator=(const IntrusivePtr& other) {
		using ReferenceBaseT =
			IntrusivePtrEnabled<typename T::EnabledBaseT, typename T::EnabledDeleterT, typename T::EnabledReferenceOpsT>;

		if (this != &other) {
			Reset();
			_data = other._data;
			if (_data) { static_cast<ReferenceBaseT*>(_data)->AddReference(); }
		}

		return *this;
	}
	template <typename U>
	IntrusivePtr& operator=(const IntrusivePtr<U>& other) {
		static_assert(std::is_base_of<T, U>::value, "Cannot safely assign downcasted intrusive pointers.");

		using ReferenceBaseT =
			IntrusivePtrEnabled<typename T::EnabledBaseT, typename T::EnabledDeleterT, typename T::EnabledReferenceOpsT>;

		Reset();
		_data = static_cast<T*>(other._data);
		if (_data) { static_cast<ReferenceBaseT*>(_data)->AddReference(); }

		return *this;
	}
	IntrusivePtr& operator=(IntrusivePtr&& other) {
		if (this != &other) {
			Reset();
			_data       = other._data;
			other._data = nullptr;
		}

		return *this;
	}
	template <typename U>
	IntrusivePtr& operator=(IntrusivePtr<U>&& other) {
		Reset();
		_data       = other._data;
		other._data = nullptr;

		return *this;
	}

	T* operator->() {
		return _data;
	}
	const T* operator->() const {
		return _data;
	}
	T& operator*() {
		return *_data;
	}
	const T& operator*() const {
		return *_data;
	}

	explicit operator bool() const {
		return _data != nullptr;
	}
	bool operator==(const IntrusivePtr& other) const {
		return _data == other._data;
	}
	bool operator!=(const IntrusivePtr& other) const {
		return _data != other._data;
	}

 private:
	T* _data = nullptr;
};

template <typename T, typename Deleter, typename ReferenceOps>
IntrusivePtr<T> IntrusivePtrEnabled<T, Deleter, ReferenceOps>::ReferenceFromThis() {
	AddReference();

	return IntrusivePtr<T>(static_cast<T*>(this));
}

template <typename Derived>
using DerivedIntrusivePtrT = IntrusivePtr<Derived>;

template <typename T, typename... Args>
DerivedIntrusivePtrT<T> MakeHandle(Args&&... args) {
	return DerivedIntrusivePtrT<T>(new T(std::forward<Args>(args)...));
}

template <typename Base, typename Derived, typename... Args>
typename Base::IntrusivePtrT MakeDerivedHandle(Args&&... args) {
	return typename Base::IntrusivePtrT(new Derived(std::forward<Args>(args)...));
}
}  // namespace tk

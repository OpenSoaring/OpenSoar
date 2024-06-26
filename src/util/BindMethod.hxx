// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <type_traits>
#include <utility>

#ifdef _MSC_VER
# define NoExcept true
#endif // _MSCVER

/**
 * This object stores a function pointer wrapping a method, and a
 * reference to an instance of the method's class.  It can be used to
 * wrap instance methods as callback functions.
 *
 * @param S the plain function signature type
 */
template<typename S=void()>
class BoundMethod;

template<typename R,
#ifndef _MSC_VER
	 bool NoExcept,
#endif  // _MSCVER
	 typename... Args>
class BoundMethod<R(Args...) noexcept(NoExcept)> {
	using function_pointer = R (*)(void *, Args...) noexcept(NoExcept);

	void *instance_;
	function_pointer function;

public:
	/**
	 * Non-initializing trivial constructor
	 */
	constexpr BoundMethod() = default;

	constexpr
	BoundMethod(void *_instance, function_pointer _function) noexcept
		:instance_(_instance), function(_function) {}

	/**
	 * Construct an "undefined" object.  It must not be called,
	 * and its "bool" operator returns false.
	 */
	constexpr BoundMethod(std::nullptr_t) noexcept:function(nullptr) {}

	/**
	 * Was this object initialized with a valid function pointer?
	 */
	constexpr operator bool() const noexcept {
		return function != nullptr;
	}

	R operator()(Args... args) const noexcept(NoExcept) {
		return function(instance_, std::forward<Args>(args)...);
	}
};

namespace BindMethodDetail {

/**
 * Helper class which introspects a method/function pointer type.
 *
 * @param M the method/function pointer type
 */
template<typename M>
struct SignatureHelper;

template<typename R,
#ifndef _MSC_VER
	bool NoExcept,
#endif // _MSCVER
	typename T, typename... Args>
struct SignatureHelper<R (T::*)(Args...) noexcept(NoExcept)> {
	/**
	 * The class which contains the given method (signature).
	 */
	using class_type = T;

	/**
	 * A function type which describes the "plain" function
	 * signature.
	 */
	using plain_signature = R (Args...) noexcept(NoExcept);

	using function_pointer = R (*)(void *, Args...) noexcept(NoExcept);
};

template<typename R,
#ifndef _MSC_VER
	bool NoExcept,
#endif // _MSCVER
	typename... Args>
struct SignatureHelper<R (*)(Args...) noexcept(NoExcept)> {
	using plain_signature = R (Args...) noexcept(NoExcept);

	using function_pointer = R (*)(void *, Args...) noexcept(NoExcept);
};

/**
 * Generate a wrapper function.
 *
 * @param method the method/function pointer
 */
template<typename M, auto method>
struct WrapperGenerator;

template<typename T,
#ifndef _MSC_VER
	bool NoExcept,
#endif // _MSCVER
	 auto method, typename R, typename... Args>
struct WrapperGenerator<R (T::*)(Args...) noexcept(NoExcept), method> {
	static R Invoke(void *_instance, Args... args) noexcept(NoExcept) {
		auto &t = *(T *)_instance;
		return (t.*method)(std::forward<Args>(args)...);
	}
};

template<auto function,
#ifndef _MSC_VER
	bool NoExcept,
#endif // _MSCVER
        typename R, typename... Args>
struct WrapperGenerator<R (*)(Args...) noexcept(NoExcept), function> {
	static R Invoke(void *, Args... args) noexcept(NoExcept) {
		return function(std::forward<Args>(args)...);
	}
};

template<auto method>
typename SignatureHelper<decltype(method)>::function_pointer
MakeWrapperFunction() noexcept
{
	return WrapperGenerator<decltype(method), method>::Invoke;
}

} /* namespace BindMethodDetail */

/**
 * Construct a #BoundMethod instance.
 *
 * @param method the method pointer
 * @param instance the instance of #T to be bound
 */
template<auto method>
constexpr BoundMethod<typename BindMethodDetail::SignatureHelper<decltype(method)>::plain_signature>
BindMethod(typename BindMethodDetail::SignatureHelper<decltype(method)>::class_type &instance) noexcept
{
	return {
		&instance,
		BindMethodDetail::MakeWrapperFunction<method>(),
	};
}

/**
 * Shortcut macro which takes an instance and a method pointer and
 * constructs a #BoundMethod instance.
 */
#define BIND_METHOD(instance, method) \
	BindMethod<method>(instance)

/**
 * Shortcut wrapper for BIND_METHOD() which assumes "*this" is the
 * instance to be bound.
 */
#define BIND_THIS_METHOD(method) BIND_METHOD(*this, &std::remove_reference_t<decltype(*this)>::method)

/**
 * Construct a #BoundMethod instance for a plain function.
 *
 * @param function the function pointer
 */
template<auto function>
constexpr BoundMethod<typename BindMethodDetail::SignatureHelper<decltype(function)>::plain_signature>
BindFunction() noexcept
{
	return {
		nullptr,
		BindMethodDetail::MakeWrapperFunction<function>(),
	};
}

/**
 * Shortcut macro which takes a function pointer and constructs a
 * #BoundMethod instance.
 */
#define BIND_FUNCTION(function) \
	BindFunction<&function>()

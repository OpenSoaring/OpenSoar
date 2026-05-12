// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#ifdef _WIN32
// needed for proj build in CMake ExternalProject_Add, which doesn't define WIN32_LEAN_AND_MEAN
// for GCC it is not necessary, but this is no failure
# include <windows.h>
#endif
#include <synchapi.h>

/**
 * Wrapper for a CRITICAL_SECTION, backend for the Mutex class.
 */
class CriticalSection {
	friend class WindowsCond;

	CRITICAL_SECTION critical_section;

public:
	CriticalSection() noexcept {
		::InitializeCriticalSection(&critical_section);
	}

	~CriticalSection() noexcept {
		::DeleteCriticalSection(&critical_section);
	}

	CriticalSection(const CriticalSection &other) = delete;
	CriticalSection &operator=(const CriticalSection &other) = delete;

	void lock() noexcept {
		::EnterCriticalSection(&critical_section);
	}

	bool try_lock() noexcept {
		return ::TryEnterCriticalSection(&critical_section) != 0;
	}

	void unlock() noexcept {
		::LeaveCriticalSection(&critical_section);
	}
};

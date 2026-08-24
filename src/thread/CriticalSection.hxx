// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#if defined(_WIN32) && defined(_MSC_VER)
/* MSVC needs the full <windows.h> here, because the proj library built as
   a CMake ExternalProject includes this header without WIN32_LEAN_AND_MEAN.
   GCC/MinGW must stay with <synchapi.h> only: <windows.h> drags in
   <wingdi.h>, whose ERROR macro then breaks every enumerator of that
   name (FLARM::MessageType::ERROR in the FLARM driver, for example). */
# include <windows.h>
/* ... and MSVC gets exactly that macro here as well, so take it back
   right away: nothing in this code base uses the GDI region constant,
   and upstream already removes it the same way in the FLARM driver. */
# ifdef ERROR
#  undef ERROR
# endif
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

// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma GCC diagnostic ignored "-Wcast-align"
#endif

#ifdef __clang__
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#endif

/* suppress -Wundef (GCC/clang only; on MSVC the dummy would just
   produce a C4005 redefinition warning) */
#ifdef __GNUC__
#define BOOST_VERSION 0
#endif

#include <boost/json/src.hpp>

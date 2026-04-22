// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <boost/json/fwd.hpp>

class OutputStream;
class AllocatedPath;

namespace Json {

void
Serialize(OutputStream &os, const boost::json::value &v);

void
Save(const boost::json::value &v, const AllocatedPath &filepath);

} // namespace Json

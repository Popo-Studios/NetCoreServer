#pragma once

#include "pch.h"

namespace NetCoreServer {
	class ServerCreationError : public std::runtime_error {
	public:
		ServerCreationError() : std::runtime_error("Failed to create ENet server host") {}
	};
}
#pragma once
#include "pch.h"

namespace NetCoreServer {
	template<typename ContextType>
	class AbstractPacketHandler {
	public:
		virtual void rawHandle(ContextType& context, ENetPeer* peer, const std::vector<uint8_t>& rawData) = 0;
	};
}
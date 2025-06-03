#include "pch.h"
#include "AbstractSession.hpp"
#include "SessionServer.hpp"

namespace NetCoreServer {
	void AbstractSession::sendPacket(uint64_t uid, uint8_t channel, Packet packet) {
		auto peer = server->getPeerByUid(uid);
		if (peer != nullptr) sendPacket(peer, channel, packet);
	}

	void AbstractSession::sendPacket(ENetPeer* peer, uint8_t channel, Packet packet) {
		server->sendPacket(peer, channel, packet);
	}

	const std::optional<uint64_t> AbstractSession::getPeerUid(ENetPeer* peer) {
		return server->getPeerUid(peer);
	}
}
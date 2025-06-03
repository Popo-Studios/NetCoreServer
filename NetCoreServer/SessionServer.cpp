#include "pch.h"
#include "SessionServer.hpp"

namespace NetCoreServer {
	void SessionJoinHandler::rawHandle(Server& server, ENetPeer* peer, const std::vector<uint8_t>& rawData) {
		SessionJoinOption option = PacketUtils::parseRawData<SessionJoinOption>(rawData);
		SessionServer& sessionServer = dynamic_cast<SessionServer&>(server);
		SessionJoinResult result;
		bool isValid = true; // todo
		if (isValid) {
			server.setPeerUid(peer, option.userIdentifier.userId);
			sessionServer.addUser(option.sessionNumber, option.userIdentifier.userId);
			result.success = true;
			result.errorCode = 0;

			Logger::info(server.makeLog(std::format("A user has joined (Uid: {})", option.userIdentifier.userId)));
		} else {
			result.success = false;
			result.errorCode = 1;
		}

		auto flag = sessionServer.getSessionJoinPacketFlag();
		auto packet = PacketUtils::createPacket("JoinSession", result, flag);
		server.sendPacket(peer, server.getSessionChannel(), packet);
	}

}

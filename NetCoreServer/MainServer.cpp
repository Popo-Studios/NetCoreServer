#include "pch.h"
#include "MainServer.hpp"

namespace NetCoreServer {
	void SessionListHandler::handle(Server& server, ENetPeer* peer, const SessionListOption& data) {
		MainServer& mainServer = dynamic_cast<MainServer&>(server);
		auto packet = PacketUtils::createPacket("GetSessionList", mainServer.getSessionList(data), server.getSessionPacketFlag());
		server.sendPacket(peer, server.getSessionChannel(), packet);
	}

	void SessionCreationHandler::handle(Server& server, ENetPeer* peer, const SessionCreationOption& data) {
		MainServer& mainServer = dynamic_cast<MainServer&>(server);
		auto result = mainServer.createNewSession(data);

		auto packet = PacketUtils::createPacket<SessionCreationResult>("CreateSession", result, server.getSessionPacketFlag());
		server.sendPacket(peer, server.getSessionChannel(), packet);
	}

	void LoginHandler::rawHandle(Server& server, ENetPeer* peer, const std::vector<uint8_t>& rawData) {
		LoginData data = PacketUtils::parseRawData<LoginData>(rawData);
		auto result = loginFunc(data);
		if (result.success && result.userIdentifier.has_value()) {
			server.setPeerUid(peer, result.userIdentifier->userId);
		}

		MainServer& mainServer = dynamic_cast<MainServer&>(server);
		auto flag = mainServer.getLoginPacketFlag();
		auto packet = PacketUtils::createPacket<LoginResult>("Login", result, flag);
		server.sendPacket(peer, mainServer.getLoginChannel(), packet);
	}
}
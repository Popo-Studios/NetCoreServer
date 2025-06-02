#include "pch.h"
#include "Session.hpp"
#include "Server.hpp"

namespace NetCoreServer {
	SessionCreationResult SessionManager::createNewSession(const SessionCreationOption& opt) {
		SessionCreationResult result;
		if (sessionGenerators.contains(opt.sessionType)) {
			SessionInfo info = {
				opt.name,
				SessionIdentifier{ 0, 0 },
				opt.maxPlayers,
				0,
				opt.isPrivate,
				opt.password.has_value(),
				usernameProvider(opt.userIdentifier.userId)
			};

			std::shared_ptr<AbstractSession> session = sessionGenerators[opt.sessionType](info, opt);
			session->peerUidFunc = peerUidFunc;

			for (uint16_t i = 0; i < sessionServers.size(); i++) {
				if (sessionTypes[i] == opt.sessionType && sessionServers[i]->getSessionsCount() < sessionServerOption.maxSessions) {
					info.identifier.sessionNumber = sessionServers[i]->attachSession(std::move(session));
					info.identifier.sessionPort = sessionServers[i]->getServerPort();

					result.success = true;
					result.errorCode = 0;
					result.sessionInfo = info;
					
					return result;
				}
			}

			auto size = static_cast<uint16_t>(sessionServers.size());
			if (size >= sessionServerOption.maxSessions) {
				result.success = false;
				result.errorCode = 2;
				return result;
			}

			auto newServer = std::make_unique<SessionServer>(
				sessionServerOption.portRange.first + size,
				sessionServerOption.maxConnection,
				sessionServerOption.maxChannel,
				sessionServerOption.queueSize,
				sessionServerOption.incomingBandwidth,
				sessionServerOption.outgoingBandwidth,
				sessionServerOption.bufferSize
			);

			for (auto& handler : onConnectionHandlers)
				newServer->registerConnectionHandler(handler.second);

			for (auto& handler : onDisconnectionHandlers)
				newServer->registerDisconnectionHandler(handler.second);

			for (auto& handler : onPacketReceivedHandlers)
				newServer->registerPacketReceivedHandler(handler.second);

			info.identifier.sessionPort = newServer->getServerPort();
			newServer->attachSession(std::move(session));

			sessionServers.push_back(std::move(newServer));
			sessionTypes.push_back(opt.sessionType);

			result.success = true;
			result.errorCode = 0;
			result.sessionInfo = info;
		} else {
			result.success = false;
			result.errorCode = 1;
		}
		return result;
	}

	SessionListResult SessionManager::getSessionList(const SessionListOption& option) {
		SessionListResult result;
		std::vector<SessionInfo> list;
		for (size_t i = 0; i < sessionServers.size(); i++) {
			auto items = sessionServers[i]->getSessionList(option.sessionType, option.nameFilter);
			list.insert(list.end(), items.begin(), items.end());
		}
		result.totalSessionCount = static_cast<uint32_t>(list.size());
		const uint32_t startIdx = (option.page - 1) * option.sessionPerPage;
		if (startIdx < list.size()) {
			result.sessionInfoList = std::vector(
				list.begin() + startIdx,
				list.begin() + std::min(startIdx + option.sessionPerPage, static_cast<uint32_t>(list.size()))
			);
		}
		
		return result;
	}
	void AbstractSession::handlePacket(uint16_t typeId, ENetPeer* peer, std::vector<uint8_t>& rawData) {
		for (auto& handler : packetHandlers[typeId]) {
			handler->rawHandle(*this, peer, rawData);
		}
	}
}
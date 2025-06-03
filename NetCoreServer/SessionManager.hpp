#pragma once
#include "pch.h"
#include "SessionServer.hpp"
#include "AbstractSession.hpp"

namespace NetCoreServer {
	using SessionPtr = std::shared_ptr<AbstractSession>;
	using SessionGenerator = std::function<SessionPtr(const SessionInfo&, const SessionCreationOption&)>;
	using UsernameProvider = std::function<std::string(uint64_t)>;

	template<typename SessionType>
	concept IsSession = std::is_base_of_v<AbstractSession, SessionType>;
	class SessionManager {
	private:
		std::vector<std::shared_ptr<SessionServer>> sessionServers;
		std::vector<std::string> sessionTypes;
		SessionServerOption sessionServerOption;
		UsernameProvider usernameProvider;

		std::unordered_map<std::string, SessionGenerator> sessionGenerators;

		static HandlerId eventHandlerNextId;

		std::unordered_map<HandlerId, std::function<void(ENetPeer*)>> onConnectionHandlers;
		std::unordered_map<HandlerId, std::function<void(ENetPeer*)>> onDisconnectionHandlers;
		std::unordered_map<HandlerId, std::function<void(ENetPeer*, ENetPacket*)>> onPacketReceivedHandlers;

		template<typename T>
		HandlerId registerHandler(std::unordered_map<HandlerId, std::function<T>>& handlers, std::function<T> handler) {
			HandlerId id = eventHandlerNextId++;
			handlers.emplace(id, std::move(handler));
			return id;
		}

		template<typename T>
		bool removeHandler(std::unordered_map<HandlerId, std::function<T>>& handlers, HandlerId id) {
			return handlers.erase(id) > 0;
		}

	public:
		SessionManager(SessionServerOption opt, UsernameProvider provider) : sessionServerOption(std::move(opt)), usernameProvider(std::move(provider)) {
		}

		HandlerId registerConnectionHandler(const std::function<void(ENetPeer*)>& handler) {
			return registerHandler(onConnectionHandlers, handler);
		}

		HandlerId registerDisconnectionHandler(const std::function<void(ENetPeer*)>& handler) {
			return registerHandler(onDisconnectionHandlers, handler);
		}

		HandlerId registerPacketReceivedHandler(const std::function<void(ENetPeer*, ENetPacket*)>& handler) {
			return registerHandler(onPacketReceivedHandlers, handler);
		}

		bool removeConnectionHandler(HandlerId id) {
			return removeHandler(onConnectionHandlers, id);
		}

		bool removeDisconnectionHandler(HandlerId id) {
			return removeHandler(onDisconnectionHandlers, id);
		}

		bool removePacketReceivedHandler(HandlerId id) {
			return removeHandler(onPacketReceivedHandlers, id);
		}

		void registerSessionGenerator(std::string sessionType, SessionGenerator generator) {
			sessionGenerators[sessionType] = std::move(generator);
		}

		void removeSessionGenerator(std::string sessionType) {
			if (sessionGenerators.contains(sessionType)) sessionGenerators[sessionType] = nullptr;
		}

		SessionCreationResult createNewSession(const SessionCreationOption& opt) {
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

				for (uint16_t i = 0; i < sessionServers.size(); i++) {
					if (sessionTypes[i] == opt.sessionType && sessionServers[i]->getSessionsCount() < sessionServerOption.maxSessions) {
						session->server = sessionServers[i];
						info.identifier.sessionNumber = sessionServers[i]->attachSession(session);
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

				auto newServer = std::make_shared<SessionServer>(
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
				newServer->attachSession(session);

				sessionServers.push_back(newServer);
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

		SessionListResult getSessionList(const SessionListOption& option) {
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
	};
}
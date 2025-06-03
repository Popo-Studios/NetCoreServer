#pragma once
#include "pch.h"
#include "Server.hpp"
#include "NetCoreStructure.hpp"
#include "AbstractSession.hpp"

namespace NetCoreServer {
	class SessionJoinHandler : public AbstractPacketHandler<Server> {
	public:
		void rawHandle(Server& server, ENetPeer* peer, const std::vector<uint8_t>& rawData) override;
	};

	class SessionServer : public Server {
	private:
		uint8_t sessionJoinChannel = 0;
		ENetPacketFlag sessionJoinPacketFlag = ENET_PACKET_FLAG_RELIABLE;

		std::unordered_map<uint64_t, uint16_t> uidToSessionNumberTable;
		std::unordered_map<uint16_t, std::vector<uint64_t>> sessionNumberToUidTable;

		std::vector<std::shared_ptr<AbstractSession>> sessions;
		std::vector<std::unique_ptr<std::thread>> sessionThreads;

		bool detachSession(uint16_t sessionNumber) {
			if (sessions.size() > sessionNumber) {
				Logger::success(makeLog(std::format("A session is deleted (Num: {})", sessionNumber)));
				sessions[sessionNumber]->stop();
				sessionThreads[sessionNumber]->detach();
				sessionThreads[sessionNumber].reset();
				sessions[sessionNumber].reset();
				return true;
			} else {
				Logger::error(makeLog(std::format("Failed to delete a session (Num: {})", sessionNumber)));
				return false;
			}
		}

	public:
		SessionServer(uint16_t port, size_t max_connection, size_t max_channel, size_t queueSize = 1024, uint32_t incomingBandwidth = 0, uint32_t outgoingBandwidth = 0, int32_t bufferSize = BufferSize::DEFAULT)
			: Server(port, max_connection, max_channel, queueSize, incomingBandwidth, outgoingBandwidth, bufferSize) {
			auto joinHandler = std::make_shared<SessionJoinHandler>();
			registerPacketHandler("JoinSession", joinHandler);

			registerDisconnectionHandler([this](ENetPeer* peer) {
				auto uid = getPeerUid(peer);
				if (uid.has_value()) {
					removePeer(*uid);
					removeUser(*uid);
				}
				});

			registerPacketReceivedHandler([this](ENetPeer* peer, ENetPacket* enetPacket) {
				auto ppacket = PacketUtils::parsePacket(Packet{ enetPacket });
				if (ppacket.has_value()) {
					switch (ppacket->header.packetTypeId) {
					case static_cast<uint16_t>(PredefinedPacketType::CreateSession):
					case static_cast<uint16_t>(PredefinedPacketType::GetServerType):
					case static_cast<uint16_t>(PredefinedPacketType::GetSessionList):
					case static_cast<uint16_t>(PredefinedPacketType::Login):
						return;
					}

					auto uid = getPeerUid(peer);
					if (uid.has_value()) {
						auto snum = getSessionNumberByUid(*uid);

						if (snum.has_value() && sessions.size() > *snum)
							sessions[*snum]->handlePacket(ppacket->header.packetTypeId, peer, ppacket->rawData);
					}
				}
				});
		}

		~SessionServer() {}

		std::vector<SessionInfo> getSessionList(std::string sessionType, std::optional<std::string> nameFilter = std::nullopt) {
			std::vector<SessionInfo> list;
			for (size_t i = 0; i < sessions.size(); i++) {
				if (sessions[i] == nullptr) continue;

				SessionInfo info = sessions[i]->getSessionInfo();
				if (info.isPrivate) continue;
				if (info.sessionType != sessionType) continue;
				if (nameFilter.has_value() && info.name.find(toLower(*nameFilter)) == std::string::npos) continue;
				list.push_back(info);
			}
			return list;
		}

		void setSessionJoinChannel(uint8_t channel) {
			sessionJoinChannel = channel;
		}

		uint8_t getSessionJoinChannel() const {
			return sessionJoinChannel;
		}

		void setSessionJoinPacketFlag(ENetPacketFlag flag) {
			sessionJoinPacketFlag = flag;
		}

		ENetPacketFlag getSessionJoinPacketFlag() const {
			return sessionJoinPacketFlag;
		}

		void addUser(uint16_t sessionNumber, uint64_t uid) {
			uidToSessionNumberTable.emplace(uid, sessionNumber);
			sessionNumberToUidTable[sessionNumber].push_back(uid);

			sessions[sessionNumber]->sessionInfo.currentPlayers += 1;
			sessions[sessionNumber]->players.push_back(uid);
		}

		std::optional<uint16_t> getSessionNumberByUid(uint64_t uid) const {
			if (uidToSessionNumberTable.contains(uid)) {
				return uidToSessionNumberTable.at(uid);
			} return std::nullopt;
		}

		bool removeUser(uint64_t uid) {
			if (uidToSessionNumberTable.contains(uid)) {
				auto num = uidToSessionNumberTable[uid];
				uidToSessionNumberTable.erase(uid);
				auto& vec = sessionNumberToUidTable[num];
				vec.erase(std::remove(vec.begin(), vec.end(), uid), vec.end());

				if (vec.empty()) {
					return detachSession(num);
				} else {
					sessions[num]->sessionInfo.currentPlayers -= 1;
					auto& players = sessions[num]->players;
					players.erase(std::remove(players.begin(), players.end(), uid), players.end());
				}

				return true;
			} else return false;
		}

		constexpr std::string getServerType() const override final {
			return "SESSION_SERVER";
		}

		const size_t getSessionsCount() {
			return std::count_if(sessions.begin(), sessions.end(), [](const std::shared_ptr<AbstractSession>& ptr) {
				return ptr != nullptr;
				});
		}

		uint16_t attachSession(std::shared_ptr<AbstractSession> session) {
			uint16_t num = 0;
			auto& info = session->getSessionInfo();
			auto thread = std::make_unique<std::thread>([session]() {
				const auto tickInterval = std::chrono::duration<double>(1.0 / session->getFramerate());
				auto previous = std::chrono::steady_clock::now();
				auto nextTick = previous + tickInterval;

				while (session->isRunning()) {
					auto now = std::chrono::steady_clock::now();
					std::chrono::duration<double> deltaTime = now - previous;
					previous = now;

					session->tick(deltaTime.count());

					nextTick += tickInterval;
					if (std::chrono::steady_clock::now() < nextTick) {
						std::this_thread::sleep_until(nextTick);
					} else {
						nextTick = std::chrono::steady_clock::now();
					}
				}
			});

			bool created = false;
			for (; num < static_cast<uint16_t>(sessions.size()); num++) {
				if (sessions[num] == nullptr) {
					sessions[num] = session;
					sessionThreads[num] = std::move(thread);
					created = true;
					break;
				}
			}

			if (!created) {
				sessions.push_back(std::move(session));
				sessionThreads.push_back(std::move(thread));
			}

			Logger::success(makeLog(std::format("A new session is created (Num: {}, Type: {}, Name: {}, MaxPlayers: {}, IsPrivate: {})", num, info.sessionType, info.name, info.maxPlayers, info.isPrivate)));

			return num;
		}
	};
}
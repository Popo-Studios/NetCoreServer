#include "pch.h"
#include "Server.hpp"

namespace NetCoreServer {
	bool initialize() {
		if (enet_initialize() != 0) {
			Logger::error("Failed to initialize ENet.");
			return false;
		}
		PacketUtils::registerPredefinedPacketType();
		atexit(enet_deinitialize);
		return true;
	}

	HandlerId Server::eventHandlerNextId = 1;

	Server::Server(uint16_t port, size_t max_connection, size_t max_channel, size_t queueSize,
		uint32_t incomingBandwidth, uint32_t outgoingBandwidth, int32_t bufferSize)
			: address({ ENET_HOST_ANY, port }), packetQueue(queueSize) {
		server = enet_host_create(&address, max_connection, max_channel, incomingBandwidth, outgoingBandwidth, bufferSize);

		if (!server) throw ServerCreationError();

		running = true;
		serverThread = std::thread(&Server::run, this);

		auto handler = std::make_shared<ServerTypePacketHandler>();
		registerPacketHandler("GetServerType", handler);
	}

	Server::~Server() {
		if (server) {
			enet_host_destroy(server);
		}
	}

	std::string Server::getServerHostName() const {
		char hostname[256] = { 0, };
		enet_address_get_hostname(&address, hostname, sizeof(hostname));
		return std::string(hostname);
	}

	std::string Server::getPeerIP(ENetPeer* peer) {
		char ip[16] = { 0, };
		enet_address_get_ip(&peer->address, ip, sizeof(ip));
		return std::string(ip);
	}

	ENetPeer* Server::getPeerByUid(uint64_t uid) const {
		auto it = uidToPeerTable.find(uid);
		if (it != uidToPeerTable.end()) {
			return it->second;
		}
		return nullptr;
	}

	bool Server::removePeerUid(ENetPeer* peer) {
		auto it = peerToUidTable.find(peer);
		if (it != peerToUidTable.end()) {
			uint64_t uid = it->second;
			peerToUidTable.erase(it);
			uidToPeerTable.erase(uid);
			return true;
		}
		return false;
	}

	std::optional<uint64_t> Server::getPeerUid(ENetPeer* peer) const {
		auto it = peerToUidTable.find(peer);
		if (it != peerToUidTable.end()) {
			return it->second;
		}
		return std::nullopt;
	}

	void Server::run() {
		Logger::info(makeLog(std::format("Server started at port {}", getServerPort())));
		while (running.load()) {
			ENetEvent event;
			while (enet_host_service(server, &event, timeout.load()) > 0) {
				switch (event.type) {
				case ENET_EVENT_TYPE_CONNECT:
					for (auto& handler : onConnectionHandlers) {
						handler.second(event.peer);
					}
					Logger::info(makeLog(std::format("A new client connected from {}", getPeerIP(event.peer))));
					break;
				case ENET_EVENT_TYPE_RECEIVE: {
					for (auto& handler : onPacketReceivedHandlers) {
						handler.second(event.peer, event.packet);
					}

					auto parsedPacket = PacketUtils::parsePacket(Packet{ event.packet });
					if (parsedPacket.has_value()) {
						auto& packet = parsedPacket.value();
						for (auto& handler : packetHandlers[packet.header.packetTypeId]) {
							handler->rawHandle(*this, event.peer, packet.rawData);
						}
					}

					//Logger::info("Received a packet from a client. " + std::to_string(parsedPacket->header.packetTypeId));

					enet_packet_destroy(event.packet);
					break;
				}
				case ENET_EVENT_TYPE_DISCONNECT:
					for (auto& handler : onDisconnectionHandlers) {
						handler.second(event.peer);
					}
					Logger::info(makeLog(std::format("A client disconnected from {}", getPeerIP(event.peer))));
					break;
				default:
					break;
				}
			}

			while (!packetQueue.empty()) {
				QueuedPacket* qpacket;
				if (packetQueue.pop(qpacket)) {


					qpacket->packet.destory();
					delete qpacket;
				} else break;
			}
		}
	}

	void Server::stop() {
		if (running.exchange(false)) {
			if (serverThread.joinable()) serverThread.join();
		}
	}

	void Server::wait() {
		if (serverThread.joinable()) {
			serverThread.join();
		}
	}

	void Server::sendPacket(ENetPeer* peer, uint8_t channel, Packet packet) {
		if (peer && server) {
			enet_peer_send(peer, channel, packet.enetPacket);
		} else {
			Logger::error(makeLog(format("Failed to send packet: Invalid peer or server. (Peer: {})", getPeerIP(peer))));
		}
	}

    inline void SessionCreationHandler::handle(Server& server, ENetPeer* peer, const SessionCreationOption& data) {
       MainServer& mainServer = dynamic_cast<MainServer&>(server);
       auto result = mainServer.createNewSession(data);

       auto packet = PacketUtils::createPacket<SessionCreationResult>("CreateSession", result, server.getSessionPacketFlag());
       server.sendPacket(peer, server.getSessionChannel(), packet);
    }

	MainServer::MainServer(const LoginFunc& loginFunc, const UsernameProvider& provider, const SessionServerOption& opt, uint16_t port, size_t max_connection, size_t max_channel, size_t queueSize, uint32_t incomingBandwidth, uint32_t outgoingBandwidth, int32_t bufferSize) 
		: Server(port, max_connection, max_channel, queueSize, incomingBandwidth, outgoingBandwidth, bufferSize), sessionManager(opt, provider, [this](ENetPeer* peer) { return getPeerUid(peer); }) {
		auto loginHandler = std::make_shared<LoginHandler>(loginFunc);
		registerPacketHandler("Login", loginHandler);
		auto listHandler = std::make_shared<SessionListHandler>();
		registerPacketHandler("GetSessionList", listHandler);
		auto creationHandler = std::make_shared<SessionCreationHandler>();
		registerPacketHandler("CreateSession", creationHandler);
	}

	bool SessionServer::detachSession(uint16_t sessionNumber) {
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

	SessionServer::SessionServer(uint16_t port, size_t max_connection, size_t max_channel, size_t queueSize, uint32_t incomingBandwidth, uint32_t outgoingBandwidth, int32_t bufferSize)
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

	std::vector<SessionInfo> SessionServer::getSessionList(std::string sessionType, std::optional<std::string> nameFilter) {
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

	uint16_t SessionServer::attachSession(std::shared_ptr<AbstractSession> session) {
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

	void SessionJoinHandler::rawHandle(Server& server, ENetPeer* peer, const std::vector<uint8_t>& rawData) {
		SessionJoinOption option = PacketUtils::parseRawData<SessionJoinOption>(rawData);
		SessionServer& sessionServer = dynamic_cast<SessionServer&>(server);
		SessionJoinResult result;
		bool isValid = true; // todo
		if (isValid) {
			server.setPeerUid(peer, option.userIdentifier.userId);
			sessionServer.setUserSessionNumber(option.sessionNumber, option.userIdentifier.userId);
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

	void SessionListHandler::handle(Server& server, ENetPeer* peer, const SessionListOption& data) {
		MainServer& mainServer = dynamic_cast<MainServer&>(server);
		auto packet = PacketUtils::createPacket("GetSessionList", mainServer.getSessionList(data), server.getSessionPacketFlag());
		server.sendPacket(peer, server.getSessionChannel(), packet);
	}
}

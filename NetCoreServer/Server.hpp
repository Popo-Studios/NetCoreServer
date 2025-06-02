#pragma once
#include "pch.h"
#include "Logger.hpp"
#include "Error.hpp"
#include "Packet.hpp"
#include "Session.hpp"

namespace NetCoreServer {
	bool initialize();

	typedef struct _QueuedPacket {
		uint8_t channel;
		Packet packet;
	} QueuedPacket;

	template<typename ContextType>
	class AbstractPacketHandler;
	class MainServer;
	class SessionServer;

	using HandlerId = uint64_t;
	class Server {
	private:
		ENetAddress address;
		ENetHost* server;

		std::thread serverThread;

		std::atomic<uint32_t> timeout;
		std::atomic<bool> running;

		boost::lockfree::queue<QueuedPacket*> packetQueue;
		std::unordered_map<uint64_t, ENetPeer*> connectedPeers;

		std::unordered_map<HandlerId, std::vector<std::shared_ptr<AbstractPacketHandler<Server>>>> packetHandlers;

		static HandlerId eventHandlerNextId;

		std::unordered_map<HandlerId, std::function<void(ENetPeer*)>> onConnectionHandlers;
		std::unordered_map<HandlerId, std::function<void(ENetPeer*)>> onDisconnectionHandlers;
		std::unordered_map<HandlerId, std::function<void(ENetPeer*, ENetPacket*)>> onPacketReceivedHandlers;

		std::unordered_map<ENetPeer*, uint64_t> peerToUidTable;
		std::unordered_map<uint64_t, ENetPeer*> uidToPeerTable;

		uint8_t sessionChannel;
		ENetPacketFlag sessionPacketFlag = ENET_PACKET_FLAG_RELIABLE;

		void run();

	protected:
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
		Server(uint16_t port, size_t max_connection, size_t max_channel, size_t queueSize = 1024, uint32_t incomingBandwidth = 0, uint32_t outgoingBandwidth = 0, int32_t bufferSize = BufferSize::DEFAULT);
		virtual ~Server();

		std::string makeLog(std::string content) {
			return std::format("[{}:{}] ", getServerType(), getServerPort()) + content;
		}

		virtual constexpr std::string getServerType() const {
			return "SERVER";
		}

		std::string getServerHostName() const;

		uint16_t getServerPort() const {
			return address.port;
		}

		static std::string getPeerIP(ENetPeer* peer);

		void setTimeout(uint32_t timeout = 50) {
			this->timeout = timeout;
		}

		void setPeerUid(ENetPeer* peer, uint64_t uid) {
			peerToUidTable[peer] = uid;
			uidToPeerTable[uid] = peer;
		}

		void removePeer(uint64_t uid) {
			if (uidToPeerTable.contains(uid)) {
				auto peer = uidToPeerTable[uid];
				peerToUidTable.erase(peer);
				uidToPeerTable.erase(uid);
			}
		}

		std::string getServerIP() const {
			char ip[16] = { 0, };
			enet_address_get_ip(&address, ip, sizeof(ip));
			return std::string(ip);
		}

		void setSessionChannel(uint8_t channel) {
			sessionChannel = channel;
		}

		uint8_t getSessionChannel() const {
			return sessionChannel;
		}

		void setSessionPacketFlag(ENetPacketFlag flag) {
			sessionPacketFlag = flag;
		}

		ENetPacketFlag getSessionPacketFlag() const {
			return sessionPacketFlag;
		}

		ENetPeer* getPeerByUid(uint64_t uid) const;

		bool removePeerUid(ENetPeer* peer);

		std::optional<uint64_t> getPeerUid(ENetPeer* peer) const;

		inline bool registerPacketHandler(uint16_t packetTypeId, std::shared_ptr<AbstractPacketHandler<Server>> handler) {
			auto& l = packetHandlers[packetTypeId];
			if (std::find(l.begin(), l.end(), handler) == l.end()) {
				l.push_back(handler);
				return true;
			} else return false;
		}

		inline bool registerPacketHandler(std::string packetTypeName, std::shared_ptr<AbstractPacketHandler<Server>> handler) {
			auto id = PacketUtils::getPacketTypeId(packetTypeName);
			if (id.has_value()) {
				return registerPacketHandler(id.value(), handler);
			} else return false;
		}

		inline bool removePacketHandler(uint16_t packetTypeId, std::shared_ptr<AbstractPacketHandler<Server>> handler) {
			auto& l = packetHandlers[packetTypeId];
			auto it = std::find(l.begin(), l.end(), handler);
			if (it != l.end()) {
				l.erase(it);
				return true;
			} else return false;
		}

		inline bool removePacketHandler(std::string packetTypeName, std::shared_ptr<AbstractPacketHandler<Server>> handler) {
			auto id = PacketUtils::getPacketTypeId(packetTypeName);
			if (id.has_value()) {
				return removePacketHandler(id.value(), handler);
			} else return false;
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

		void stop();
		void wait();

		void sendPacket(uint64_t uid, uint8_t channel, Packet packet) {
			sendPacket(getPeerByUid(uid), channel, packet);
		}

		void sendPacket(ENetPeer* peer, uint8_t channel, Packet packet);
	};

	struct LoginData final {
		std::string id;
		std::string password;

		MSGPACK_DEFINE_ARRAY(id, password);
	};

	struct LoginResult final {
		bool success;

		std::optional<UserIdentifier> userIdentifier;
		std::optional<uint8_t> errorCode;

		MSGPACK_DEFINE_ARRAY(success, userIdentifier, errorCode);
	};

	template<typename DataType>
	class ServerPacketHandler : public AbstractPacketHandler<Server> {
	public:
		void rawHandle(Server& server, ENetPeer* peer, const std::vector<uint8_t>& rawData) override {
			DataType data = PacketUtils::parseRawData<DataType>(rawData);
			handle(server, peer, data);
		}

	protected:
		virtual void handle(Server& server, ENetPeer* peer, const DataType& data) = 0;
	};

	template<>
	class ServerPacketHandler<void> : public AbstractPacketHandler<Server> {
	public:
		void rawHandle(Server& server, ENetPeer* peer, const std::vector<uint8_t>& rawData) override {
			handle(server, peer);
		}

	protected:
		virtual void handle(Server& server, ENetPeer* peer) = 0;
	};

	class ServerTypePacketHandler : public ServerPacketHandler<void> {
	protected:
		void handle(Server& server, ENetPeer* peer) override {
			auto packet = PacketUtils::createPacket("GetServerType", server.getServerType(), ENetPacketFlag::ENET_PACKET_FLAG_RELIABLE);
			server.sendPacket(peer, 0, packet);
		}
	};

	using LoginFunc = std::function<LoginResult(LoginData)>;
	class LoginHandler : public AbstractPacketHandler<Server> {
	private:
		LoginFunc loginFunc;
	public:
		LoginHandler(LoginFunc loginFunc)
			: loginFunc(std::move(loginFunc)) {
		}
	
		void rawHandle(Server& server, ENetPeer* peer, const std::vector<uint8_t>& rawData) override;
	};

	struct SessionServerOption;
	struct SessionCreationOption;

	class SessionCreationHandler : public ServerPacketHandler<SessionCreationOption> {
	protected:
		void handle(Server& server, ENetPeer* peer, const SessionCreationOption& data) override;
	};

	class SessionListHandler : public ServerPacketHandler<SessionListOption> {
	protected:
		void handle(Server& server, ENetPeer* peer, const SessionListOption& data) override;
	};

	class SessionManager;

	class MainServer: public Server {
	private:
		friend class LoginHandler;

		uint8_t loginChannel = 0;
		ENetPacketFlag loginPacketFlag = ENET_PACKET_FLAG_RELIABLE;

		SessionManager sessionManager;

	public:
		MainServer(const LoginFunc& loginFunc, const UsernameProvider& provider, const SessionServerOption& opt, uint16_t port, size_t max_connection, size_t max_channel, size_t queueSize = 1024, uint32_t incomingBandwidth = 0, uint32_t outgoingBandwidth = 0, int32_t bufferSize = BufferSize::DEFAULT);

		~MainServer() {}

		HandlerId registerConnectionHandlerOnSessionServer(const std::function<void(ENetPeer*)>& handler) {
			return sessionManager.registerConnectionHandler(handler);
		}

		HandlerId registerDisconnectionHandlerOnSessionServer(const std::function<void(ENetPeer*)>& handler) {
			return sessionManager.registerDisconnectionHandler(handler);
		}

		HandlerId registerPacketReceivedHandlerOnSessionServer(const std::function<void(ENetPeer*, ENetPacket*)>& handler) {
			return sessionManager.registerPacketReceivedHandler(handler);
		}

		bool removeConnectionHandlerOnSessionServer(HandlerId id) {
			return sessionManager.removeConnectionHandler(id);
		}

		bool removeDisconnectionHandlerOnSessionServer(HandlerId id) {
			return sessionManager.removeDisconnectionHandler(id);
		}

		bool removePacketReceivedHandlerOnSessionServer(HandlerId id) {
			return sessionManager.removePacketReceivedHandler(id);
		}

		SessionListResult getSessionList(const SessionListOption& option) {
			return sessionManager.getSessionList(option);
		}

		constexpr std::string getServerType() const override final {
			return "MAIN_SERVER";
		}

		void registerSessionGenerator(std::string sessionType, SessionGenerator generator) {
			sessionManager.registerSessionGenerator(sessionType, generator);
		}

		void removeSessionGenerator(std::string sessionType) {
			sessionManager.removeSessionGenerator(sessionType);
		}

		void setLoginChannel(uint8_t channel) {
			loginChannel = channel;
		}

		uint8_t getLoginChannel() const {
			return loginChannel;
		}

		void setLoginPacketFlag(ENetPacketFlag flag) {
			loginPacketFlag = flag;
		}

		ENetPacketFlag getLoginPacketFlag() const {
			return loginPacketFlag;
		}

		SessionCreationResult createNewSession(const SessionCreationOption& option) {
			return sessionManager.createNewSession(option);
		}
	};

	class AbstractSession;

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

		bool detachSession(uint16_t sessionNumber);

	public:
		SessionServer(uint16_t port, size_t max_connection, size_t max_channel, size_t queueSize = 1024, uint32_t incomingBandwidth = 0, uint32_t outgoingBandwidth = 0, int32_t bufferSize = BufferSize::DEFAULT);

		~SessionServer() {}

		std::vector<SessionInfo> getSessionList(std::string sessionType, std::optional<std::string> nameFilter = std::nullopt);

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

		void setUserSessionNumber(uint16_t sessionNumber, uint64_t uid) {
			uidToSessionNumberTable.emplace(uid, sessionNumber);
			sessionNumberToUidTable[sessionNumber].push_back(uid);
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

		uint16_t attachSession(std::shared_ptr<AbstractSession> session);
	};
}
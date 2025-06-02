#pragma once
#include "pch.h"
#include "Packet.hpp"

namespace NetCoreServer {
	struct SessionIdentifier final {
		uint16_t sessionPort;
		uint16_t sessionNumber;

		MSGPACK_DEFINE_ARRAY(sessionPort, sessionNumber);
	};

	struct UserIdentifier final {
		uint64_t userId;
		std::string userToken;

		MSGPACK_DEFINE_ARRAY(userId, userToken);
	};

	struct SessionCreationOption final {
		std::string name;
		std::optional<std::string> password;
		uint8_t maxPlayers;
		bool isPrivate;
		UserIdentifier userIdentifier;
		std::string sessionType;

		MSGPACK_DEFINE_ARRAY(name, password, maxPlayers, isPrivate, userIdentifier, sessionType);
	};

	struct SessionInfo final {
		std::string name;
		SessionIdentifier identifier;
		uint8_t maxPlayers;
		uint8_t currentPlayers;
		bool isPrivate;
		bool hasPassword;
		std::string authorName;
		std::string sessionType;

		MSGPACK_DEFINE_ARRAY(name, identifier, maxPlayers, currentPlayers, isPrivate, hasPassword, authorName, sessionType);
	};

	struct SessionListResult final {
		uint32_t totalSessionCount;
		std::vector<SessionInfo> sessionInfoList;

		MSGPACK_DEFINE_ARRAY(totalSessionCount, sessionInfoList);
	};

	struct SessionListOption final {
		std::optional<std::string> nameFilter;
		uint32_t page;
		uint32_t sessionPerPage;
		std::string sessionType;
		MSGPACK_DEFINE_ARRAY(nameFilter, page, sessionPerPage, sessionType);
	};

	struct SessionJoinOption final {
		UserIdentifier userIdentifier;
		uint16_t sessionNumber;
		std::optional<std::string> password;

		MSGPACK_DEFINE_ARRAY(userIdentifier, sessionNumber, password);
	};

	struct SessionJoinResult final {
		bool success;
		uint8_t errorCode;

		MSGPACK_DEFINE_ARRAY(success, errorCode);
	};

	struct SessionCreationResult final {
		bool success;
		uint8_t errorCode;
		std::optional<SessionInfo> sessionInfo;

		MSGPACK_DEFINE_ARRAY(success, errorCode, sessionInfo);
	};

	template<typename ContextType>
	class AbstractPacketHandler;

	class SessionManager;

	using PeerUidFunc = std::function<std::optional<uint64_t>(ENetPeer*)>;
	class AbstractSession {
	private:
		friend class SessionManager;

		SessionInfo sessionInfo;
		std::optional<std::string> password;
		PeerUidFunc peerUidFunc = nullptr;
		const double framerate;

		std::unordered_map<uint16_t, std::vector<std::shared_ptr<AbstractPacketHandler<AbstractSession>>>> packetHandlers;

		std::atomic<bool> running;

	public:
		AbstractSession(SessionInfo info, const SessionCreationOption& opt, const double framerate)
			: sessionInfo(std::move(info)), password(opt.password), framerate(framerate) {
		}
		virtual ~AbstractSession() {}

		virtual void tick(double deltaTime) = 0;

		const double getFramerate() const {
			return framerate;
		}

		bool isRunning() const {
			return running.load();
		}

		void stop() {
			running.store(false);
		}

		inline bool registerPacketHandler(uint16_t packetTypeId, std::shared_ptr<AbstractPacketHandler<AbstractSession>> handler) {
			auto& l = packetHandlers[packetTypeId];
			if (std::find(l.begin(), l.end(), handler) == l.end()) {
				l.push_back(handler);
				return true;
			} else return false;
		}

		inline bool registerPacketHandler(std::string packetTypeName, std::shared_ptr<AbstractPacketHandler<AbstractSession>> handler) {
			auto id = PacketUtils::getPacketTypeId(packetTypeName);
			if (id.has_value()) {
				return registerPacketHandler(id.value(), handler);
			} else return false;
		}

		inline bool removePacketHandler(uint16_t packetTypeId, std::shared_ptr<AbstractPacketHandler<AbstractSession>> handler) {
			auto& l = packetHandlers[packetTypeId];
			auto it = std::find(l.begin(), l.end(), handler);
			if (it != l.end()) {
				l.erase(it);
				return true;
			} else return false;
		}

		inline bool removePacketHandler(std::string packetTypeName, std::shared_ptr<AbstractPacketHandler<AbstractSession>> handler) {
			auto id = PacketUtils::getPacketTypeId(packetTypeName);
			if (id.has_value()) {
				return removePacketHandler(id.value(), handler);
			} else return false;
		}

		void handlePacket(uint16_t typeId, ENetPeer* peer, std::vector<uint8_t>& rawData);

		const SessionInfo& getSessionInfo() const {
			return sessionInfo;
		}

		void setSessionInfo(SessionInfo info) {
			sessionInfo = std::move(info);
		}

		bool comparePassword(const std::string& inputPassword) const {
			if (password.has_value()) {
				return *password == inputPassword;
			} else return true;
		}

		const std::string& getSessionType() const {
			return sessionInfo.sessionType;
		}

		const std::optional<uint64_t> getPeerUid(ENetPeer* peer) {
			return peerUidFunc == nullptr ? std::nullopt : peerUidFunc(peer);
		}
	};

	template<typename DataType>
	class SessionPacketHandler : public AbstractPacketHandler<AbstractSession> {
	public:
		void rawHandle(AbstractSession& session, ENetPeer* peer, std::vector<uint8_t>& rawData) override {
			DataType data = PacketUtils::parseRawData<DataType>(rawData);
			auto uid = session.getPeerUid(peer);
			if (uid.has_value()) {
				handle(session, *uid, data);
			}
		}

	protected:
		virtual void handle(AbstractSession& session, uint64_t uid, DataType data) = 0;
	};

	template<>
	class SessionPacketHandler<void> : public AbstractPacketHandler<AbstractSession> {
	public:
		void rawHandle(AbstractSession& session, ENetPeer* peer, const std::vector<uint8_t>& rawData) override {
			auto uid = session.getPeerUid(peer);
			if (uid.has_value()) {
				handle(session, *uid);
			}
		}

	protected:
		virtual void handle(AbstractSession& session, uint64_t uid) = 0;
	};

	enum BufferSize {
		DEFAULT = 0,
		SMALL = 262144, // 256 KB
		MEDIUM = 524288, // 512 KB
		LARGE = 1048576 // 1 MB
	};

	struct SessionServerOption {
		size_t maxConnection;
		size_t maxChannel;
		uint16_t maxSessions;
		std::pair<uint16_t, uint16_t> portRange;
		size_t queueSize = 1024;
		uint32_t incomingBandwidth = 0;
		uint32_t outgoingBandwidth = 0;
		int32_t bufferSize = BufferSize::DEFAULT;
	};

	class SessionServer;

	using SessionPtr = std::shared_ptr<AbstractSession>;
	using SessionGenerator = std::function<SessionPtr(const SessionInfo&, const SessionCreationOption&)>;
	using UsernameProvider = std::function<std::string(uint64_t)>;

	template<typename SessionType>
	concept IsSession = std::is_base_of_v<AbstractSession, SessionType>;

	using HandlerId = uint64_t;
	class SessionManager {
	private:
		std::vector<std::unique_ptr<SessionServer>> sessionServers;
		std::vector<std::string> sessionTypes;
		SessionServerOption sessionServerOption;
		UsernameProvider usernameProvider;
		PeerUidFunc peerUidFunc;
		
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
		SessionManager(SessionServerOption opt, UsernameProvider provider, PeerUidFunc peerUidFunc) : sessionServerOption(std::move(opt)), usernameProvider(std::move(provider)), peerUidFunc(std::move(peerUidFunc)) {
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

		SessionCreationResult createNewSession(const SessionCreationOption& opt);

		SessionListResult getSessionList(const SessionListOption& option);
	};
}
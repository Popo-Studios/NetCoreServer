#pragma once
#include "pch.h"
#include "Logger.hpp"
#include "Error.hpp"
#include "Packet.hpp"
#include "AbstractHandler.hpp"
#include "NetCoreStructure.hpp"

namespace NetCoreServer {
	bool initialize();

	typedef struct _QueuedPacket {
		uint8_t channel;
		Packet packet;
	} QueuedPacket;

	class Server;

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
		void handle(Server& server, ENetPeer* peer) override;
	};

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
		Server(uint16_t port, size_t max_connection, size_t max_channel, size_t queueSize = 1024, uint32_t incomingBandwidth = 0, uint32_t outgoingBandwidth = 0, int32_t bufferSize = BufferSize::DEFAULT)
			: address({ ENET_HOST_ANY, port }), packetQueue(queueSize) {
			server = enet_host_create(&address, max_connection, max_channel, incomingBandwidth, outgoingBandwidth, bufferSize);

			if (!server) throw ServerCreationError();

			running = true;
			serverThread = std::thread(&Server::run, this);

			auto handler = std::make_shared<ServerTypePacketHandler>();
			registerPacketHandler("GetServerType", handler);
		}

		~Server() {
			if (server) {
				enet_host_destroy(server);
			}
		}

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
}
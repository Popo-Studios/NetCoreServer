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

	void ServerTypePacketHandler::handle(Server& server, ENetPeer* peer) {
		auto packet = PacketUtils::createPacket("GetServerType", server.getServerType(), ENetPacketFlag::ENET_PACKET_FLAG_RELIABLE);
		server.sendPacket(peer, 0, packet);
	}
}

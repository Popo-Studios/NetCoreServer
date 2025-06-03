#pragma once
#include "pch.h"
#include "NetCoreStructure.hpp"
#include "AbstractHandler.hpp"
#include "Packet.hpp"

namespace NetCoreServer {
	class SessionManager;
	class SessionServer;

	class AbstractSession {
	private:
		friend class SessionManager;
		friend class SessionServer;

		SessionInfo sessionInfo;
		std::vector<uint64_t> players;
		std::optional<std::string> password;

		std::shared_ptr<SessionServer> server;

		const double framerate;

		std::unordered_map<uint16_t, std::vector<std::shared_ptr<AbstractPacketHandler<AbstractSession>>>> packetHandlers;

		std::atomic<bool> running;

	protected:
		void sendPacket(uint64_t uid, uint8_t channel, Packet packet);

		void sendPacket(ENetPeer* peer, uint8_t channel, Packet packet);

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

		void handlePacket(uint16_t typeId, ENetPeer* peer, std::vector<uint8_t>& rawData) {
			for (auto& handler : packetHandlers[typeId]) {
				handler->rawHandle(*this, peer, rawData);
			}
		}

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

		const std::optional<uint64_t> getPeerUid(ENetPeer* peer);
	};
}
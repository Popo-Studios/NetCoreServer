#pragma once
#include "pch.h"
#include "Server.hpp"
#include "SessionManager.hpp"
#include "Packet.hpp"

namespace NetCoreServer {
	class MainServer;

	class SessionListHandler : public ServerPacketHandler<SessionListOption> {
	protected:
		void handle(Server& server, ENetPeer* peer, const SessionListOption& data) override;
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

	class SessionCreationHandler : public ServerPacketHandler<SessionCreationOption> {
	protected:
		void handle(Server& server, ENetPeer* peer, const SessionCreationOption& data) override;
	};

	class MainServer : public Server {
	private:
		friend class LoginHandler;

		uint8_t loginChannel = 0;
		ENetPacketFlag loginPacketFlag = ENET_PACKET_FLAG_RELIABLE;

		SessionManager sessionManager;

	public:
		MainServer(const LoginFunc& loginFunc, const UsernameProvider& provider, const SessionServerOption& opt, uint16_t port, size_t max_connection, size_t max_channel, size_t queueSize = 1024, uint32_t incomingBandwidth = 0, uint32_t outgoingBandwidth = 0, int32_t bufferSize = BufferSize::DEFAULT)
			: Server(port, max_connection, max_channel, queueSize, incomingBandwidth, outgoingBandwidth, bufferSize), sessionManager(opt, provider) {
			auto loginHandler = std::make_shared<LoginHandler>(loginFunc);
			registerPacketHandler("Login", loginHandler);
			auto listHandler = std::make_shared<SessionListHandler>();
			registerPacketHandler("GetSessionList", listHandler);
			auto creationHandler = std::make_shared<SessionCreationHandler>();
			registerPacketHandler("CreateSession", creationHandler);
		}

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
}
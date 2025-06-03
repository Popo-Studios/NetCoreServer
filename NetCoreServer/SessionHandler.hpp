#pragma once
#include "pch.h"
#include "AbstractHandler.hpp"
#include "AbstractSession.hpp"

namespace NetCoreServer {
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
}
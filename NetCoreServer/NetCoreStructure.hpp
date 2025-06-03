#pragma once
#include "pch.h"

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
}
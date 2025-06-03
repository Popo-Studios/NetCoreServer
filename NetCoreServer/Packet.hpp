#pragma once
#include "pch.h"
#include "Logger.hpp"

namespace NetCoreServer {  
	struct PacketHeader final {  
		uint16_t packetTypeId;  
		int64_t timestamp;

		MSGPACK_DEFINE_ARRAY(packetTypeId, timestamp);
	};

	struct Packet final{
		ENetPacket* enetPacket;

		void destory() {
			if (enetPacket) {
				enet_packet_destroy(enetPacket);
				enetPacket = nullptr;
			}
		}
	};

	struct ParsedPacket final {
		PacketHeader header;
		std::vector<uint8_t> rawData;
	};

	enum class PredefinedPacketType : uint16_t {
		CreateSession = std::numeric_limits<uint16_t>::max(),
		JoinSession = std::numeric_limits<uint16_t>::max() - 1,
		Login = std::numeric_limits<uint16_t>::max() - 2,
		GetServerType = std::numeric_limits<uint16_t>::max() - 3,
		GetSessionList = std::numeric_limits<uint16_t>::max() - 4,
	};

	class PacketUtils {  
	private:  
		static std::unordered_map<std::string, uint16_t> typeNameToId;  
		static std::unordered_map<uint16_t, std::string> typeIdToName; 

		static std::random_device rd;
		static std::mt19937 mt;
		static uuids::uuid_random_generator uuidGenerator;
		
		static std::once_flag initFlag;  // Ensure that the initialization happens only once

	public:
		static void registerPredefinedPacketType() {
			std::call_once(initFlag, []() {
				registerPacketType(static_cast<uint16_t>(PredefinedPacketType::CreateSession), "CreateSession");
				registerPacketType(static_cast<uint16_t>(PredefinedPacketType::JoinSession), "JoinSession");
				registerPacketType(static_cast<uint16_t>(PredefinedPacketType::Login), "Login");
				registerPacketType(static_cast<uint16_t>(PredefinedPacketType::GetServerType), "GetServerType");
				registerPacketType(static_cast<uint16_t>(PredefinedPacketType::GetSessionList), "GetSessionList");
			});
		}

		static void registerPacketType(uint16_t typeId, std::string typeName) {  
			typeNameToId[typeName] = typeId;  
			typeIdToName[typeId] = typeName;
		}

		static std::optional<uint16_t> getPacketTypeId(std::string typeName) {
			if (typeNameToId.contains(typeName)) return typeNameToId[typeName];
			else return std::nullopt;
		}

		static std::optional<std::string> getPacketTypeName(uint16_t typeId) {
			if (typeIdToName.contains(typeId)) return typeIdToName[typeId];
			else return std::nullopt;
		}

		template<typename T>
		static Packet createPacket(uint16_t packetType, const T& data, ENetPacketFlag flag = ENetPacketFlag::ENET_PACKET_FLAG_NONE, int64_t timestamp = -1) {
			if (timestamp < 0) {
				timestamp = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now()).time_since_epoch().count();
			}

			PacketHeader header{ packetType, timestamp };

			msgpack::sbuffer headerBuf, dataBuf;
			msgpack::pack(headerBuf, header);
			msgpack::pack(dataBuf, data);

			std::vector<uint8_t> bytes;
			uint32_t headerLength = static_cast<uint32_t>(headerBuf.size());
			const uint8_t* hlptr = reinterpret_cast<uint8_t*>(&headerLength);

			// Insert header length as a 4-byte prefix
			bytes.insert(bytes.end(), hlptr, hlptr + sizeof(headerLength));

			// Insert header data
			bytes.insert(bytes.end(), headerBuf.data(), headerBuf.data() + headerBuf.size());

			// Insert packet data
			bytes.insert(bytes.end(), dataBuf.data(), dataBuf.data() + dataBuf.size());

			return Packet{ enet_packet_create(bytes.data(), bytes.size(), flag) };
		}

		template<typename T>
		static Packet createPacket(std::string packetTypeName, const T& data, ENetPacketFlag flag = ENetPacketFlag::ENET_PACKET_FLAG_NONE, int64_t timestamp = -1) {
			auto id = getPacketTypeId(packetTypeName);
			if (id.has_value()) {
				return createPacket(id.value(), data, flag, timestamp);
			} else {
				Logger::error(std::format("Failed to create packet: Invalid packet type name '{}'", packetTypeName));
				return Packet{ nullptr };
			}
		}

		static Packet createEmptyPacket(uint16_t packetType, ENetPacketFlag flag = ENetPacketFlag::ENET_PACKET_FLAG_NONE, int64_t timestamp = -1);

		static Packet createEmptyPacket(std::string packetTypeName, ENetPacketFlag flag = ENetPacketFlag::ENET_PACKET_FLAG_NONE, int64_t timestamp = -1) {
			auto id = getPacketTypeId(packetTypeName);
			if (id.has_value()) {
				return createEmptyPacket(*id, flag, timestamp);
			} else {
				Logger::error(std::format("Failed to create packet: Invalid packet type name '{}'", packetTypeName));
				return Packet{ nullptr };
			}
		}

		static std::optional<ParsedPacket> parsePacket(const Packet& packet);

		template<typename T>
		static T parseRawData(std::vector<uint8_t> const& rawData) {
			T result;
			msgpack::object_handle oh = msgpack::unpack(reinterpret_cast<const char*>(rawData.data()), rawData.size());
			try {
				oh.get().convert(result);
			} catch (const msgpack::type_error& e) {
				Logger::error(std::format("Failed to parse raw data (Type error): {}", e.what()));
				std::cout << oh.get() << std::endl;
			} catch (const msgpack::insufficient_bytes& e) {
				Logger::error(std::format("Failed to parse raw data (Insufficient bytes): {}", e.what()));
			} catch (const msgpack::unpack_error& e) {
				Logger::error(std::format("Failed to parse raw data (Unpack error): {}", e.what()));
			} catch (const std::exception& e) {
				Logger::error(std::format("Failed to parse raw data (Unknown error): {}", e.what()));
			}
			return result;
		}

		static std::string generateUUID() {
			return uuids::to_string(uuidGenerator());
		}
	};
}
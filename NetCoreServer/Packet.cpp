#include "pch.h"
#include "Packet.hpp"

namespace NetCoreServer {
	std::once_flag PacketUtils::initFlag;
	std::unordered_map<std::string, uint16_t> PacketUtils::typeNameToId;
	std::unordered_map<uint16_t, std::string> PacketUtils::typeIdToName;

	std::random_device PacketUtils::rd;
	std::mt19937 PacketUtils::mt(rd());
	uuids::uuid_random_generator PacketUtils::uuidGenerator(mt);

	Packet PacketUtils::createEmptyPacket(uint16_t packetType, ENetPacketFlag flag, int64_t timestamp) {
		if (timestamp < 0) {
			timestamp = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now()).time_since_epoch().count();
		}

		PacketHeader header{ packetType, timestamp };

		msgpack::sbuffer headerBuf;
		msgpack::pack(headerBuf, header);

		std::vector<uint8_t> bytes;
		uint32_t headerLength = static_cast<uint32_t>(headerBuf.size());
		const uint8_t* hlptr = reinterpret_cast<uint8_t*>(&headerLength);

		// Insert header length as a 4-byte prefix
		bytes.insert(bytes.end(), hlptr, hlptr + sizeof(headerLength));

		// Insert header data
		bytes.insert(bytes.end(), headerBuf.data(), headerBuf.data() + headerBuf.size());

		return Packet{ enet_packet_create(bytes.data(), bytes.size(), flag) };
	}

	std::optional<ParsedPacket> PacketUtils::parsePacket(const Packet& packet) {
		ParsedPacket parsedPacket;
		if (!packet.enetPacket) {
			return std::nullopt;
		}

		const uint8_t* data = packet.enetPacket->data;
		uint32_t headerLength = *reinterpret_cast<const uint32_t*>(data);
		data += sizeof(uint32_t);

		msgpack::object_handle oh = msgpack::unpack(reinterpret_cast<const char*>(data), headerLength);
		oh.get().convert(parsedPacket.header);
		data += headerLength;

		uint32_t dataLength = packet.enetPacket->dataLength - sizeof(uint32_t) - headerLength;
		if (dataLength > 0) {
			parsedPacket.rawData.assign(data, data + dataLength);
		}

		return parsedPacket;
	}
}
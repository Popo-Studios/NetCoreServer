#pragma once
#include "pch.h"

namespace NetCoreServer {
	enum class LogColor {
		RED = 31,
		GREEN = 32,
		YELLOW = 33,
		BLUE = 34,
		MAGENTA = 35,
		CYAN = 36,
		WHITE = 37,
		RESET = 0
	};

	class Logger {
	private:
		static std::thread loggerThread;
		static std::atomic<bool> running;
		static std::atomic<size_t> queueSize;

		static std::unique_ptr<boost::lockfree::queue<std::string*>> logQueue;

		static void process();
		static bool push(std::string* str);

	public:
		static void stop();
		static void start();

		static void setQueueSize(size_t size) {
			queueSize = size;
		}
		static const std::string toColor(LogColor color);
		static std::string getTimeString();

		static bool error(const std::string& message);
		static bool warn(const std::string& message);
		static bool success(const std::string& message);
		static bool info(const std::string& message);
		static bool print(const std::string& message);
	};
}
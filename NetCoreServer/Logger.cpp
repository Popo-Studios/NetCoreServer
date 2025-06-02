#include "pch.h"
#include "Logger.hpp"

namespace NetCoreServer {
	std::thread Logger::loggerThread;
	std::atomic<size_t> Logger::queueSize = 256;
	std::atomic<bool> Logger::running = false;
	std::unique_ptr<boost::lockfree::queue<std::string*>> Logger::logQueue = nullptr;

	void Logger::process() {
		while (running.load()) {
			if (logQueue && !logQueue->empty()) {
				std::string* logMessage = nullptr;
				if (logQueue->pop(logMessage)) {
					if (logMessage) {
						std::cout << *logMessage;
						delete logMessage;
					}
				}
			} else {
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}

		while (logQueue && !logQueue->empty()) {
			std::string* msg = nullptr;
			logQueue->pop(msg);
			delete msg;
		}
	}

	bool Logger::push(std::string* str) {
		if (logQueue->push(str)) {
			return true;
		} else {
			delete str;
			return false;
		}
	}

	void Logger::start() {
		if (!running.exchange(true)) {
			logQueue = std::make_unique<boost::lockfree::queue<std::string*>>(queueSize.load());
			loggerThread = std::thread(process);
		}
	}

	bool Logger::error(const std::string& message) {
		if (logQueue) {
			auto time = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
			std::string* str = new std::string(std::format("{}[{}] [ERROR] {}{}\n", toColor(LogColor::RED), getTimeString(), message, toColor(LogColor::RESET)));
			return push(str);
		} else return false;
	}

	bool Logger::warn(const std::string& message) {
		if (logQueue) {
			auto time = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
			std::string* str = new std::string(std::format("{}[{}] [WARN] {}{}\n", toColor(LogColor::YELLOW), getTimeString(), message, toColor(LogColor::RESET)));
			return push(str);
		} else return false;
	}

	bool Logger::info(const std::string& message) {
		if (logQueue) {
			auto time = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
			std::string* str = new std::string(std::format("{}[{}] [INFO] {}{}\n", toColor(LogColor::WHITE), getTimeString(), message, toColor(LogColor::RESET)));
			return push(str);
		} else return false;
	}

	bool Logger::success(const std::string& message) {
		if (logQueue) {
			std::string* str = new std::string(std::format("{}[{}] [SUCCESS] {}{}\n", toColor(LogColor::GREEN), getTimeString(), message, toColor(LogColor::RESET)));
			return push(str);
		} else return false;
	}

	bool Logger::print(const std::string& message) {
		if (logQueue) {
			std::string* str = new std::string(message);
			return push(str);
		} else return false;
	}

	void Logger::stop() {
		if (running.exchange(false)) {
			if (loggerThread.joinable()) {
				loggerThread.join();
			}
			
			logQueue.reset();
		}
	}

	const std::string Logger::toColor(LogColor color) {
		return "\033[" + std::to_string(static_cast<int>(color)) + "m";
	}

	std::string Logger::getTimeString() {
		auto now = std::chrono::system_clock::now();
		auto time = std::chrono::floor<std::chrono::seconds>(now);
		std::time_t t = std::chrono::system_clock::to_time_t(time);
		std::tm local_tm = safe_localtime(&t);

		char buf[20];
		std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local_tm);
		return std::string(buf);
	}
}
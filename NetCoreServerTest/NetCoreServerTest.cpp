#include <NetCoreServer.hpp>

using namespace NetCoreServer;
using namespace std;

class SimpleSession : public AbstractSession {
public:
	SimpleSession(SessionInfo info, SessionCreationOption opt) : AbstractSession(info, opt, 60.0) {

	}

	void tick(double t) override {

	}
};

SessionPtr gen(const SessionInfo& info, const SessionCreationOption& opt) {
	return make_shared<SimpleSession>(info, opt);
}

int main()
{
	initialize();
	Logger::start();
	LoginFunc func = [](LoginData data) -> LoginResult {
		LoginResult result;
		result.success = true;
		result.userIdentifier = { 1, "test" };
		return result;
	};
	MainServer mainServer(func, [](uint64_t _) {return ""; }, SessionServerOption{
		10, 10, 10, make_pair(6000, 6010)
	}, 12345, 10, 10);
	mainServer.registerSessionGenerator("", gen);
	
	mainServer.wait();

	return 0;
}
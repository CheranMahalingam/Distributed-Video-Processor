#include <boost/asio.hpp>
#include <string>
#include <vector>
#include <thread>

#include "node.h"

int main(int argc, char* argv[]) {
    boost::asio::io_context io;

    std::string address = argv[1];
    std::vector<std::string> peer_ids = {};
    for (int i = 2; i < argc; i++) {
        peer_ids.push_back(argv[i]);
    }

    raft::Node server(address, peer_ids, io);
    server.Run();

    return 0;
}

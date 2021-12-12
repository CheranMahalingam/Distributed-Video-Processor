#include "server.h"

namespace raft {

Server::Server(int id, std::vector<int>& peer_ids)
    : server_id_(id), peer_ids_(peer_ids) {
        boost::asio::io_context io_context;
    cm_ = std::unique_ptr<ConcensusModule>(new ConcensusModule(io_context, id, peer_ids));
}

void ConnectToPeer(int peer_id) {

}

}

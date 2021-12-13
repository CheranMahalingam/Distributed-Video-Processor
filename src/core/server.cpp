#include "server.h"

namespace raft {

Server::Server(std::string address, std::vector<std::string>& peer_ids)
    : address_(address), peer_ids_(peer_ids), id_(current_id) {
        boost::asio::io_context io_context;
    cm_ = std::unique_ptr<ConcensusModule>(new ConcensusModule(id_, io_context, peer_ids));
    current_id++;
}

void Server::ConnectToPeer(int peer_id) {

}

}

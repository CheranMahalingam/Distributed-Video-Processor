#ifndef SERVER_H
#define SERVER_H

#include <vector>
#include <unordered_map>
#include <memory>

#include "concensus_module.h"

namespace raft {

class Server {
public:
    Server(int id, std::vector<int>& peer_ids);

    void ConnectToPeer(int peer_id);

    void RequestVote();

    void AppendEntries();

private:
    int server_id_;
    std::vector<int> peer_ids_;
    std::unique_ptr<ConcensusModule> cm_;
};

}

#endif
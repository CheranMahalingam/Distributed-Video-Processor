#ifndef SERVER_H
#define SERVER_H

#include <vector>
#include <unordered_map>
#include <memory>
#include <string>

#include "concensus_module.h"

namespace raft {

class Server {
public:
    Server(std::string address, std::vector<std::string>& peer_ids);

    ~Server();

    void ConnectToPeer(int peer_id);

    void RequestVote();

    void AppendEntries();

    static int current_id;

private:
    int id_;
    std::string address_;
    std::vector<std::string> peer_ids_;
    std::unique_ptr<ConcensusModule> cm_;
};

}

#endif
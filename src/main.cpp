#include <boost/asio.hpp>
#include <grpc++/grpc++.h>
#include <string>
#include <vector>
#include <memory>
#include <thread>

#include "concensus_module.h"
#include "raft_server.h"
#include "client_callback_queue.h"
#include "commit_channel.h"
#include "chunk_manager.h"

using work_guard_type = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
using grpc::CompletionQueue;

int main(int argc, char* argv[]) {
    boost::asio::io_context io;
    // Prevents io_context from returning
    work_guard_type work_guard(io.get_executor());

    std::string address = argv[1];
    std::vector<std::string> peer_ids = {};
    for (int i = 2; i < argc; i++) {
        peer_ids.push_back(argv[i]);
    }

    CompletionQueue cq;
    std::shared_ptr<raft::ConcensusModule> cm(std::make_shared<raft::ConcensusModule>(io, address, peer_ids, cq));
    std::unique_ptr<raft::RaftServer> server(std::make_unique<raft::RaftServer>(address, peer_ids, cm));
    std::unique_ptr<raft::ClientCallbackQueue> reply_queue(std::make_unique<raft::ClientCallbackQueue>(peer_ids, cm, cq));
    // std::unique_ptr<file_system::ChunkManager> chunk_manager(std::make_unique<file_system::ChunkManager>(address, "../data", 2048, cm));

    std::thread commit_queue_loop(&raft::CommitChannel::ConsumeEvents, cm->channel_.get());
    std::thread server_event_loop(&raft::RaftServer::HandleRPC, server.get());
    std::thread reply_queue_loop(&raft::ClientCallbackQueue::AsyncRpcResponseHandler, reply_queue.get());

    boost::asio::steady_timer start_cluster(io);
    start_cluster.expires_from_now(std::chrono::seconds(5));
    start_cluster.wait();
    cm->ElectionTimeout(cm->current_term());
    // All async callbacks will be handled on the main thread
    io.run();

    server_event_loop.join();
    reply_queue_loop.join();
    commit_queue_loop.join();

    return 0;
}

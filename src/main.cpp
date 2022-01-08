#include <boost/asio.hpp>
#include <grpc++/grpc++.h>
#include <string>
#include <vector>
#include <memory>
#include <thread>

#include "concensus_module.h"
#include "client_callback_queue.h"
#include "commit_channel.h"
#include "chunk_manager.h"
#include "server_impl.h"
#include "test_server.hpp"

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
    std::shared_ptr<file_system::ChunkManager> manager(std::make_shared<file_system::ChunkManager>(address, "/data", 20));
    std::shared_ptr<raft::ConcensusModule> cm(std::make_shared<raft::ConcensusModule>(io, address, peer_ids, cq, manager)); 
    std::unique_ptr<ServerImpl> impl(std::make_unique<ServerImpl>(address, cm, manager));
    std::unique_ptr<raft::ClientCallbackQueue> reply_queue(std::make_unique<raft::ClientCallbackQueue>(peer_ids, cm, cq));

    std::thread commit_queue_loop(&raft::CommitChannel::ConsumeEvents, cm->channel_.get());
    std::thread server_event_loop(&ServerImpl::HandleRPC, impl.get());
    std::thread reply_queue_loop(&raft::ClientCallbackQueue::AsyncRpcResponseHandler, reply_queue.get());
    std::thread create_event_loop(&generate_load, address, cm);

    boost::asio::steady_timer start_cluster(io);
    start_cluster.expires_from_now(std::chrono::seconds(5));
    start_cluster.wait();
    cm->ElectionTimeout(cm->current_term());
    // All async callbacks will be handled on the main thread
    io.run();

    create_event_loop.join();
    reply_queue_loop.join();
    commit_queue_loop.join();
    server_event_loop.join();

    return 0;
}

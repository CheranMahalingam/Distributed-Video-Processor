#include <boost/asio.hpp>
#include <grpc++/grpc++.h>
#include <string>
#include <vector>
#include <memory>
#include <thread>

#include "concensus_module.h"
#include "rpc_client.h"
#include "rpc_server.h"
#include "client_callback_queue.h"
#include "command_log.h"

using work_guard_type = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
using grpc::CompletionQueue;

int main(int argc, char* argv[]) {
    boost::asio::io_context io;
    work_guard_type work_guard(io.get_executor());

    std::string address = argv[1];
    std::vector<std::string> peer_ids = {};
    for (int i = 2; i < argc; i++) {
        peer_ids.push_back(argv[i]);
    }

    CompletionQueue cq;

    std::unique_ptr<raft::RpcClient> client(std::make_unique<raft::RpcClient>(address, peer_ids, cq));
    std::unique_ptr<raft::CommandLog> log(std::make_unique<raft::CommandLog>(peer_ids));
    std::shared_ptr<raft::ConcensusModule> cm(std::make_shared<raft::ConcensusModule>(io, address, peer_ids, std::move(client), std::move(log)));
    std::unique_ptr<raft::RpcServer> server(std::make_unique<raft::RpcServer>(io, address, peer_ids, cm));
    std::unique_ptr<raft::ClientCallbackQueue> reply_queue(std::make_unique<raft::ClientCallbackQueue>(peer_ids, cm, cq));

    std::thread server_event_loop(&raft::RpcServer::HandleRPC, server.get());
    std::thread reply_queue_loop(&raft::ClientCallbackQueue::AsyncRpcResponseHandler, reply_queue.get());

    boost::asio::steady_timer start_cluster(io);
    start_cluster.expires_from_now(std::chrono::seconds(10));
    start_cluster.wait();
    cm->ElectionTimeout(cm->current_term());
    io.run();

    reply_queue_loop.join();
    server_event_loop.join();

    return 0;
}

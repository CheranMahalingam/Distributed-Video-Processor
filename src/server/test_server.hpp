#include <string>
#include <memory>
#include <boost/asio.hpp>
#include <grpc++/grpc++.h>

#include "concensus_module.h"
#include "raft.grpc.pb.h"
#include "file_system.grpc.pb.h"
#include "server.grpc.pb.h"
#include "log.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

void generate_load(std::string address, std::shared_ptr<raft::ConcensusModule> cm) {
    boost::asio::io_context io;
    std::shared_ptr<Channel> chan = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    auto stub = server::VideoProcessorService::NewStub(chan);
    while (true) {
        std::string str("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
        std::random_device rd;
        std::mt19937 generator(rd());
        std::shuffle(str.begin(), str.end(), generator);

        boost::asio::steady_timer send_event(io);
        send_event.expires_from_now(std::chrono::seconds(1));
        send_event.wait();

        storage::UploadVideoRequest request;
        request.set_data(str);
        request.set_version(1);
        request.set_id(str.substr(0, 7));

        storage::UploadVideoResponse reply;
        ClientContext ctx;

        auto status = stub->UploadVideo(&ctx, request, &reply);

        if (status.ok()) {
            logger(LogLevel::Debug) << reply.success();
        }
    }
}
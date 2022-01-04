#ifndef ASYNC_SERVER_H
#define ASYNC_SERVER_H

#include <grpc++/grpc++.h>

#include "grpc_msg_defs.h"
#include "concensus_module.h"
#include "server.grpc.pb.h"

using grpc::ServerCompletionQueue;
using grpc::ServerContext;

class AsyncServer {
public:
    AsyncServer();

    virtual ~AsyncServer();

protected:
    struct Tag {
        void* call;
        RpcCommandID id;
    };

    class CallData {
    public:
        CallData(
            server::VideoProcessorService::AsyncService* service,
            ServerCompletionQueue* scq,
            raft::ConcensusModule* cm);

        virtual void Proceed() = 0;

    protected:
        enum class CallStatus {
            Create,
            Process,
            Finish
        };
        server::VideoProcessorService::AsyncService* service_;
        ServerCompletionQueue* scq_;
        raft::ConcensusModule* cm_;
        ServerContext ctx_;
        CallStatus status_;
    };
};

#endif

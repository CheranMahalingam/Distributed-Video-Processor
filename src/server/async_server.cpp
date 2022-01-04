#include "async_server.h"

AsyncServer::AsyncServer() {
}

AsyncServer::~AsyncServer() {
}

AsyncServer::CallData::CallData(
    server::VideoProcessorService::AsyncService* service,
    ServerCompletionQueue* scq,
    raft::ConcensusModule* cm)
    : service_(service), scq_(scq), cm_(cm), status_(CallStatus::Create) {
}

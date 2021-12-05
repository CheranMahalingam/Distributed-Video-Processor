#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::Status;

class AsyncServer final {
  public:
    ~AsyncServer() {

    }
  
  private:
    // std::unique_ptr<ServerCompletionQueue> cq_;

};

int main() {
  return 0;
}
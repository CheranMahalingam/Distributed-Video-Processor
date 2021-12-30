# Distributed-Video-Processor
A video storage system designed using the [raft](https://raft.github.io/) protocol.

# Building
Prerequisites:
- boost >=1.71.0
- protobuf >=3.6.0
- grpc >=1.41.0

> Note: Currently only compiles on *nix based systems

Run
```
cmake -S . -B build
cmake --build build
```

# Usage
For each server run the executable and specify arguments for the addresses of the servers. For instance,
```
cd build/bin
./VideoProcessor 127.0.0.1:3001 127.0.0.1:3002 127.0.0.1:3003
```
will start a server at `127.0.0.1:3001` and send requests to `127.0.0.1:3002` and `127.0.0.1:3003`

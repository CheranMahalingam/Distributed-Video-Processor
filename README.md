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
./VideoProcessor localhost:3001 localhost:3002 localhost:3003
```
will start a server at `localhost:3001` and send requests to `localhost:3002` and `localhost:3003`

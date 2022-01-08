# Distributed-Video-Processor
A video storage system designed using the [raft](https://raft.github.io/) protocol.

# Usage
Prerequisites:
- docker >= 20.10.5
- docker-compose >= 1.29.0

To start a three node raft cluster run,
```
docker-compose up
```
To stop the cluster run,
```
docker-compose down
```
To remove the raft logs and videos persisted to disk run,
```
chmod +x clean.sh
./clean.sh
```

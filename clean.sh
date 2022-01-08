#!/bin/bash
docker volume rm \
    distributed-video-processor_data_1 \
    distributed-video-processor_data_2 \
    distributed-video-processor_data_3 \
    distributed-video-processor_snapshot_1 \
    distributed-video-processor_snapshot_2 \
    distributed-video-processor_snapshot_3

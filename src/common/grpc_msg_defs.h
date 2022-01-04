#ifndef GRPC_MSG_DEFS
#define GRPC_MSG_DEFS

enum class RaftMessageID {
    RequestVote,
    AppendEntries
};

enum class VideoMessageID {
    Upload,
    Download,
    Delete
};

#endif

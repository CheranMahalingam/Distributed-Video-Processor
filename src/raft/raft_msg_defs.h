#ifndef RAFT_MSG_DEFS
#define RAFT_MSG_DEFS

namespace raft {

enum class MessageID {
    RequestVote,
    AppendEntries
};

}

#endif

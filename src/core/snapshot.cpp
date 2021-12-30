#include "snapshot.h"

namespace raft {

Snapshot::Snapshot(const std::string dir) {
    std::filesystem::create_directories(dir);
    log_file_ = dir + "log";
    state_file_ = dir + "state";
}

void Snapshot::PersistState(const int term, const std::string vote_id) {
    std::fstream out(state_file_, std::ios::out | std::ios::trunc | std::ios::binary);

    rpc::State state;
    state.set_term(term);
    state.set_voteid(vote_id);
    SerializeDelimitedToOstream(state, &out);
    logger(LogLevel::Debug) << "Persisted state to disk";

    out.close();
}

void Snapshot::PersistLog(const std::vector<rpc::LogEntry>& entries, const bool append) {
    auto flags = std::ios::out | std::ios::binary;
    if (!append) {
        flags |= std::ios::trunc;
    }
    std::fstream out(log_file_, flags);
    for (auto &entry:entries) {
        SerializeDelimitedToOstream(entry, &out);
    }
    logger(LogLevel::Debug) << "Persisted log to disk";
}

std::tuple<int, std::string> Snapshot::RestoreState() {
    std::fstream in(state_file_, std::ios::in | std::ios::binary);
    IstreamInputStream state_stream(&in);

    rpc::State state;
    ParseDelimitedFromZeroCopyStream(&state, &state_stream, nullptr);

    logger(LogLevel::Debug) << "Restoring from disk, term =" << state.term() << "voteId =" << state.voteid();

    in.close();

    return std::make_tuple(state.term(), state.voteid());
}

std::vector<rpc::LogEntry> Snapshot::RestoreLog() {
    std::fstream in(log_file_, std::ios::in | std::ios::binary);
    IstreamInputStream log_stream(&in);

    std::vector<rpc::LogEntry> entries;
    rpc::LogEntry entry;
    while (ParseDelimitedFromZeroCopyStream(&entry, &log_stream, nullptr)) {
        entries.push_back(entry);
        logger(LogLevel::Info) << entry.command();
    }

    in.close();

    return entries;
}

bool Snapshot::Empty() const {
    std::ifstream state(state_file_, std::ios::binary);
    std::ifstream log(log_file_, std::ios::binary);
    return state.peek() == std::ifstream::traits_type::eof() && 
        log.peek() == std::ifstream::traits_type::eof();
}

}

#ifndef LOCK_MANAGER_H
#define LOCK_MANAGER_H

#include <shared_mutex>
#include <unordered_map>
#include <string>
#include <memory>
#include <functional>

namespace file_system {

class LockManager {
public:
    enum class LockType {
        Read,
        Write,
        Delete
    };

    LockManager();

    void FetchLock(const std::string& filename, const LockType type);

    void Unlock(const std::string& filename, const LockType type);

private:
    void CreateLock(const std::string& filename);

    bool LockExists(const std::string& filename);

private:
    std::unordered_map<std::string, std::unique_ptr<std::shared_mutex>> file_locks_;
};

}

#endif

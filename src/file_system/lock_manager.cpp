#include "lock_manager.h"

namespace file_system {

LockManager::LockManager() {
}

void LockManager::FetchLock(const std::string& filename, const LockType type) {
    if (!LockExists(filename) && type == LockType::Write) {
        CreateLock(filename);
    }

   switch(type) {
        case LockType::Read: {
            file_locks_[filename]->lock_shared();
            break;
        }
        case LockType::Write: {
            file_locks_[filename]->lock();
            break;
        }
        case LockType::Delete: {
            file_locks_[filename]->lock();
            break;
        }
    }
}

void LockManager::Unlock(const std::string& filename, const LockType type) {
    switch(type) {
        case LockType::Read: {
            file_locks_[filename]->unlock_shared();
            break;
        }
        case LockType::Write: {
            file_locks_[filename]->unlock();
            break;
        }
        case LockType::Delete: {
            file_locks_[filename]->unlock();
            file_locks_.erase(filename);
            break;
        }
    }
}

void LockManager::CreateLock(const std::string& filename) {
    file_locks_.emplace(filename, std::make_unique<std::shared_mutex>());
}

bool LockManager::LockExists(const std::string& filename) {
    return file_locks_.find(filename) != file_locks_.end();
}

}

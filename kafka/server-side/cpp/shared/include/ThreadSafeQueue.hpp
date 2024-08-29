/*
** CELTE, 2024
** celte
**
** Team members:
** Eliot Janvier
** Clement Toni
** Ewen Briand
** Laurent Jiang
** Thomas Laprie
**
** File description:
** ThreadSafeQueue.hpp
*/

#pragma once
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>

namespace celte {
    namespace utils {
        /**
         * @brief Warps std::queue in a thread safe way
         *
         * @tparam T
         */
        template <typename T> class ThreadSafeQueue {
        public:
            void Push(T value)
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _dataQueue.push(std::move(value));
                _condVar.notify_one();
            }

            std::optional<T> Pop()
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _condVar.wait(lock,
                    [this] { return !_dataQueue.empty() || !_isRunning; });
                if (not _isRunning) {
                    return std::nullopt;
                }
                T value = std::move(_dataQueue.front());
                _dataQueue.pop();
                return value;
            }

            void Stop()
            {
                _isRunning = false;
                _condVar.notify_all();
            }

        private:
            std::queue<T> _dataQueue;
            std::mutex _mutex;
            std::condition_variable _condVar;
            std::atomic_bool _isRunning = true;
        };
    } // namespace utils
} // namespace celte
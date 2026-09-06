#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace Hybrid
{
    struct JobSystemConfig
    {
        std::size_t worker_count = 0; // 0 selects max(1, hardware_concurrency - 1).
    };

    enum class JobShutdownMode
    {
        Drain,
    };

    class JobSystem
    {
    public:
        JobSystem() = default;
        ~JobSystem();

        JobSystem(const JobSystem&) = delete;
        JobSystem& operator=(const JobSystem&) = delete;

        bool initialize(const JobSystemConfig& config = {});
        void waitIdle();
        void shutdown(JobShutdownMode mode = JobShutdownMode::Drain) noexcept;

        bool isRunning() const;
        std::size_t workerCount() const;

        template<typename Function>
        auto submit(Function&& function) -> std::future<std::invoke_result_t<Function>>
        {
            using Result = std::invoke_result_t<Function>;
            auto task = std::make_shared<std::packaged_task<Result()>>(std::forward<Function>(function));
            std::future<Result> future = task->get_future();
            {
                std::scoped_lock lock(m_mutex);
                if (!m_accepting)
                    throw std::runtime_error("JobSystem is not accepting jobs");
                m_jobs.emplace_back([task]() { (*task)(); });
            }
            m_work_available.notify_one();
            return future;
        }

    private:
        void workerLoop();

        mutable std::mutex m_mutex;
        std::condition_variable m_work_available;
        std::condition_variable m_idle;
        std::deque<std::function<void()>> m_jobs;
        std::vector<std::thread> m_workers;
        std::size_t m_active_jobs = 0;
        bool m_accepting = false;
        bool m_stopping = false;
    };
} // namespace Hybrid

#include "job_system.h"

#include <algorithm>

namespace Hybrid
{
    JobSystem::~JobSystem()
    {
        shutdown();
    }

    bool JobSystem::initialize(const JobSystemConfig& config)
    {
        std::scoped_lock lock(m_mutex);
        if (m_accepting)
            return true;

        const unsigned int hardware_threads = std::thread::hardware_concurrency();
        const std::size_t default_count = hardware_threads > 1 ? hardware_threads - 1 : 1;
        const std::size_t count = std::max<std::size_t>(1, config.worker_count == 0 ? default_count : config.worker_count);

        m_stopping = false;
        m_accepting = true;
        try
        {
            m_workers.reserve(count);
            for (std::size_t i = 0; i < count; ++i)
                m_workers.emplace_back([this]() { workerLoop(); });
        }
        catch (...)
        {
            m_accepting = false;
            m_stopping = true;
            m_work_available.notify_all();
            throw;
        }
        return true;
    }

    void JobSystem::waitIdle()
    {
        std::unique_lock lock(m_mutex);
        m_idle.wait(lock, [this]() { return m_jobs.empty() && m_active_jobs == 0; });
    }

    void JobSystem::shutdown(JobShutdownMode mode) noexcept
    {
        (void)mode; // Drain is the only supported mode in this milestone.
        {
            std::scoped_lock lock(m_mutex);
            if (m_workers.empty() && !m_accepting)
                return;
            m_accepting = false;
            m_stopping = true;
        }
        m_work_available.notify_all();

        for (std::thread& worker : m_workers)
        {
            if (worker.joinable())
                worker.join();
        }

        std::scoped_lock lock(m_mutex);
        m_workers.clear();
        m_jobs.clear();
        m_active_jobs = 0;
        m_stopping = false;
        m_idle.notify_all();
    }

    bool JobSystem::isRunning() const
    {
        std::scoped_lock lock(m_mutex);
        return m_accepting;
    }

    std::size_t JobSystem::workerCount() const
    {
        std::scoped_lock lock(m_mutex);
        return m_workers.size();
    }

    void JobSystem::workerLoop()
    {
        for (;;)
        {
            std::function<void()> job;
            {
                std::unique_lock lock(m_mutex);
                m_work_available.wait(lock, [this]() { return m_stopping || !m_jobs.empty(); });
                if (m_jobs.empty())
                {
                    if (m_stopping)
                        return;
                    continue;
                }
                job = std::move(m_jobs.front());
                m_jobs.pop_front();
                ++m_active_jobs;
            }

            job();

            {
                std::scoped_lock lock(m_mutex);
                --m_active_jobs;
                if (m_jobs.empty() && m_active_jobs == 0)
                    m_idle.notify_all();
            }
        }
    }
} // namespace Hybrid

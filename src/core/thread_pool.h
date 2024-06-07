#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <queue>
#include <thread>
#include <vector>
#include <vk_backend/vk_scene.h>

class ThreadPool {
public:
  ~ThreadPool() { destroy(); }
  void create();
  void destroy();
  template <typename Func, typename... Args>
  std::future<void> push_job(Func&& func, Args&&... args) {
    using ReturnType = void;
    auto job = std::make_shared<std::packaged_task<ReturnType()>>(
        [func = std::forward<Func>(func), ... args = std::forward<Args>(args)]() mutable {
          std::invoke(func, args...);
        });

    std::future<ReturnType> res = job->get_future();
    {
      std::unique_lock<std::mutex> lock(_queue_mutex);
      _jobs.emplace([job]() { (*job)(); });
    }
    _cv.notify_one();
    return res;
  }

  std::queue<std::function<void()>> _jobs;

private:
  std::vector<std::thread> _threads;
  std::condition_variable _cv;
  std::mutex _queue_mutex;
  bool _stop;
};

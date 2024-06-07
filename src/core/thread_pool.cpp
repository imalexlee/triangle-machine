#include "thread_pool.h"

void ThreadPool::create() {
  // try doing
  // threads - 1
  // because the
  // main thread
  // counts as a
  // thread
  int thread_count = std::thread::hardware_concurrency();
  for (int i = 0; i < thread_count; ++i) {
    _threads.emplace_back([this] {
      while (true) {
        std::function<void()> job;
        {
          std::unique_lock<std::mutex> lock(_queue_mutex);
          _cv.wait(lock, [this] { return !_jobs.empty() || _stop; });
          if (_stop && _jobs.empty()) {
            return;
          }

          // Get the
          // next
          // task
          // from
          // the
          // queue
          job = std::move(_jobs.front());
          _jobs.pop();
        }

        job();
      }
    });
  }
}
// void
// ThreadPool::push_job(std::function<void()>
// job) {
//   {
//     std::unique_lock<std::mutex>
//     lock(_queue_mutex);
//     _jobs.emplace(std::move(job));
//   }
//   _cv.notify_one();
// }

void ThreadPool::destroy() {
  {
    std::unique_lock<std::mutex> lock(_queue_mutex);
    _stop = true;
  }

  _cv.notify_all();

  for (auto& thread : _threads) {
    thread.join();
  }
}

#include "tasksys.h"
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int num_threads) {}
ITaskSystem::~ITaskSystem() {}

/*
 * ================================================================
 * Serial task system implementation
 * ================================================================
 */

const char *TaskSystemSerial::name() { return "Serial"; }

TaskSystemSerial::TaskSystemSerial(int num_threads)
    : ITaskSystem(num_threads) {}

TaskSystemSerial::~TaskSystemSerial() {}

void TaskSystemSerial::run(IRunnable *runnable, int num_total_tasks) {
  for (int i = 0; i < num_total_tasks; i++) {
    runnable->runTask(i, num_total_tasks);
  }
}

TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable *runnable,
                                          int num_total_tasks,
                                          const std::vector<TaskID> &deps) {
  // You do not need to implement this method.
  return 0;
}

void TaskSystemSerial::sync() {
  // You do not need to implement this method.
  return;
}

/*
 * ================================================================
 * Parallel Task System Implementation
 * ================================================================
 */

const char *TaskSystemParallelSpawn::name() {
  return "Parallel + Always Spawn";
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads)
    : ITaskSystem(num_threads), _num_threads(num_threads) {}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable *runnable, int num_total_tasks) {

  std::vector<std::thread> threads = {};
  for (int thread_idx = 0; thread_idx < _num_threads; ++thread_idx) {
    threads.push_back(
        std::thread([&runnable, num_total_tasks, thread_idx, this] {
          for (int j = thread_idx; j < num_total_tasks; j += _num_threads) {
            runnable->runTask(j, num_total_tasks);
          }
        }));
  }
  for (std::thread &t : threads) {
    t.join();
  }
  return;
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(
    IRunnable *runnable, int num_total_tasks, const std::vector<TaskID> &deps) {
  // You do not need to implement this method.
  return 0;
}

void TaskSystemParallelSpawn::sync() {
  // You do not need to implement this method.
  return;
}

/*
 * ================================================================
 * Parallel Thread Pool Spinning Task System Implementation
 * ================================================================
 */

const char *TaskSystemParallelThreadPoolSpinning::name() {
  return "Parallel + Thread Pool + Spin";
}

// CONSTRUCTOR
TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(
    int num_threads)
    : ITaskSystem(num_threads), _num_threads(num_threads),
      _thread_pool(num_threads), _stop(false) {

  for (int i = 0; i < num_threads; ++i) {
    _thread_pool[i] = std::thread([this, i]() {
      thread_local int local_count = 0;
      while (true) {
        /* printf("In thread %d\n", i); */
        std::function<void()> task;
        {
          std::unique_lock<std::mutex> lock(_just_a_mutex);
          if (_task_queue.empty()) {
            if (local_count > 0) {
              _completed_tasks.fetch_add(local_count);
              local_count = 0;
            }
            /* _work_available.wait( */
            /*     lock, [this] { return _stop || !_task_queue.empty(); }); */
            if (_stop) {
              /* printf("thread %d, Stopping\n", i); */
              return;
            } else {
              continue;
            }
          }

          task = std::move(_task_queue.front());
          _task_queue.pop();
        }
        task();
        local_count++;
      }
    });
  }
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {
  {
    std::lock_guard<std::mutex> lock(_just_a_mutex);
    _stop = true;
  }
  /* _work_available.notify_all(); */
  for (auto &thread : _thread_pool) {
    thread.join();
  }
}

// RUN
void TaskSystemParallelThreadPoolSpinning::run(IRunnable *runnable,
                                               int num_total_tasks) {
  _completed_tasks.store(0);
  for (int i = 0; i < num_total_tasks; ++i) {
    std::function<void()> task = [runnable, i, num_total_tasks] {
      runnable->runTask(i, num_total_tasks);
    };
    {
      std::lock_guard<std::mutex> lock(_just_a_mutex);
      _task_queue.emplace(std::move(task));
    }
  }
  /* _work_available.notify_all(); */

  while (_completed_tasks.load() < num_total_tasks) {
  }
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(
    IRunnable *runnable, int num_total_tasks, const std::vector<TaskID> &deps) {
  // You do not need to implement this method.
  return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
  // You do not need to implement this method.
  return;
}

/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

const char *TaskSystemParallelThreadPoolSleeping::name() {
  return "Parallel + Thread Pool + Sleep";
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(
    int num_threads)
    : ITaskSystem(num_threads), _num_threads(num_threads),
      _thread_pool(num_threads), _stop(false), _num_runs(0) {

  for (int i = 0; i < num_threads; ++i) {
    _thread_pool[i] = std::thread([this, i]() {
      thread_local int local_count = 0;
      std::vector<std::function<void()>> local_tasks;
      bool accounted_for = false;
      std::string lock_name = "thread_lock" + std::to_string(i);
      while (true) {
        /* printf("In thread %d\n", i); */
        /* std::function<void()> task; */
        {
          auto start_time = std::chrono::high_resolution_clock::now();
          std::unique_lock<std::mutex> lock(_just_a_mutex);
          auto end_time = std::chrono::high_resolution_clock::now();
          auto sleep_duration =
              std::chrono::duration_cast<std::chrono::microseconds>(end_time -
                                                                    start_time);

          total_time[lock_name] +=
              static_cast<long long>(sleep_duration.count());
          if (_task_queue.empty()) {
            if (local_count > 0) {
              int last_complete = _completed_tasks.fetch_add(local_count);
              lock.unlock();
              {
                std::lock_guard<std::mutex> we_lock(_another_mutex);
                _work_completed.notify_all();
              }
              local_count = 0;
              lock.lock();
            }
            accounted_for = false;
            /* printf("%d WAITING\n", i); */
            _work_available.wait(
                lock, [this] { return _stop || !_task_queue.empty(); });
            /* printf("%d FREE\n", i); */

            if (_stop) {
              return;
            }
          }

          local_tasks.clear();
          while (!_task_queue.empty() && local_tasks.size() < 4) {
            local_tasks.push_back(std::move(_task_queue.front()));
            _task_queue.pop();
          }
          /* task = std::move(_task_queue.front()); */
          /* _task_queue.pop(); */
        }
        if (!accounted_for) {
          _num_threads_unleashed.fetch_add(1);
          /* printf("%d UNLEAHSED\n", i); */
          accounted_for = true;
        }
        auto start_time = std::chrono::high_resolution_clock::now();
        for (auto &task : local_tasks) {
          task();
          local_count++;
        }
        auto end_time = std::chrono::high_resolution_clock::now();
        auto sleep_duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end_time -
                                                                  start_time);
        total_time[lock_name + "task"] += static_cast<long long>(sleep_duration.count());
        /* if (local_count >= 32) { */
        /*   int last_complete = _completed_tasks.fetch_add(local_count); */
        /*   if (last_complete + local_count >= _num_tasks) { */
        /*       std::lock_guard<std::mutex> we_lock(_another_mutex); */
        /*       _work_completed.notify_all(); */
        /*   } */
        /*   local_count = 0; */
        /* } */
      }
    });
  }
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
  {
    std::lock_guard<std::mutex> lock(_just_a_mutex);
    _stop = true;
  }
  _work_available.notify_all();
  for (auto &thread : _thread_pool) {
    thread.join();
  }
  for (int i = 0; i < _num_threads; ++i) {
    std::string lock_name = "thread_lock" + std::to_string(i);
    printf("Avg task for thread %d: %lld lock: %lld\n", i,
           total_time[lock_name + "task"] / _num_runs,
           total_time[lock_name] / _num_runs);
  }
  printf("Number of runs: %d, average time asleep %lld, average time top pop "
         "%lld, averae time notify %lld\n",
         _num_runs, total_time["main_thread_sleep"] / _num_runs,
         total_time["populate"] / _num_runs, total_time["main_thread_notify"] / _num_runs);
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable *runnable,
                                               int num_total_tasks) {
  _num_runs += 1;
  _num_tasks.store(num_total_tasks);
  _completed_tasks.store(0);
  _num_threads_unleashed.store(0);
  auto start_time = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < num_total_tasks; ++i) {
    {
      std::lock_guard<std::mutex> lock(_just_a_mutex);
      _task_queue.emplace([this, runnable, i, num_total_tasks] {
        runnable->runTask(i, num_total_tasks);
      });
    }
  }
  auto end_time = std::chrono::high_resolution_clock::now();
  auto sleep_duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time);
  total_time["populate"] += static_cast<long long>(sleep_duration.count());
  /* printf("Time to enqueue %d tasks: %lld microseconds\n", num_total_tasks, */
  /*        static_cast<long long>(sleep_duration.count())); */
  start_time = std::chrono::high_resolution_clock::now();
  /* while (_num_threads_unleashed.load() < _num_threads && */
  /*        _completed_tasks < num_total_tasks) { */
    _work_available.notify_all();
    /* printf("NUM UNLEASED %d NUM THREADS %d \n",
     * _num_threads_unleashed.load(), _num_threads); */
  /* } */
  end_time = std::chrono::high_resolution_clock::now();
  sleep_duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time);
  /* printf("YAYA %lld\n", static_cast<long long>(sleep_duration.count())); */
  total_time["main_thread_notify"] +=
      static_cast<long long>(sleep_duration.count());

  start_time = std::chrono::high_resolution_clock::now();
  /* std::unique_lock<std::mutex> lock(_just_a_mutex); */
  std::unique_lock<std::mutex> lock(_another_mutex);
  _work_completed.wait(lock, [this, num_total_tasks] {
    /* printf("completed_tasks %d\n", _completed_tasks.load()); */
    return _completed_tasks >= (num_total_tasks);
  });
  /* while (_completed_tasks.load() < num_total_tasks) { */
  /* } */
  end_time = std::chrono::high_resolution_clock::now();
  sleep_duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time);
  total_time["main_thread_sleep"] +=
      static_cast<long long>(sleep_duration.count());
  /* printf("Thread slept for %lld microseconds\n", */
  /*        static_cast<long long>(sleep_duration.count())); */
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(
    IRunnable *runnable, int num_total_tasks, const std::vector<TaskID> &deps) {

  //
  // TODO: CS149 students will implement this method in Part B.
  //

  return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync() {

  //
  // TODO: CS149 students will modify the implementation of this method in Part
  // B.
  //

  return;
}

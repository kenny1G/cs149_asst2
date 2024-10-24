#include "tasksys.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
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
              if (_completed_tasks >= _num_tasks) {
                _work_completed.notify_one();
              }
            }
            _work_available.wait(
                lock, [this] { return _stop || !_task_queue.empty(); });
            if (_stop) {
              /* printf("thread %d, Stopping\n", i); */
              return;
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

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
  {
    std::lock_guard<std::mutex> lock(_just_a_mutex);
    _stop = true;
  }
  _work_available.notify_all();
  for (auto &thread : _thread_pool) {
    thread.join();
  }
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable *runnable,
                                               int num_total_tasks) {
  _num_tasks.store(num_total_tasks);
  _completed_tasks.store(0);
  for (int i = 0; i < num_total_tasks; ++i) {
    {
      std::lock_guard<std::mutex> lock(_just_a_mutex);
      std::function<void()> task = [this, runnable, i, num_total_tasks] {
        runnable->runTask(i, num_total_tasks);
      };
      _task_queue.emplace(std::move(task));
    }
  }
  _work_available.notify_all();

  std::unique_lock<std::mutex> lock(_just_a_mutex);
  _work_completed.wait(lock, [this, num_total_tasks] {
    return _completed_tasks >= num_total_tasks;
  });
  /* while (_completed_tasks.load() < num_total_tasks) { */
  /* } */
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

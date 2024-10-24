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

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(
    int num_threads)
    : ITaskSystem(num_threads), _num_threads(num_threads),
      _thread_pool(num_threads), _stop(false) {

  for (int i = 0; i < num_threads; ++i) {
    _thread_pool[i] = std::thread([this]() {
      std::vector<std::function<void()>> tasks;
      while (true) {
        {
          std::unique_lock<std::mutex> lock(_just_a_mutex);
          _work_available.wait(
              lock, [this] { return _stop || !_task_queue.empty(); });
          if (_stop && _task_queue.empty()) {
            /* printf("Stopping\n"); */
            return;
          }
          tasks = std::move(_task_queue.front());
          _task_queue.pop();
        }

        for (auto &task : tasks) {
          task();
        }
      }
    });
  }
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {
  {
    std::lock_guard<std::mutex> lock(_just_a_mutex);
    _stop = true;
  }
  _work_available.notify_all();
  for (auto &thread : _thread_pool) {
    thread.join();
  }
}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable *runnable,
                                               int num_total_tasks) {
  std::atomic<int> completed_tasks(0);
  int chunk_size = (num_total_tasks + _num_threads - 1) / _num_threads;

  for (int j = 0; j < _num_threads; ++j) {
    std::vector<std::function<void()>> thread_tasks;
    {
      std::lock_guard<std::mutex> lock(_just_a_mutex);
      int chunk_end_index = (j + 1) * chunk_size;
      for (int i = j * chunk_size; i < chunk_end_index && i < num_total_tasks;
           ++i) {
        std::function<void()> task = [runnable, i, chunk_size, chunk_end_index, num_total_tasks,
                                      &completed_tasks] {
          runnable->runTask(i, num_total_tasks);
          if (i == chunk_end_index - 1) {
            completed_tasks.fetch_add(chunk_size);
          }
          /* printf("Complted tasks: %d\n", completed_tasks.load()); */
        };
        thread_tasks.emplace_back(std::move(task));
        /* printf("the fuck"); */
      }
      /* printf("Assigned %zu task: \n", thread_tasks.size()); */
      _task_queue.emplace(std::move(thread_tasks));
    }
    _work_available.notify_one();
  }

  while (completed_tasks.load() < num_total_tasks) {
    /* printf("Completed tasks: %d\n", completed_tasks.load()); */
  }
  /* runnable->runTask(i, num_total_tasks); */
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
    _thread_pool[i] = std::thread([this]() {
      std::vector<std::function<void()>> tasks;
      while (true) {
        {
          std::unique_lock<std::mutex> lock(_just_a_mutex);
          _work_available.wait(
              lock, [this] { return _stop || !_task_queue.empty(); });
          if (_stop && _task_queue.empty()) {
            /* printf("Stopping\n"); */
            return;
          }
          tasks = std::move(_task_queue.front());
          _task_queue.pop();
        }

        for (auto &task : tasks) {
          task();
        }
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

  std::atomic<int> completed_tasks(0);
  int chunk_size = (num_total_tasks + _num_threads - 1) / _num_threads;

  for (int j = 0; j < _num_threads; ++j) {
    std::vector<std::function<void()>> thread_tasks;
    {
      std::lock_guard<std::mutex> lock(_just_a_mutex);
      int chunk_end_index = (j + 1) * chunk_size;
      for (int i = j * chunk_size; i < chunk_end_index && i < num_total_tasks;
           ++i) {
        std::function<void()> task = [runnable, i, num_total_tasks,
                                      &completed_tasks] {
          runnable->runTask(i, num_total_tasks);
          completed_tasks.fetch_add(1);
          /* printf("Complted tasks: %d\n", completed_tasks.load()); */
        };
        thread_tasks.emplace_back(std::move(task));
        /* printf("the fuck"); */
      }
      /* printf("Assigned %zu task: \n", thread_tasks.size()); */
      _task_queue.emplace(std::move(thread_tasks));
    }
    _work_available.notify_one();
  }

  while (completed_tasks.load() < num_total_tasks) {
    /* printf("Completed tasks: %d\n", completed_tasks.load()); */
  }
  /* runnable->runTask(i, num_total_tasks); */
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

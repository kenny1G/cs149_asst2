#include "tasksys.h"
#include "itasksys.h"
#include <algorithm>
#include <cstddef>
#include <future>
#include <mutex>
#include <thread>
#include <atomic>

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
  for (int i = 0; i < num_total_tasks; i++) {
    runnable->runTask(i, num_total_tasks);
  }

  return 0;
}

void TaskSystemSerial::sync() { return; }

/*
 * ================================================================
 * Parallel Task System Implementation
 * ================================================================
 */

const char *TaskSystemParallelSpawn::name() {
  return "Parallel + Always Spawn";
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads)
    : ITaskSystem(num_threads) {
  // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn
  // in Part B.
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable *runnable, int num_total_tasks) {
  // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn
  // in Part B.
  for (int i = 0; i < num_total_tasks; i++) {
    runnable->runTask(i, num_total_tasks);
  }
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(
    IRunnable *runnable, int num_total_tasks, const std::vector<TaskID> &deps) {
  // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn
  // in Part B.
  for (int i = 0; i < num_total_tasks; i++) {
    runnable->runTask(i, num_total_tasks);
  }

  return 0;
}

void TaskSystemParallelSpawn::sync() {
  // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn
  // in Part B.
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
    : ITaskSystem(num_threads) {
  // NOTE: CS149 students are not expected to implement
  // TaskSystemParallelThreadPoolSpinning in Part B.
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable *runnable,
                                               int num_total_tasks) {
  // NOTE: CS149 students are not expected to implement
  // TaskSystemParallelThreadPoolSpinning in Part B.
  for (int i = 0; i < num_total_tasks; i++) {
    runnable->runTask(i, num_total_tasks);
  }
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(
    IRunnable *runnable, int num_total_tasks, const std::vector<TaskID> &deps) {
  // NOTE: CS149 students are not expected to implement
  // TaskSystemParallelThreadPoolSpinning in Part B.
  for (int i = 0; i < num_total_tasks; i++) {
    runnable->runTask(i, num_total_tasks);
  }

  return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
  // NOTE: CS149 students are not expected to implement
  // TaskSystemParallelThreadPoolSpinning in Part B.
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

void thread_fn(ThreadsState *thread_state) {
  while (true) {
    if (thread_state->dead_)
      break;
    // Note: better name for bulk is probably bundle... but i'm in too deep

    // Critical section to claim a bulk in which to look for a task to execute
    BulkNode *bulk_to_search;
    {
      std::unique_lock<std::mutex> get_candidate_bulk_lock(
          *thread_state->mutex_);
      if (thread_state->ready_queue_.empty()) {
        get_candidate_bulk_lock.unlock();

        std::unique_lock<std::mutex> wait_for_bulk_lk(
            *thread_state->another_mutex_);
        thread_state->available_cv_->wait(wait_for_bulk_lk, [thread_state] {
          return thread_state->dead_ || !thread_state->ready_queue_.empty();
        });
        continue;
      }
      std::vector<BulkNode *> &ready_queue = thread_state->ready_queue_;
      int bulk_index = rand() % ready_queue.size();
      bulk_to_search = ready_queue[bulk_index];
    }

    // Critical section to check if this bulk has an unclaimed task
    TaskID task_to_execute;
    {
      std::unique_lock<std::mutex> search_bulk_lk(*bulk_to_search->mutex_);
      bool last_task_claimed =
          bulk_to_search->next_task_id_ >= bulk_to_search->num_total_tasks_;
      if (last_task_claimed) {
        continue;
      }
      task_to_execute = bulk_to_search->next_task_id_++;
    }

    // Execute task
    bulk_to_search->runnable_->runTask(task_to_execute,
                                       bulk_to_search->num_total_tasks_);
    bulk_to_search->num_completed_tasks_.fetch_add(1);
    bool bulk_not_done = bulk_to_search->num_completed_tasks_.load() <
                         bulk_to_search->num_total_tasks_;

    if (bulk_not_done) {
      continue;
    }

    // Critical section to update last done id & promote from dependent-> ready
    {
      std::unique_lock<std::mutex> promote_bulks_lk(*thread_state->mutex_);
      thread_state->last_done_bulk_id_ =
          std::max(thread_state->last_done_bulk_id_, bulk_to_search->id_);

      auto it = thread_state->ready_queue_.begin();
      for (; it != thread_state->ready_queue_.end(); ++it) {
        if ((*it)->id_ == bulk_to_search->id_) {
          thread_state->ready_queue_.erase(it);
          break;
        }
      }

      if (!thread_state->ready_queue_.empty()) {
        continue;
      }

      // Promote freed dependent tasks
      auto dit = thread_state->dependent_queue_.begin();
      for (; dit != thread_state->dependent_queue_.end(); ++dit) {
        if (bulk_to_search->bottleneck_id_ > thread_state->last_done_bulk_id_) {
          break;
        }
      }

      if (dit != thread_state->dependent_queue_.begin()) {
        thread_state->ready_queue_.insert(
            thread_state->ready_queue_.end(),
            thread_state->dependent_queue_.begin(), dit);
        thread_state->dependent_queue_.erase(
            thread_state->dependent_queue_.begin(), dit);
        thread_state->available_cv_->notify_all();
      }

      if (thread_state->ready_queue_.empty())
        thread_state->sync_cv_->notify_all();
    }
  }
  thread_state->num_exited_++;
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(
    int num_threads)
    : ITaskSystem(num_threads), next_bulk_id_(0) {
  thread_pool_ = new std::thread[num_threads];
  threads_state_ = new ThreadsState(num_threads);
  for (int i = 0; i < num_threads; ++i) {
    thread_pool_[i] = std::thread(thread_fn, threads_state_);
  }
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
  threads_state_->dead_ = true;
  while (threads_state_->num_exited_ < threads_state_->num_threads_) {
    threads_state_->available_cv_->notify_all();
  }

  for (int i = 0; i < threads_state_->num_threads_; ++i) {
    thread_pool_[i].join();
  }
  delete[] thread_pool_;
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable *runnable,
                                               int num_total_tasks) {
  runAsyncWithDeps(runnable, num_total_tasks, {});
  sync();
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(
    IRunnable *runnable, int num_total_tasks, const std::vector<TaskID> &deps) {
  const TaskID current_id = next_bulk_id_++;
  TaskID bottleneck_id =
      deps.empty() ? -1 : *std::max_element(deps.begin(), deps.end());
  BulkNode *bulk_node =
      new BulkNode(current_id, num_total_tasks, runnable, bottleneck_id);
  {
    std::lock_guard<std::mutex> lock(*threads_state_->mutex_);
    if (bulk_node->bottleneck_id_ < threads_state_->last_done_bulk_id_ ||
        threads_state_->ready_queue_.empty()) {
      threads_state_->ready_queue_.push_back(bulk_node);
    } else {
      threads_state_->dependent_queue_.insert(bulk_node);
    }
  }
  threads_state_->available_cv_->notify_all();
  return current_id;
}

void TaskSystemParallelThreadPoolSleeping::sync() {
  std::unique_lock<std::mutex> lk(*threads_state_->mutex_);
  // TODO: also test && dependent.empty
  threads_state_->sync_cv_->wait(
      lk, [this] { return threads_state_->ready_queue_.empty(); });
  return;
}

#ifndef _TASKSYS_H
#define _TASKSYS_H

#include "itasksys.h"
#include <atomic>
#include <thread>

/*
 * TaskSystemSerial: This class is the student's implementation of a
 * serial task execution engine.  See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemSerial : public ITaskSystem {
public:
  TaskSystemSerial(int num_threads);
  ~TaskSystemSerial();
  const char *name();
  void run(IRunnable *runnable, int num_total_tasks);
  TaskID runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                          const std::vector<TaskID> &deps);
  void sync();
};

/*
 * TaskSystemParallelSpawn: This class is the student's implementation of a
 * parallel task execution engine that spawns threads in every run()
 * call.  See definition of ITaskSystem in itasksys.h for documentation
 * of the ITaskSystem interface.
 */
class TaskSystemParallelSpawn : public ITaskSystem {
public:
  TaskSystemParallelSpawn(int num_threads);
  ~TaskSystemParallelSpawn();
  const char *name();
  void run(IRunnable *runnable, int num_total_tasks);
  TaskID runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                          const std::vector<TaskID> &deps);
  void sync();

  int num_threads;
  std::thread *threads;
};

/*
 * TaskSystemParallelThreadPoolSpinning: This class is the student's
 * implementation of a parallel task execution engine that uses a
 * thread pool. See definition of ITaskSystem in itasksys.h for
 * documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSpinning : public ITaskSystem {
public:
  TaskSystemParallelThreadPoolSpinning(int num_threads);
  ~TaskSystemParallelThreadPoolSpinning();
  const char *name();
  void run(IRunnable *runnable, int num_total_tasks);
  TaskID runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                          const std::vector<TaskID> &deps);
  void sync();

  int num_threads;
  std::vector<std::thread> threads;
  std::atomic<int> next_task;
  std::atomic<int> completed_tasks; // New member
  int num_tasks;
  IRunnable *member_runnable;
  bool dead;
};

/*
 * TaskSystemParallelThreadPoolSleeping: This class is the student's
 * optimized implementation of a parallel task execution engine that uses
 * a thread pool. See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSleeping : public ITaskSystem {
public:
  TaskSystemParallelThreadPoolSleeping(int num_threads);
  ~TaskSystemParallelThreadPoolSleeping();
  const char *name();
  void run(IRunnable *runnable, int num_total_tasks);
  TaskID runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                          const std::vector<TaskID> &deps);
  void sync();

private:
  std::vector<std::thread> worker_threads; // baristas
  std::atomic<int> next_task;
  std::atomic<int> completed_tasks;
  int num_tasks;
  IRunnable *member_runnable;
  bool dead;
  std::mutex just_a_mutex;
  std::condition_variable work_available;
  std::condition_variable work_completed;

  void my_worker_thread() {
    while (true) {
      // Initial wait to check for work
      {
        std::unique_lock<std::mutex> lock(just_a_mutex);
        work_available.wait(
            lock, [this]() { return member_runnable != nullptr || dead; });

        if (dead)
          break;
      }
      //
      while (true) {

        int task_index = next_task.fetch_add(1);
        if (task_index >= num_tasks)
          break;

        member_runnable->runTask(task_index, num_tasks);

        // notify main thread if this is the last task
        // +1 because fetch_add returns the value before incrementing
        if (completed_tasks.fetch_add(1) + 1 == num_tasks) {
          std::unique_lock<std::mutex> lock(just_a_mutex);
          work_completed.notify_one();
          break;
        }
      }
    }
  }
};

#endif

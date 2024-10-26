#ifndef _TASKSYS_H
#define _TASKSYS_H

#include "itasksys.h"
#include <list>
#include <thread>
#include <vector>

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
};

class BulkNode {
public:
  // Invariants
  IRunnable *runnable_;
  TaskID bottleneck_id_;
  const TaskID id_;

  // States
  std::mutex lock_;
  int next_task_id_;
  int num_total_tasks_;
  std::atomic<int> num_completed_tasks_;

  BulkNode(const TaskID id, const int num_total_tasks, IRunnable *runnable,
           const TaskID bottleneck_id)
      : id_(id), next_task_id_(0), num_total_tasks_(num_total_tasks),
        num_completed_tasks_(0), runnable_(runnable),
        bottleneck_id_(bottleneck_id) {}
};

class ThreadsState {
public:
  const int num_threads_;
  bool done_;
  TaskID last_done_id_;

  std::mutex *mutex_;
  std::mutex *another_mutex_;
  std::condition_variable *available_cv_;
  std::condition_variable *sync_cv_;
  std::atomic<int> num_exited_;

  std::list<BulkNode *> dependent_queue_;
  std::vector<BulkNode *> ready_queue_;

  ThreadsState(int num_threads)
      : num_threads_(num_threads), done_(false), last_done_id_(0),
        num_exited_(0) {
    mutex_ = new std::mutex();
    another_mutex_ = new std::mutex();
  }
  ~ThreadsState() {
    delete mutex_;
    delete another_mutex_;
    delete available_cv_;
    delete sync_cv_;
  }
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

  std::atomic<TaskID> next_bulk_id_;
  std::thread *thread_pool;
  ThreadsState *threads_state;
};

#endif

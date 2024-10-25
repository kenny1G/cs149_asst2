#ifndef _TASKSYS_H
#define _TASKSYS_H

#include "itasksys.h"
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>

/*
 * TaskSystemSerial: This class is the student's implementation of a
 * serial task execution engine.  See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemSerial : public ITaskSystem
{
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
class TaskSystemParallelSpawn : public ITaskSystem
{
public:
  TaskSystemParallelSpawn(int num_threads);
  ~TaskSystemParallelSpawn();
  const char *name();
  void run(IRunnable *runnable, int num_total_tasks);
  TaskID runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                          const std::vector<TaskID> &deps);
  void sync();
};

class ThreadState
{
public:
  std::atomic<int> evens_next_task;
  std::atomic<int> odds_next_task;
  std::atomic<int> num_completed_tasks;
  std::mutex *mutex;
  bool dead;
  int num_total_tasks;
  IRunnable *runnable;
  // int num_threads_;
  ThreadState(int num_threads)
  {
    mutex = new std::mutex();
    evens_next_task = 0;
    odds_next_task = 0;
    num_completed_tasks = 0;
    // num_threads_ = num_threads;
    runnable = nullptr;
    num_total_tasks = 0;
    dead = false;
  }
  ~ThreadState()
  {
    delete mutex;
  }
};

class SleepThreadState : public ThreadState
{
public:
  std::mutex *done_mutex;
  std::condition_variable *available_cv;
  std::condition_variable *done_cv;
  int counter_;
  SleepThreadState(int num_threads) : ThreadState(num_threads)
  {
    available_cv = new std::condition_variable();
    done_cv = new std::condition_variable();
    done_mutex = new std::mutex();
    counter_ = 0;
  }
  ~SleepThreadState()
  {
    delete available_cv;
    delete done_cv;
    delete done_mutex;
  }
};
/*
 * TaskSystemParallelThreadPoolSpinning: This class is the student's
 * implementation of a parallel task execution engine that uses a
 * thread pool. See definition of ITaskSystem in itasksys.h for
 * documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSpinning : public ITaskSystem
{
public:
  TaskSystemParallelThreadPoolSpinning(int num_threads);
  ~TaskSystemParallelThreadPoolSpinning();
  const char *name();
  void run(IRunnable *runnable, int num_total_tasks);
  TaskID runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                          const std::vector<TaskID> &deps);
  void sync();

private:
  std::thread *thread_pool;
  ThreadState *thread_state;
};

/*
 * TaskSystemParallelThreadPoolSleeping: This class is the student's
 * optimized implementation of a parallel task execution engine that uses
 * a thread pool. See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSleeping : public ITaskSystem
{
public:
  TaskSystemParallelThreadPoolSleeping(int num_threads);
  ~TaskSystemParallelThreadPoolSleeping();
  const char *name();
  void run(IRunnable *runnable, int num_total_tasks);
  TaskID runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                          const std::vector<TaskID> &deps);
  void sync();

private:
  std::thread *thread_pool;
  SleepThreadState *sleep_thread_state;
};

#endif

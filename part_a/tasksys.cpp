#include "tasksys.h"
#include <iostream>
#include <thread>
#include <vector>

IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int num_threads) { _num_threads = num_threads; }
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

void TaskSystemSerial::run(IRunnable *runnable, int num_total_tasks)
{
  for (int i = 0; i < num_total_tasks; i++)
  {
    runnable->runTask(i, num_total_tasks);
  }
}

TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable *runnable,
                                          int num_total_tasks,
                                          const std::vector<TaskID> &deps)
{
  // You do not need to implement this method.
  return 0;
}

void TaskSystemSerial::sync()
{
  // You do not need to implement this method.
  return;
}

/*
 * ================================================================
 * Parallel Task System Implementation
 * ================================================================
 */

const char *TaskSystemParallelSpawn::name()
{
  return "Parallel + Always Spawn";
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads)
    : ITaskSystem(num_threads)
{
  //
  // TODO: CS149 student implementations may decide to perform setup
  // operations (such as thread pool construction) here.
  // Implementations are free to add new class member variables
  // (requiring changes to tasksys.h).
  //
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable *runnable, int num_total_tasks)
{

  //
  // TODO: CS149 students will modify the implementation of this
  // method in Part A.  The implementation provided below runs all
  // tasks sequentially on the calling thread.
  //

  std::vector<std::thread> threads = {};
  for (int thread_idx = 0; thread_idx < _num_threads; ++thread_idx)
  {
    threads.push_back(std::thread([&runnable, num_total_tasks, thread_idx, this]
                                  {
      for (int j = thread_idx; j < num_total_tasks; j+=_num_threads) {
        runnable->runTask(j, num_total_tasks);
      } }));
  }
  for (std::thread &t : threads)
  {
    t.join();
  }
  return;
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(
    IRunnable *runnable, int num_total_tasks, const std::vector<TaskID> &deps)
{
  // You do not need to implement this method.
  return 0;
}

void TaskSystemParallelSpawn::sync()
{
  // You do not need to implement this method.
  return;
}

/*
 * ================================================================
 * Parallel Thread Pool Spinning Task System Implementation
 * ================================================================
 */

const char *TaskSystemParallelThreadPoolSpinning::name()
{
  return "Parallel + Thread Pool + Spin";
}

std::mutex cout_mutex;
std::vector<std::string> messages;
void print_message(int thread_idx, const std::string &message)
{
  std::lock_guard<std::mutex> lock(cout_mutex);
  messages.push_back(std::string("Thread ") + std::to_string(thread_idx) + std::string(":") + message);
}

void thread_fn(ThreadState *thread_state, int thread_idx)
{
  while (true)
  {
    if (thread_state->dead)
      return;
    int last_index;
    {
      std::lock_guard<std::mutex> lock(*thread_state->mutex);
      // busy wait
      if (thread_state->runnable == nullptr)
      {
        continue;
      }
    }
    std::atomic<int> *next_task = thread_idx % 2 == 0 ? &thread_state->evens_next_task : &thread_state->odds_next_task;
    // std::atomic<int> &next_task = thread_idx % 2 == 0 ? thread_state->evens_next_task : thread_state->odds_next_task;
    last_index = next_task->fetch_add(2);
    // last_index = thread_state->evens_next_task.fetch_add(1);
    // busy_wait
    if (last_index >= thread_state->num_total_tasks)
    {
      // std::atomic<int> &next_task = thread_idx % 2 == 0 ? thread_state->evens_next_task : thread_state->odds_next_task;
      // next_task.fetch_sub(2);
      continue;
    }
    // print_message(thread_idx, std::string("Running task ") + std::to_string(last_index));
    thread_state->runnable->runTask(last_index, thread_state->num_total_tasks);
    thread_state->num_completed_tasks.fetch_add(1);
    // local_num_completed++;
  }
  return;
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(
    int num_threads)
    : ITaskSystem(num_threads)
{
  thread_pool = new std::thread[num_threads];
  thread_state = new ThreadState(num_threads);
  for (int i = 0; i < num_threads; i++)
  {
    thread_pool[i] = std::thread(thread_fn, thread_state, i);
  }
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning()
{
  thread_state->dead = true;
  for (int i = 0; i < _num_threads; i++)
  {
    thread_pool[i].join();
  }
  delete[] thread_pool;
  delete thread_state;
}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable *runnable,
                                               int num_total_tasks)
{
  // print_message(-1, std::string("Starting run of ") + std::to_string(num_total_tasks) + std::string(" tasks"));
  // auto start_time = std::chrono::high_resolution_clock::now();
  {
    std::lock_guard<std::mutex> lock(*thread_state->mutex);
    thread_state->num_total_tasks = num_total_tasks;
    thread_state->runnable = runnable;
    thread_state->num_completed_tasks = 0;
    thread_state->evens_next_task = 0;
    thread_state->odds_next_task = 1;
  }

  while (thread_state->num_completed_tasks < num_total_tasks)
  {
    // printf("%d Tasks Completed\n", thread_state->num_completed_tasks.load());
    // busy wait
  }
  {
    std::lock_guard<std::mutex> lock(*thread_state->mutex);
    thread_state->runnable = nullptr;
  }
  // print_message(-1, std::string("Run complete"));
  // for (std::string &message : messages)
  // {
  //   std::cout << message << std::endl;
  // }
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(
    IRunnable *runnable, int num_total_tasks, const std::vector<TaskID> &deps)
{
  // You do not need to implement this method.
  return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync()
{
  // You do not need to implement this method.
  return;
}

/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

const char *TaskSystemParallelThreadPoolSleeping::name()
{
  return "Parallel + Thread Pool + Sleep";
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(
    int num_threads)
    : ITaskSystem(num_threads)
{
  //
  // TODO: CS149 student implementations may decide to perform setup
  // operations (such as thread pool construction) here.
  // Implementations are free to add new class member variables
  // (requiring changes to tasksys.h).
  //
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping()
{
  //
  // TODO: CS149 student implementations may decide to perform cleanup
  // operations (such as thread pool shutdown construction) here.
  // Implementations are free to add new class member variables
  // (requiring changes to tasksys.h).
  //
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable *runnable,
                                               int num_total_tasks)
{

  //
  // TODO: CS149 students will modify the implementation of this
  // method in Parts A and B.  The implementation provided below runs all
  // tasks sequentially on the calling thread.
  //

  for (int i = 0; i < num_total_tasks; i++)
  {
    runnable->runTask(i, num_total_tasks);
  }
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(
    IRunnable *runnable, int num_total_tasks, const std::vector<TaskID> &deps)
{

  //
  // TODO: CS149 students will implement this method in Part B.
  //

  return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync()
{

  //
  // TODO: CS149 students will modify the implementation of this method in Part
  // B.
  //

  return;
}

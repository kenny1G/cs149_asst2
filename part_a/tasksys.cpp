#include "tasksys.h"
#include <thread>
#include <atomic>
#include <iostream>

IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int num_threads) {}
ITaskSystem::~ITaskSystem() {}

/*
 * ================================================================
 * Serial task system implementation
 * ================================================================
 */

const char *TaskSystemSerial::name()
{
  return "Serial";
}

TaskSystemSerial::TaskSystemSerial(int num_threads) : ITaskSystem(num_threads)
{
}

TaskSystemSerial::~TaskSystemSerial() {}

void TaskSystemSerial::run(IRunnable *runnable, int num_total_tasks)
{
  for (int i = 0; i < num_total_tasks; i++)
  {
    runnable->runTask(i, num_total_tasks);
  }
}

TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
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

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads) : ITaskSystem(num_threads)
{
  //
  // TODO: CS149 student implementations may decide to perform setup
  // operations (such as thread pool construction) here.
  // Implementations are free to add new class member variables
  // (requiring changes to tasksys.h).
  //

  // set up threads
  this->num_threads = num_threads;
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable *runnable, int num_total_tasks)
{

  //
  // TODO: CS149 students will modify the implementation of this
  // method in Part A.  The implementation provided below runs all
  // tasks sequentially on the calling thread.
  //

  // serial code:
  // for (int i = 0; i < num_total_tasks; i++) {
  //     runnable->runTask(i, num_total_tasks);
  // }

  // parallel code:
  std::thread *threads = new std::thread[num_threads];
  for (int i = 0; i < num_threads; i++)
  {
    threads[i] = std::thread([runnable, num_total_tasks, i, this]()
                             {
            for (int j = i; j < num_total_tasks; j += num_threads) {
                runnable->runTask(j, num_total_tasks);
            } });
  }

  // wait for threads to finish
  for (int i = 0; i < num_threads; i++)
  {
    threads[i].join();
  }

  // clean up
  delete[] threads;
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                                                 const std::vector<TaskID> &deps)
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

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads) : ITaskSystem(num_threads)
{
  //
  // TODO: CS149 student implementations may decide to perform setup
  // operations (such as thread pool construction) here.
  // Implementations are free to add new class member variables
  // (requiring changes to tasksys.h).
  //

  this->num_threads = num_threads;
  this->threads = std::vector<std::thread>(num_threads);
  this->next_task = 0;
  this->completed_tasks = 0; // Initialize the new member
  this->num_tasks = 0;
  this->member_runnable = nullptr;
  this->dead = false;
  // create threads that will spin and check for tasks
  for (int i = 0; i < num_threads; i++)
  {
    threads[i] = std::thread([this]()
                             {
            while (true) {
              if (dead) {
                break;
              }
                if (member_runnable == nullptr)
                {
                  continue;
                }

                // Check if the task index is within bounds
                int task_index = next_task.fetch_add(1); // Increment and get the current value
                if (task_index >= num_tasks) {
                  continue;
                }
                member_runnable->runTask(task_index, num_tasks);
                completed_tasks.fetch_add(1);

            } });
  }
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning()
{
  dead = true;
  for (int i = 0; i < num_threads; i++)
  {
    threads[i].join();
  }
}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable *runnable, int num_total_tasks)
{
  member_runnable = runnable;
  next_task = 0;
  completed_tasks = 0; // Reset the completed tasks counter
  num_tasks = num_total_tasks;

  // printf("Run called with %d tasks\n", num_total_tasks);
  // Wait until all tasks are completed
  while (completed_tasks.load() != num_total_tasks)
  { }
  member_runnable = nullptr;
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                                                              const std::vector<TaskID> &deps)
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

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads) : ITaskSystem(num_threads)
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

void TaskSystemParallelThreadPoolSleeping::run(IRunnable *runnable, int num_total_tasks)
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

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                                                              const std::vector<TaskID> &deps)
{

  //
  // TODO: CS149 students will implement this method in Part B.
  //

  return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync()
{

  //
  // TODO: CS149 students will modify the implementation of this method in Part B.
  //

  return;
}

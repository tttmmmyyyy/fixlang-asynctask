#include <pthread.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef int *TaskFunction;
typedef int *TaskResult;
typedef struct
{
    TaskFunction function;
    TaskResult result;
    void (*release_result)(void *);
    void (*retain_result)(void *);
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    uint8_t refcnt;
} Task;

// Interface functions.
void fixasynctask_thread_prepare_termination();
void fixasynctask_thread_terminate();
Task *fixasynctask_thread_create_task(TaskFunction function, void (*release_result)(void *), void (*retain_result)(void *));
TaskResult fixasynctask_thread_get_task_result(Task *task);
void fixasynctask_thread_release_task(Task *task);

// Internal functions.
void fixasynctask_thread_destroy_task(Task *task);
void fixasynctask_thread_execute_task(Task *task);
void *fixasynctask_thread_execute_task_void(void *task);

// Utility functions.
void pthread_mutex_lock_or_exit(pthread_mutex_t *mutex, const char *msg)
{
    if (pthread_mutex_lock(mutex))
    {
        perror(msg);
        exit(1);
    }
}
void pthread_mutex_unlock_or_exit(pthread_mutex_t *mutex, const char *msg)
{
    if (pthread_mutex_unlock(mutex))
    {
        perror(msg);
        exit(1);
    }
}
void pthread_cond_wait_or_exit(pthread_cond_t *cond, pthread_mutex_t *mutex, const char *msg)
{
    if (pthread_cond_wait(cond, mutex))
    {
        perror(msg);
        exit(1);
    }
}
// void pthread_cond_signal_or_exit(pthread_cond_t *cond, const char *msg)
// {
//     if (pthread_cond_signal(cond))
//     {
//         perror(msg);
//         exit(1);
//     }
// }

void pthread_cond_broadcast_or_exit(pthread_cond_t *cond, const char *msg)
{
    if (pthread_cond_broadcast(cond))
    {
        perror(msg);
        exit(1);
    }
}

void pthread_cond_signal_or_exit(pthread_cond_t *cond, const char *msg)
{
    if (pthread_cond_signal(cond))
    {
        perror(msg);
        exit(1);
    }
}

#ifdef TERMINATE_TASKS
// Variables to wait all tasks to be completed.
uint64_t task_count;
pthread_mutex_t task_count_mutex;
pthread_cond_t task_count_cond;

// Initialize variables `fixasynctask_thread_terminate`
void fixasynctask_thread_prepare_termination()
{
    // Initialize mutex and condition variable for tasks that runs even after released on a dedicated thread.
    task_count = 0;
    if (pthread_mutex_init(&task_count_mutex, NULL))
    {
        perror("[runtime] Failed to initialize mutex `task_count_mutex`.");
        exit(1);
    }
    if (pthread_cond_init(&task_count_cond, NULL))
    {
        perror("[runtime] Failed to initialize condvar `task_count_cond`.");
        exit(1);
    }
}

// Wait for all tasks to be completed.
// This function is used only for compiler development (leak detector).
void fixasynctask_thread_terminate()
{
    // Wait for all tasks that runs even after released on a dedicated thread to be completed.
    pthread_mutex_lock_or_exit(&task_count_mutex, "[runtime] Failed to lock mutex `task_count_mutex`.");
    while (task_count > 0)
    {
        pthread_cond_wait_or_exit(
            &task_count_cond,
            &task_count_mutex, "[runtime] Failed to wait condvar `task_count_cond`.");
    }
    pthread_mutex_unlock_or_exit(&task_count_mutex, "[runtime] Failed to unlock mutex `task_count_mutex`.");

    // Destroy mutex and condition variable.
    if (pthread_mutex_destroy(&task_count_mutex))
    {
        perror("[runtime] Failed to destroy mutex `task_count_mutex`.");
        exit(1);
    }
    if (pthread_cond_destroy(&task_count_cond))
    {
        perror("[runtime] Failed to destroy condvar `task_count_cond`.");
        exit(1);
    }
}

#endif // TERMINATE_TASKS

// Create an asynchronous task.
Task *fixasynctask_thread_create_task(TaskFunction function, void (*release_result)(void *), void (*retain_result)(void *))
{
    Task *task = (Task *)malloc(sizeof(Task));
    task->function = function;
    task->result = NULL;
    task->release_result = release_result;
    task->retain_result = retain_result;
    task->refcnt = 2; // One ownership for this library, and one for the user.
    if (pthread_mutex_init(&task->mutex, NULL))
    {
        perror("[runtime] Failed to initialize mutex for a task.");
        exit(1);
    }
    if (pthread_cond_init(&task->cond, NULL))
    {
        perror("[runtime] Failed to initialize condition variable for a task.");
        exit(1);
    }

    // Run the task on a thread.
    pthread_t thread;
    if (pthread_create(&thread, NULL, fixasynctask_thread_execute_task_void, task))
    {
        perror("[runtime] Failed to create thread to run a task.");
        exit(1);
    }
    if (pthread_detach(thread))
    {
        perror("[runtime] Failed to detach thread to run a task.");
        exit(1);
    }
#ifdef TERMINATE_TASKS
    // If the task should be terminated, then increment the counter for such tasks.
    pthread_mutex_lock_or_exit(&task_count_mutex, "[runtime] Failed to lock mutex `task_count_mutex`.");
    task_count++;
    pthread_mutex_unlock_or_exit(&task_count_mutex, "[runtime] Failed to unlock mutex `task_count_mutex`.");
#endif // TERMINATE_TASKS
    return task;
}

// Get the task result.
TaskResult fixasynctask_thread_get_task_result(Task *task)
{
    pthread_mutex_lock_or_exit(&task->mutex, "[runtime] Failed to lock mutex for a task.");
    while (!task->result)
    {
        pthread_cond_wait_or_exit(&task->cond, &task->mutex, "[runtime] Failed to wait condvar for a task.");
    }
    TaskResult result = task->result;
    task->retain_result(result);
    pthread_mutex_unlock_or_exit(&task->mutex, "[runtime] Failed to unlock mutex for a task.");
    return result;
}

// Release a task.
void fixasynctask_thread_release_task(Task *task)
{
    pthread_mutex_lock_or_exit(&task->mutex, "[runtime] Failed to lock mutex for a task.");
    uint8_t refcnt = --task->refcnt;
    pthread_mutex_unlock_or_exit(&task->mutex, "[runtime] Failed to unlock mutex for a task.");
    if (refcnt == 0)
    {
        fixasynctask_thread_destroy_task(task);
    }
}

// A C function exported in "asynctask.fix".
// This function takes a pointer to the value `Std::Boxed (() -> Ptr)` and evaluate the pointer.
void *fixasynctask_run_task_function(void *function);

// Run a task on this thread.
void fixasynctask_thread_execute_task(Task *task)
{
    TaskResult result = fixasynctask_run_task_function(task->function);

    pthread_mutex_lock_or_exit(&task->mutex, "[runtime] Failed to lock mutex for a task.");
    task->result = result;
    pthread_cond_broadcast_or_exit(&task->cond, "[runtime] Failed to signal condvar for a task.");
    uint8_t refcnt = --task->refcnt;
    pthread_mutex_unlock_or_exit(&task->mutex, "[runtime] Failed to unlock mutex.");
    if (refcnt == 0)
    {
        fixasynctask_thread_destroy_task(task);
    }
}

void *fixasynctask_thread_execute_task_void(void *task)
{
    fixasynctask_thread_execute_task((Task *)task);
    return NULL;
}

// Free the task object.
void fixasynctask_thread_destroy_task(Task *task)
{
    (*task->release_result)(task->result);
    if (pthread_mutex_destroy(&task->mutex))
    {
        perror("[runtime] Failed to destroy mutex for a task.");
        exit(1);
    }
    if (pthread_cond_destroy(&task->cond))
    {
        perror("[runtime] Failed to destroy condition variable for a task.");
        exit(1);
    }

#ifdef TERMINATE_TASKS
    // If the task should run even after released on a dedicated thread, then decrement the counter for such tasks.
    pthread_mutex_lock_or_exit(&task_count_mutex, "[runtime] Failed to lock mutex `task_count_mutex`.");
    task_count--;
    pthread_cond_signal_or_exit(&task_count_cond, "[runtime] Failed to signal condvar `task_count_cond`.");
    pthread_mutex_unlock_or_exit(&task_count_mutex, "[runtime] Failed to unlock mutex `task_count_mutex`.");
#endif // TERMINATE_TASKS

    free(task);
}

typedef struct Var
{
    void *data;
    void (*release_func)(void *);
    void (*retain_func)(void *);
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} Var;

Var *fixasynctask_thread_var_create(void *data, void (*release_func)(void *), void (*retain_func)(void *))
{
    struct Var *handle = (struct Var *)malloc(sizeof(struct Var));

    // Create recursive mutex.
    pthread_mutexattr_t Attr;
    pthread_mutexattr_init(&Attr);
    pthread_mutexattr_settype(&Attr, PTHREAD_MUTEX_RECURSIVE);
    if (pthread_mutex_init(&handle->mutex, &Attr))
    {
        perror("[runtime] Failed to initialize mutex for a Var.");
        exit(1);
    }
    pthread_mutexattr_destroy(&Attr);

    // Create condition variable.
    if (pthread_cond_init(&handle->cond, NULL))
    {
        perror("[runtime] Failed to initialize condition variable for a Var.");
        exit(1);
    }

    // Set fields.
    handle->data = data;
    handle->release_func = release_func;
    handle->retain_func = retain_func;

    return handle;
}

void fixasynctask_thread_var_destroy(Var *handle)
{
    (*handle->release_func)(handle->data);
    if (pthread_mutex_destroy(&handle->mutex))
    {
        perror("[runtime] Failed to destroy mutex for a Var.");
        exit(1);
    }
    if (pthread_cond_destroy(&handle->cond))
    {
        perror("[runtime] Failed to destroy condition variable for a Var.");
        exit(1);
    }
    free(handle);
}

void fixasynctask_thread_var_lock(Var *handle)
{
    pthread_mutex_lock_or_exit(&handle->mutex, ("[runtime] Failed to lock mutex for a Var."));
}

void fixasynctask_thread_var_unlock(Var *handle)
{
    pthread_mutex_unlock_or_exit(&handle->mutex, ("[runtime] Failed to unlock mutex for a Var."));
}

void fixasynctask_thread_var_wait(Var *handle)
{
    pthread_cond_wait_or_exit(&handle->cond, &handle->mutex, "[runtime] Failed to wait condition variable for a Var.");
}

void fixasynctask_thread_var_signalall(Var *handle)
{
    pthread_cond_broadcast_or_exit(&handle->cond, "[runtime] Failed to signal condition variable for a Var.");
}

void *fixasynctask_thread_var_get(Var *handle)
{
    void *data = handle->data;
    (*handle->retain_func)(data);
    return data;
}

void fixasynctask_thread_var_set(Var *handle, void *data)
{
    (*handle->release_func)(handle->data);
    handle->data = data;
}

int64_t fixasynctask_get_number_of_processors()
{
    return (int64_t)sysconf(_SC_NPROCESSORS_ONLN);
}
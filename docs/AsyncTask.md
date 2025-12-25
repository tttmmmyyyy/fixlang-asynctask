# AsyncTask

Defined in asynctask@1.1.3

This module provides a way to run tasks asynchronously.

Importing this module automatically enables `--threaded` flag of the compiler.
Note that this causes some overhead even for a single-threaded program.

## Values

### namespace AsyncTask

#### get

Type: `AsyncTask::Task a -> a`

Gets the result of a task.

This function blocks the current thread until the task is finished.

#### make

Type: `(() -> a) -> AsyncTask::Task a`

Makes a task which performs a computation asynchronously.

Example:
```
module Main;
import AsyncTask;

main : IO ();
main = (
    let sum_range = |from, to| (
        loop((0, from), |(sum, i)| (
            if i == to { break $ sum };
            continue $ (sum + i, i + 1)
        ))
    );
    let n = 1000000000;
    // Compute the sum of numbers from 0 to n/2 - 1.
    // This task will be executed asynchronously (if you are using multi-core CPU).
    let sum_former = AsyncTask::make(|_| sum_range(0, n/2));
    // Compute the sum of numbers from n/2 to n.
    // We perfom this in the current thread while waiting for the result of the former task.
    let sum_latter = sum_range(n/2, n);
    // Sum up the results of the two computations.
    let sum = sum_former.get + sum_latter;
    // Then the sum should be n * (n - 1) / 2.
    assert_eq(|_|"", sum, n * (n - 1) / 2);;
    println $
        "Sum of numbers from 0 to " + (n - 1).to_string +
        " is " + sum_former.get.to_string + " + " + sum_latter.to_string +
        " = " + sum.to_string + "."
);
```

#### number_of_processors

Type: `Std::I64`

Gets the number of processors (CPU cores) currently available.
This is implemented by calling `sysconf(_SC_NPROCESSORS_ONLN)`.
The runtime pools as many threads as this number to execute asynchronous tasks.

### namespace AsyncTask::AsyncIOTask

#### get

Type: `AsyncTask::AsyncIOTask::IOTask a -> Std::IO a`

Get the result of an asynchronous I/O action.

#### make

Type: `Std::IO a -> Std::IO (AsyncTask::AsyncIOTask::IOTask a)`

An `IO` version of `AsyncTask::make`.

Example:
```
module Main;
import AsyncTask;

main : IO ();
main = (
    let print_ten : I64 -> IO () = |task_num| (
        loop_m(0, |i| (
            if i == 10 {
                break_m $ ()
            } else {
                let msg = "task number: " + task_num.to_string + ", i: " + i.to_string;
                msg.println;;
                continue_m $ i + 1
            }
        ))
    );
    (*AsyncIOTask::make(print_ten(0))).get;;
    (*AsyncIOTask::make(print_ten(1))).get;;
    pure()
);
```

### namespace AsyncTask::Var

#### get

Type: `AsyncTask::Var::Var a -> Std::IO a`

Get a value stored in a `Var`.

#### lock

Type: `(a -> Std::IO b) -> AsyncTask::Var::Var a -> Std::IO b`

`var.lock(act)` performs an action on the value in `var` while locking `var` to prevent it from being changed by another thread.

#### make

Type: `a -> Std::IO (AsyncTask::Var::Var a)`

Create a new `Var` object.

#### mod

Type: `(a -> a) -> AsyncTask::Var::Var a -> Std::IO ()`

Atomically modifies a value in a `Var`.

#### set

Type: `a -> AsyncTask::Var::Var a -> Std::IO ()`

Set a value to a `Var`.

#### wait

Type: `(a -> Std::Bool) -> AsyncTask::Var::Var a -> Std::IO ()`

`var.wait(cond)` waits until `cond` on the value of `var` is satisfied.

Note that it is not assured that `cond` is satisfied after `wait` returned;
the value in `var` may be changed after `cond` is evaluated.

#### wait_and_lock

Type: `(a -> Std::Bool) -> (a -> Std::IO b) -> AsyncTask::Var::Var a -> Std::IO b`

`var.wait_and_lock(cond, act)` waits until `cond` on the value of `var` is satisfied,
then performs `act` on the value in `var` while locking `var` to prevent it from being changed by another thread.

## Types and aliases

### namespace AsyncTask

#### Task

Defined as: `type Task a = unbox struct { ...fields... }`

A type for a computation task that runs asynchronously.

##### field `dtor`

Type: `Std::FFI::Destructor AsyncTask::TaskHandle`

#### TaskHandle

Defined as: `type TaskHandle = Std::Ptr`

A native handle of a task. This type is used only for implementation.

### namespace AsyncTask::AsyncIOTask

#### IOTask

Defined as: `type IOTask a = unbox struct { ...fields... }`

A type for an I/O action that can be run asynchronously.

##### field `_task`

Type: `AsyncTask::Task a`

### namespace AsyncTask::Var

#### Var

Defined as: `type Var a = unbox struct { ...fields... }`

A type of variable which can be modified from multiple threads.

```
module Main;
import AsyncTask;

main : IO ();
main = (
    let logger = *Var::make([]); // A mutable array of strings.

    // Launch multiple threads, and log in which order each thread is executed.
    let num_threads = number_of_processors * 2;
    Iterator::range(0, num_threads).fold_m((), |i, _| (
        AsyncIOTask::make(
            logger.lock(|logs| (
                let count = logs.get_size;
                let msg = "Thread " + i.to_string + " is running at " + count.to_string +
                    if count % 10 == 1 { "st" } else if count % 10 == 2 { "nd" } else if count % 10 == 3 { "rd" } else { "th" };
                let msg = msg + if i == count { "." } else { "!" };
                logger.set(logs.push_back(msg))
            ))
        )
    ));;

    // Wait until all threads are finished.
    +logger.wait(|logs| logs.get_size == num_threads);

    println $ (*logger.get).to_iter.join("\n")
);
```

##### field `_dtor`

Type: `Std::FFI::Destructor AsyncTask::Var::VarHandle`

#### VarHandle

Defined as: `type VarHandle = Std::Ptr`

#### VarValue

Defined as: `type VarValue a = box struct { ...fields... }`

##### field `value`

Type: `a`

## Traits and aliases

## Trait implementations
# AsyncTask

Defined in asynctask@2.0.1

This module provides a way to run tasks asynchronously.

Build a program that uses this module with multi-threading enabled, by `threaded = true` in its
project file or by the `--threaded` compiler option. Note that this causes some overhead even for
a single-threaded program.

## Values

### namespace AsyncTask

#### get

Type: `AsyncTask::Task a -> a`

Gets the result of a task.

This function blocks the current thread until the task is finished.

#### make

Type: `(() -> a) -> AsyncTask::Task a`

Makes a task which performs a computation asynchronously.

##### Examples

```fix
let sum_range = |from, to| range(from, to).sum;
let n = 1000000;
// This task computes the sum of the former half on another thread.
let sum_former = AsyncTask::make(|_| sum_range(0, n / 2));
// The current thread computes the sum of the latter half meanwhile.
let sum_latter = sum_range(n / 2, n);
// `get` waits for the task to finish.
assert_eq(|_|"", sum_former.get + sum_latter, n * (n - 1) / 2)
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

##### Examples

```fix
let var = *Var::make(0);
let task = *AsyncIOTask::make(var.Var::set(42);; pure $ "done");
// `get` waits for the action to finish, and returns its result.
assert_eq(|_|"", *task.get, "done");;
assert_eq(|_|"", *var.get, 42)
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

### namespace AsyncTask::Var

#### Var

Defined as: `type Var a = unbox struct { ...fields... }`

A type of variable which can be modified from multiple threads.

##### Examples

```fix
let counter = *Var::make(0);
let num_tasks = 10;
// Each task increments the counter atomically.
range(0, num_tasks).fold_m((), |_, _| (
    AsyncIOTask::make(counter.mod(|n| n + 1)).forget
));;
// Wait until all tasks have incremented the counter.
counter.wait(|n| n == num_tasks);;
assert_eq(|_|"", *counter.get, num_tasks)
```

#### VarHandle

Defined as: `type VarHandle = Std::Ptr`

#### VarValue

Defined as: `type VarValue a = box struct { ...fields... }`

##### field `value`

Type: `a`

## Traits and aliases

## Trait implementations
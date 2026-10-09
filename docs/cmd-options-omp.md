# Parallel Evaluation Options

These options control how device evaluation is distributed among OpenMP threads. They take effect only when the simulator runs with more than one thread (see `--ncpu` in [Command Line Options and Startup Sequence](startup-options.md#parallelism)). With a single thread they are ignored.

Before each parallel evaluation the device instances are split into batches with the same number of instances. A batch can span several models and devices. The batches are handed out to threads according to the OpenMP schedule set by `omp_evsched` and `omp_evchunk`. Afterwards the results are loaded into the Jacobian and the residual on one thread. Instances of different models and devices take different amounts of time to evaluate, so batches of equal size are not equally expensive. More batches and a dynamic schedule balance the load better, but they add scheduling overhead.

## Batching

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `omp_evbatches` | int | 10 | >=1 | Number of batches per thread. The instances are split into `ncpu*omp_evbatches` batches. If this exceeds the number of instances, the batches per thread are reduced to `instances/ncpu` (at least 1). With fewer instances than threads some threads stay idle. |

## Scheduling

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `omp_evsched` | string | `"guided"` | `"static"`, `"dynamic"`, `"guided"` | OpenMP schedule used to assign batches to threads. `static` assigns batches to threads in advance and has the lowest overhead, but cannot compensate for batches with different evaluation times. `dynamic` hands out a new chunk of batches to a thread whenever it becomes idle. `guided` is like `dynamic`, but starts with large chunks and decreases their size as the work runs out. |
| `omp_evchunk` | int | 1 | >=1 | Chunk size, in batches, passed to the OpenMP schedule. For `static` and `dynamic` it is the number of batches in one chunk. For `guided` it is the smallest chunk size. |

## Tuning

The defaults suit most circuits. When you tune these options, measure the evaluation time with `print stats` for each setting. Use `static` when all instances take about the same time to evaluate, for instance in a circuit built from a single transistor model. Increase `omp_evbatches` when the threads finish at noticeably different times. Decrease it when the circuit is small and the scheduling overhead dominates.

## Example

```text
control
  options omp_evbatches=20 omp_evsched="dynamic" omp_evchunk=2
  analysis tran1 tran step=1n stop=1u
endc
```

Run it on 4 threads with

```text
vacask -n 4 circuit.sim
```

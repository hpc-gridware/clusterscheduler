# R Integration for GCS / OCS using clustermq

This package provides example scripts and installation support for running parallel R workloads under control of:

- **Open Cluster Scheduler (OCS)**
- **Gridware Cluster Scheduler (GCS)**

It uses the R package **clustermq** to submit and manage R worker jobs through the scheduler (`qsub`).

---

## Overview

The provided files demonstrate how to:

- Install `clustermq` into a user R library
- Configure `clustermq` to use OCS or GCS
- Submit R functions as distributed cluster jobs
- Collect results on the master process
- Execute a visually demonstrative parallel workload (Mandelbrot example)

In GCS environments, enterprise binding ensures that each assigned slot can be bound to a dedicated CPU core, improving runtime predictability for CPU-bound workloads.

---

## Requirements

The following components are required to run R workloads on OCS/GCS clusters using `clustermq`:

- R (≥ 4.x recommended)
- RStudio, Positron, or another R-compatible IDE (optional)
- The host where the R environment is started must be configured as a submit host of the OCS/GCS cluster
- The executing user must have permission to submit and delete jobs via `qsub` and `qdel`
- The `clustermq` package and its dependencies must be available on all execution nodes
- The R version must match between head nodes and execution nodes

R evaluates the file `~/.Renviron` at startup. You can configure environment variables there:

```bash
PATH=<SGE_ROOT>/bin/lx-amd64:${PATH}
R_LIBS_USER=~/R/%p-library/%v
```

Replace `<SGE_ROOT>` with the actual installation path of your scheduler.

Add the scheduler’s architecture-specific `bin` directory (e.g. `bin/lx-amd64`) to your PATH.

Next, `clustermq` must be installed. This can be done either:

- System-wide on all execution nodes and R hosts
- Or in a user library (recommended for testing and development)

If using a user library, ensure:

- The library path is accessible from all execution nodes
- The path is available in the R environment on all execution nodes

Verify scheduler availability within R:

```R
Sys.which("qsub")
```

---

## Installation of clustermq

The installation process will:

- Create a user R library (if needed)
- Install required packages
- Configure R for scheduler usage

`clustermq` is available on GitHub:

https://github.com/mschubert/clustermq

Follow the installation instructions provided there for your specific environment.

This directory contains an installation script that can install `clustermq` into a user library and configure R accordingly:

```R
Rscript install_clustermq.r
```

The script:

- Checks your configured user library path
- Creates it if necessary
- Installs the `remotes` package
- Clones and installs `clustermq` and its dependencies

Compilation of some packages may be required. Ensure that the necessary build tools and system libraries are installed.

---

## Selecting Scheduler Backend

Inside R:

```R
options(clustermq.scheduler = "ocs")  # Open Cluster Scheduler
```

or

```R
options(clustermq.scheduler = "gcs")  # Gridware Cluster Scheduler
```

---

## Example: Distributed Mandelbrot Demo

Run:

```bash
Rscript mandelbrot_clustermq_demo.r
```

The script will:

1. Spawn worker jobs via `qsub`
2. Execute distributed tile computations
3. Collect results on the master node
4. Produce:

```
mandelbrot_color.png
```

The output image is written to the master working directory.

Intermediate tile results are stored in:

```
mandelbrot_tiles/
```

---

## Execution Model

- The master R process runs locally.
- Worker R sessions are started via `qsub`.
- Workers execute `compute_tile()`.
- Results are returned via `clustermq`.
- Final rendering happens on the master process.

---

## OCS vs GCS Notes

`clustermq` supports both OCS and GCS.

Both schedulers are handled slightly differently because GCS offers enterprise features such as:

- CPU binding
- NUMA-aware job placement
- Advanced scheduling policies

These features can significantly improve performance for CPU-bound R workloads.

Templates for OCS and GCS are located in the installed `clustermq` package under `clustermq/templates/`

```
$R_LIBS_USER/clustermq/templates/
```

or 

```
<R_HOME>/library/clustermq/templates/
```

- `OCS.tmpl` – Default template for Open Cluster Scheduler
- `GCS.tmpl` – Template for Gridware Cluster Scheduler with support for enterprise binding and placement features

Both templates should be adapted to your specific cluster environment. You may want to adjust:

- Memory requests
- Runtime limits
- Queue selection
- Binding or placement options

---

## Troubleshooting

### qsub not found

Check:

```R
Sys.getenv("PATH")
Sys.which("qsub")
```

If not found:

- Add the scheduler `bin` directory to `PATH`
- Configure `~/.Renviron`
- Ensure the correct scheduler is prioritized if multiple installations exist

---

### Package installation goes to system library

For shared environments, it may be preferable to install `clustermq` in the system library of each participating host. Consult your system administrator for this option.

---

### R jobs fail or remain queued

Possible causes:

- Insufficient cluster resources
- Worker nodes cannot access the R environment
- Library path not available on execution nodes
- Incorrect template configuration

Check:

- Scheduler queue state
- Job logs
- Worker node environment

You may need to adjust the job template to select an appropriate queue or partition.

For additional guidance, see the `clustermq` FAQ:

https://mschubert.github.io/clustermq/articles/faq.html

---

## Customization

You can adjust several parameters when queuing R functions with `clustermq`.

An example is provided in `mandelbrot_clustermq_demo.r`.

The Mandelbrot example:

- Splits the workload into 160 tiles
- Queues them as tasks in an array job
- Uses a configurable number of workers

You may adjust:

- Tile size and count
- Number of workers
- Memory and runtime parameters
- Scheduler template settings

Adapt these parameters according to available cluster resources and workload requirements.

---

## License

`clustermq` is licensed under the Apache License 2.0:  
https://github.com/mschubert/clustermq/blob/master/LICENSE

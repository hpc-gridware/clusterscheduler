#!/bin/bash
#
# shepherd_wrapper.sh
#
# OCS/GCS Shepherd Wrapper for GPU Device Isolation (RSMAP-based)
#
# This script is registered as shepherd_cmd in the OCS/GCS global/local config.
# It reads the GPU RSMAP selection from the job's environment file,
# builds the corresponding /dev/nvidia* device paths, patches the
# devices_allow entry in the job's CONFIG file, and then exec's
# the real shepherd binary.
#
# This script ensures that the job's GPU devices are isolated from other jobs
# and only the selected GPUs are accessible to the job.
#
# Usage:
#   Register in OCS/GCS config:
#     qconf -mconf          (global)
#     qconf -mconf <host>   (per execution host)
#     -> shepherd_cmd  /path/to/shepherd_wrapper.sh
#
#   The execution daemon has to be restarted so that a new or changed
#   shepherd_cmd is used.
#
# Configuration - adapt these to your cluster:
#   RSMAP_NAME        : name of your GPU RSMAP resource (e.g. "NVIDIA_GPUS").
#                       The name is case sensitive and must match the complex
#                       name exactly, because the granted IDs are exported as
#                       SGE_HGR_<complex name>.
#   REAL_SHEPHERD     : full path to the original sge_shepherd binary. By
#                       default it is derived from SGE_ROOT.
# ---------------------------------------------------------------------------

RSMAP_NAME="${RSMAP_NAME:-NVIDIA_GPUS}"

# The shepherd is started with SGE_ROOT set in its environment. Derive the
# path of the real binary from it so that the wrapper works for any
# installation path and architecture.
if [ -z "$REAL_SHEPHERD" ]; then
    if [ -n "$SGE_ROOT" ] && [ -x "$SGE_ROOT/util/arch" ]; then
        REAL_SHEPHERD="$SGE_ROOT/bin/$("$SGE_ROOT/util/arch")/sge_shepherd"
    else
        echo "shepherd_wrapper: ERROR - SGE_ROOT is not usable and REAL_SHEPHERD is not set" >&2
        exit 1
    fi
fi

if [ ! -x "$REAL_SHEPHERD" ]; then
    echo "shepherd_wrapper: ERROR - shepherd binary not executable: $REAL_SHEPHERD" >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# 1. Locate the job spool files
#    - environment file: contains SGE_HGR_<RSMAP> with selected GPU IDs
#    - config file:      contains devices_allow= line to be patched
#
#    The shepherd is started with the job spool directory as working
#    directory. SGE_JOB_SPOOL_DIR is NOT set in the shepherd environment,
#    so it must not be used here.
# ---------------------------------------------------------------------------
JOB_SPOOL_DIR="$(pwd)"
ENV_FILE="$JOB_SPOOL_DIR/environment"
CONFIG_FILE="$JOB_SPOOL_DIR/config"

if [ ! -f "$ENV_FILE" ]; then
    echo "shepherd_wrapper: ERROR - environment file not found: $ENV_FILE" >&2
    exec "$REAL_SHEPHERD" "$@"
fi

if [ ! -f "$CONFIG_FILE" ]; then
    echo "shepherd_wrapper: ERROR - config file not found: $CONFIG_FILE" >&2
    exec "$REAL_SHEPHERD" "$@"
fi

# Pre-create the usage file so that the execution daemon can always write the
# job usage, even when the job is rejected very early.
touch "$JOB_SPOOL_DIR/usage"

# ---------------------------------------------------------------------------
# 2. Read the GPU RSMAP selection from the environment file
#    The variable is stored as:  SGE_HGR_<RSMAP_NAME>=<gpu_ids>
#    Multiple GPUs are space-separated, e.g. "0 2 3"
#
#    The complex name is used verbatim. xxQS_NAMExx exports the granted IDs as
#    SGE_HGR_<complex name> preserving the case of the complex name, so a
#    complex named "gpu" results in SGE_HGR_gpu and not SGE_HGR_GPU.
#
#    IMPORTANT: Do NOT rely on NVIDIA_VISIBLE_DEVICES - it is set later
#    by the qgpu prolog and is NOT available when the shepherd starts.
# ---------------------------------------------------------------------------
RSMAP_VAR="SGE_HGR_${RSMAP_NAME}"

GPU_IDS=$(grep "^${RSMAP_VAR}=" "$ENV_FILE" | head -1 | cut -d'=' -f2-)

if [ -z "$GPU_IDS" ]; then
    echo "shepherd_wrapper: WARNING - no GPU selection found (${RSMAP_VAR}) in $ENV_FILE" >&2
    exec "$REAL_SHEPHERD" "$@"
fi

echo "shepherd_wrapper: GPU RSMAP selection (${RSMAP_VAR}): $GPU_IDS"

# ---------------------------------------------------------------------------
# 3. Build device paths for each selected GPU
#
#    Source code reference (sge_shepherd add_devices_allow):
#
#      #define DEVICES_DELIMITOR  ";"     <- semicolon separates entries
#      #define DEVICES_DEFAULT_MODE "r"
#
#      Parsing: sge_strtok_r(devices_allow, ";", ...)
#               then strchr(device, '=') to split name from mode
#
#      Format per entry:  <device_path>=<mode>
#      Full string:       /dev/nvidia0=rw;/dev/nvidia1=rw;/dev/nvidiactl=rw
#
#      Supported modes: "r", "w", "rw"
#      GPUs need read+write access -> use "rw"
#
#    Each GPU ID N maps to:
#      /dev/nvidia<N>   - the GPU device
#
#    Shared control devices are always included:
#      /dev/nvidiactl
#      /dev/nvidia-uvm
#      /dev/nvidia-uvm-tools  (if present)
# ---------------------------------------------------------------------------
DEVICE_LIST=""

add_device() {
    local dev="$1"
    local mode="${2:-rw}"
    if [ -z "$DEVICE_LIST" ]; then
        DEVICE_LIST="${dev}=${mode}"
    else
        DEVICE_LIST="${DEVICE_LIST};${dev}=${mode}"
    fi
}

for GPU_ID in $GPU_IDS; do
    # Validate that the ID is numeric
    if ! echo "$GPU_ID" | grep -qE '^[0-9]+$'; then
        echo "shepherd_wrapper: WARNING - skipping non-numeric GPU ID: $GPU_ID" >&2
        continue
    fi

    DEV="/dev/nvidia${GPU_ID}"

    if [ ! -e "$DEV" ]; then
        echo "shepherd_wrapper: WARNING - device does not exist: $DEV" >&2
    fi

    add_device "$DEV" "rw"
done

# Append shared NVIDIA control devices
for CTL_DEV in /dev/nvidiactl /dev/nvidia-uvm /dev/nvidia-uvm-tools; do
    if [ -e "$CTL_DEV" ]; then
        add_device "$CTL_DEV" "rw"
    fi
done

if [ -z "$DEVICE_LIST" ]; then
    echo "shepherd_wrapper: WARNING - no valid GPU devices resolved" >&2
    exec "$REAL_SHEPHERD" "$@"
fi

echo "shepherd_wrapper: devices_allow -> $DEVICE_LIST"

# ---------------------------------------------------------------------------
# 4. Patch the devices_allow entry in the CONFIG file
#
#    The config file contains a line:
#      devices_allow=
#    Replace it with the resolved device list using sed.
#
#    Example result:
#      devices_allow=/dev/nvidia0=rw;/dev/nvidia2=rw;/dev/nvidiactl=rw;/dev/nvidia-uvm=rw
#
#    The entry is only written by the execution daemon when cgroup based
#    device isolation is available on the host. Without it there is nothing to
#    patch and the job would silently run without isolation, so this case is
#    reported instead of being ignored.
# ---------------------------------------------------------------------------
if grep -q "^devices_allow=" "$CONFIG_FILE"; then
    if ! sed -i "s|^devices_allow=.*|devices_allow=${DEVICE_LIST}|" "$CONFIG_FILE"; then
        echo "shepherd_wrapper: ERROR - failed to update devices_allow in $CONFIG_FILE" >&2
    fi
else
    echo "shepherd_wrapper: WARNING - no devices_allow entry in $CONFIG_FILE," \
         "GPU device isolation is not active on this host" >&2
fi

# ---------------------------------------------------------------------------
# 5. Signal prolog to resort device IDs when cgroup isolation is active
#    This is read out by qgpu (Gridware Cluster Scheduler). If using OCS,
#    you can write your own prolog to read this env, and set NVIDIA_VISIBLE_DEVICES
#    and especially CUDA_VISIBLE_DEVICES accordingly for the job.
# ---------------------------------------------------------------------------
if ! grep -q "^SGE_RESORT_NVIDIA_VISIBLE_DEVICES=" "$ENV_FILE"; then
    echo "SGE_RESORT_NVIDIA_VISIBLE_DEVICES=from0" >> "$ENV_FILE"
fi

if ! grep -q "^SGE_RESORT_CUDA_VISIBLE_DEVICES=" "$ENV_FILE"; then
    echo "SGE_RESORT_CUDA_VISIBLE_DEVICES=from0" >> "$ENV_FILE"
fi

# ---------------------------------------------------------------------------
# 6. Hand off to the real shepherd - exec replaces this process
# ---------------------------------------------------------------------------
echo "shepherd_wrapper: launching real shepherd: $REAL_SHEPHERD $*"
exec "$REAL_SHEPHERD" "$@"

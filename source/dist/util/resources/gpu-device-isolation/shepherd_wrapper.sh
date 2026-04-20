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
# Configuration — adapt these to your cluster:
#   RSMAP_NAME        : name of your GPU RSMAP resource (e.g. "gpu")
#   REAL_SHEPHERD     : full path to the original sge_shepherd binary
# ---------------------------------------------------------------------------

#echo "WRAPPER DEBUG: SGE_JOB_SPOOL_DIR=$SGE_JOB_SPOOL_DIR" >> /tmp/wrapper_debug.log
#echo "WRAPPER DEBUG: ENV_FILE exists: $(test -f "$SGE_JOB_SPOOL_DIR/environment" && echo YES || echo NO)" >> /tmp/wrapper_debug.log
#echo "WRAPPER DEBUG: GPU_IDS=$(grep "^SGE_HGR_NVIDIA_GPUS=" "$SGE_JOB_SPOOL_DIR/environment" 2>&1 | head -1 | cut -d'=' -f2-)" >> /tmp/wrapper_debug.log


RSMAP_NAME="${RSMAP_NAME:-NVIDIA_GPUS}"
REAL_SHEPHERD="${REAL_SHEPHERD:-/opt/ocs/bin/lx-amd64/sge_shepherd}"

# ---------------------------------------------------------------------------
# 1. Locate the job spool files
#    - environment file: contains SGE_HGR_<RSMAP> with selected GPU IDs
#    - config file:      contains devices_allow= line to be patched
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

touch $SGE_JOB_SPOOL_DIR/usage

# ---------------------------------------------------------------------------
# 2. Read the GPU RSMAP selection from the environment file
#    The variable is stored as:  SGE_HGR_<RSMAP_NAME>=<gpu_ids>
#    Multiple GPUs are space-separated, e.g. "0 2 3"
#
#    IMPORTANT: Do NOT rely on NVIDIA_VISIBLE_DEVICES — it is set later
#    by the qgpu prolog and is NOT available when the shepherd starts.
# ---------------------------------------------------------------------------
RSMAP_VAR="SGE_HGR_$(echo "$RSMAP_NAME" | tr '[:lower:]' '[:upper:]')"

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
#      /dev/nvidia<N>   — the GPU device
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
#    The config file ($SGE_JOB_SPOOL_DIR/config) contains a line:
#      devices_allow=
#    Replace it with the resolved device list using sed.
#
#    Example result:
#      devices_allow=/dev/nvidia0=rw;/dev/nvidia2=rw;/dev/nvidiactl=rw;/dev/nvidia-uvm=rw
# ---------------------------------------------------------------------------
sed -i "s|^devices_allow=.*|devices_allow=${DEVICE_LIST}|" "$CONFIG_FILE"

if [ $? -ne 0 ]; then
    echo "shepherd_wrapper: ERROR - failed to update devices_allow in $CONFIG_FILE" >&2
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
# 6. Hand off to the real shepherd — exec replaces this process
# ---------------------------------------------------------------------------
echo "shepherd_wrapper: launching real shepherd: $REAL_SHEPHERD $*"
exec "$REAL_SHEPHERD" "$@"

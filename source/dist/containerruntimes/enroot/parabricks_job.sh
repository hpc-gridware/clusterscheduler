#!/bin/bash
#$ -S /bin/bash
#$ -V
#$ -l h_rt=1:00:00
#$ -l h_vmem=16G
#$ -l NVIDIA_GPUS=1
#$ -q gpu.q
#$ -j y
#$ -N enrootTest

set -euo pipefail
set -x

# ------------------------------------------------------------------------------
# This job script runs NVIDIA's Clara Parabricks in an enroot container.
# It requires a gpu.q which setup the NVIDIA_VISIBLE_DEVICES env variable
# and configures the NVIDIA_GPUS RSMAP. This is typically setup by 
# Gridware Cluster Scheduler's qgpu utility.
# ------------------------------------------------------------------------------

# ------------------------------------------------------------------------------
# User-configurable settings
# ------------------------------------------------------------------------------
CONTAINER_NAME="parabricks450"
# By default, look for the image in the user's home directory
IMAGE_FILE="$HOME/parabricks4501.sqsh"
IMAGE_SOURCE="docker://nvcr.io#nvidia/clara/clara-parabricks:4.5.0-1"

# (Optional) If $TMPDIR or a scratch path is preferred, change IMAGE_FILE as needed.
# e.g., IMAGE_FILE="$TMPDIR/parabricks4501.sqsh"
# ------------------------------------------------------------------------------

# Ensure XDG_RUNTIME_DIR is set (some sites require this for enroot)
export XDG_RUNTIME_DIR="/tmp"

echo "Starting the job script..."
echo "Host: $(hostname)"
echo "Working directory: $(pwd)"
echo "Date: $(date)"
echo "----------------------------------------"

# 1) Check if the container is already listed
if enroot list | grep -q "^${CONTAINER_NAME}\$"; then
  echo "Container '${CONTAINER_NAME}' already exists."
else
  echo "Container '${CONTAINER_NAME}' not found."

  # 2) Check if the image file is already downloaded
  if [[ -f "${IMAGE_FILE}" ]]; then
    echo "Using existing local image file '${IMAGE_FILE}'."
  else
    echo "Local image file '${IMAGE_FILE}' not found. Importing from Docker registry..."
    enroot import -o "${IMAGE_FILE}" "${IMAGE_SOURCE}"
  fi

  # 3) Double-check that the image file now exists
  if [[ -f "${IMAGE_FILE}" ]]; then
    echo "Confirmed image file exists: ${IMAGE_FILE}"
  else
    echo "ERROR: Image file '${IMAGE_FILE}' was not found or failed to import."
    exit 1
  fi

  # 4) Create the container from the image
  echo "Creating enroot container '${CONTAINER_NAME}'..."
  enroot create --name "${CONTAINER_NAME}" "${IMAGE_FILE}"
fi

echo "----------------------------------------"
echo "Current enroot container list:"
enroot list
echo "----------------------------------------"

# 5) Start the container and run nvidia-smi (example command)
echo "Launching '${CONTAINER_NAME}' container..."
enroot start --rw "${CONTAINER_NAME}" nvidia-smi

echo "----------------------------------------"
echo "Job script completed."


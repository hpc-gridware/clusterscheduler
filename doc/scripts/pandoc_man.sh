#!/bin/sh

if [ $# -ne 10 ]; then
   echo "usage: $0 <source directory> <input_directory> <target directory> <target man page> <template> <release> <date> <located_in_gcs_extensions> <build_open_source_version>"
   exit 1
fi

SOURCE_DIR=$1
INPUT_DIR=$2
OUTPUT_DIR=$3
PAGE=$4
TEMPLATE=$5
SECTION=$6
RELEASE=$7
DATE=$8
LOCATED_IN_GCS_EXTENSIONS=$9
BUILD_OPEN_SOURCE_VERSION=${10}

if [ $LOCATED_IN_GCS_EXTENSIONS -eq 0 ]; then
   COMMON_DIR="../clusterscheduler/doc/markdown/man"
else
   COMMON_DIR="../gcs-extensions/doc/markdown/man"
fi

if [ $TEMPLATE = "NONE" ]; then
   TEMPLATE_FILE=""
else
   TEMPLATE_FILE="${SOURCE_DIR}/${COMMON_DIR}/man${SECTION}/${TEMPLATE}.md"
fi

PAGE_FILE="${INPUT_DIR}/${PAGE}.md"
OUTPUT_FILE="${OUTPUT_DIR}/${PAGE}.${SECTION}"

if [ ${BUILD_OPEN_SOURCE_VERSION} -eq 1 ]; then
   QSNAME="Open Cluster Scheduler"
else
   QSNAME="Gridware Cluster Scheduler"
fi
QSPREFIX_LOWER="sge"
QSPREFIX_UPPER="SGE"

PANDOC=pandoc
OPTIONS="--standalone --to man"

echo "pandox_man.sh: ${PAGE_FILE} ${TEMPLATE_FILE} ${OUTPUT_FILE}"
cat ${PAGE_FILE} ${TEMPLATE_FILE} | \
    sed -e "s/__RELEASE__/${QSNAME} ${RELEASE}/g" \
        -e "s/__DATE__/${DATE}/g" -e "s/xxQS_NAMExx/${QSNAME}/g" \
        -e "s/xxqs_name_sxx/${QSPREFIX_LOWER}/g" \
        -e "s/xxQS_NAME_Sxx/${QSPREFIX_UPPER}/g" | \
    ${PANDOC} ${OPTIONS} -o ${OUTPUT_FILE}

#!/bin/sh

if [ $# -ne 7 ]; then
   echo "usage: $0 <source directory> <target directory> <target man page> <template> <release> <date>"
   exit 1
fi

INPUT_DIR=$1
OUTPUT_DIR=$2
PAGE=$3
TEMPLATE=$4
SECTION=$5
RELEASE=$6
DATE=$7

if [ $TEMPLATE = "NONE" ]; then
   TEMPLATE_FILE=""
else
   TEMPLATE_FILE="${INPUT_DIR}/${TEMPLATE}.md"
fi

PAGE_FILE="${INPUT_DIR}/${PAGE}.md"
OUTPUT_FILE="${OUTPUT_DIR}/${PAGE}.${SECTION}"

QSNAME="Open Gridengine"
QSPREFIX_LOWER="sge"
QSPREFIX_UPPER="SGE"

PANDOC=pandoc
OPTIONS="--standalone --to man"

cat ${PAGE_FILE} ${TEMPLATE_FILE} | \
    sed -e "s/__RELEASE__/${QSNAME} ${RELEASE}/g" -e "s/__DATE__/${DATE}/g" -e "s/xxQS_NAMExx/${QSNAME}/g" \
        -e "s/xxqs_name_sxx/${QSPREFIX_LOWER}/g" -e "s/xxQS_NAME_Sxx/${QSPREFIX_UPPER}/g" | \
    ${PANDOC} ${OPTIONS} -o ${OUTPUT_FILE}

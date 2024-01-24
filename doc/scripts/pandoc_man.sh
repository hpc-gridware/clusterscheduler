#!/bin/sh

if [ $# -ne 8 ]; then
   echo "usage: $0 <source directory> <input_directory> <target directory> <target man page> <template> <release> <date>"
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

# Independent if INPUT_DIR is located in oge-extensions repository
# then template files will also be taken from oge-extensions
COMMON_DIR="../oge-extensions/doc/markdown/manual"

if [ $TEMPLATE = "NONE" ]; then
   TEMPLATE_FILE=""
else
   TEMPLATE_FILE="${SOURCE_DIR}/${COMMON_DIR}/${TEMPLATE}.md"
fi

PAGE_FILE="${INPUT_DIR}/${PAGE}.md"
OUTPUT_FILE="${OUTPUT_DIR}/${PAGE}.${SECTION}"

QSNAME="Open Grid Engine"
QSPREFIX_LOWER="sge"
QSPREFIX_UPPER="SGE"

PANDOC=pandoc
OPTIONS="--standalone --to man"

cat ${PAGE_FILE} ${TEMPLATE_FILE} | \
    sed -e "s/__RELEASE__/${QSNAME} ${RELEASE}/g" \
        -e "s/__DATE__/${DATE}/g" -e "s/xxQS_NAMExx/${QSNAME}/g" \
        -e "s/xxqs_name_sxx/${QSPREFIX_LOWER}/g" \
        -e "s/xxQS_NAME_Sxx/${QSPREFIX_UPPER}/g" | \
    ${PANDOC} ${OPTIONS} -o ${OUTPUT_FILE}

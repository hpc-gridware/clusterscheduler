#!/bin/sh

# generate PDF from markdown manuals
# see also https://jdhao.github.io/2019/05/30/markdown2pdf_pandoc/

if [ $# -ne 7 ]; then
   echo "usage: $0 <source directory> <input directory> <target directory> <target manual> <release> <date>"
   exit 1
fi

SOURCE_DIR=$1
INPUT_DIR=$2
OUTPUT_DIR=$3
MANUAL=$4
TITLE=$5
RELEASE=$6
DATE=$7

# Independent if INPUT_DIR is located in the gridengine or oge-extensions repository
# common files for the manuals will be taken from the gridengine repository
COMMON_DIR="../gridengine/doc/markdown/manual"
SETTINGS_FILE="${SOURCE_DIR}/${COMMON_DIR}/head.tex"
TITLE_PAGE="${SOURCE_DIR}/${COMMON_DIR}/titlepage.md"
COPYRIGHT_PAGE="${SOURCE_DIR}/${COMMON_DIR}/copyright.md"
DEFINITIONS_PAGE="${SOURCE_DIR}/${COMMON_DIR}/typographic_conventions.md"

# Input files for the manual itself come from gridengine or oge-extensions repository
MANUAL_FILES="${INPUT_DIR}/*.md"
OUTPUT_FILE="${OUTPUT_DIR}/${MANUAL}.pdf"

QSNAME="Open Grid Engine"
QSPREFIX_LOWER="sge"
QSPREFIX_UPPER="SGE"
QSCOMPANYNAME="HPC-Gridware"
QSCOMPANYMAIL="sales@hpc-gridware.com"

# to show available styles: pandoc --list-highlight-style
# to show if hightlighting is supported for a language: pandoc --list-highlight-languagess
PANDOC=pandoc
OPTIONS="--pdf-engine=xelatex -H ${SETTINGS_FILE}"
# select the font family: -V CJKmainfont="<font>"
#OPTIONS="$OPTIONS --highlight-style kate"
OPTIONS="$OPTIONS --highlight-style espresso"
OPTIONE="$OPTIONS -V colorlinks=true -V urlcolor=NavyBlue -V toccolor=red"
OPTIONS="$OPTIONS --table-of-contents"
OPTIONS="$OPTIONS --number-sections"
OPTIONS="$OPTIONS -V subparagraph"

cat ${TITLE_PAGE} ${COPYRIGHT_PAGE} ${DEFINITIONS_PAGE} ${MANUAL_FILES} | \
    sed -e "s~__IMAGE_DIR__~${INPUT_DIR}~g" \
        -e "s/__RELEASE__/${QSNAME} ${RELEASE}/g" \
        -e "s/__DATE__/${DATE}/g" \
        -e "s/__TITLE__/${TITLE}/g" \
        -e "s/xxQS_NAMExx/${QSNAME}/g" \
        -e "s/xxqs_name_sxx/${QSPREFIX_LOWER}/g" \
        -e "s/xxQS_NAME_Sxx/${QSPREFIX_UPPER}/g" \
        -e "s/xxQS_COMPANY_NAMExx/${QSCOMPANYNAME}/g" \
        -e "s/xxQS_COMPANY_MAILxx/${QSCOMPANYMAIL}/g" | \
    ${PANDOC} ${OPTIONS} -o ${OUTPUT_FILE} --listings



#!/bin/sh

# generate PDF from markdown manuals
# see also https://jdhao.github.io/2019/05/30/markdown2pdf_pandoc/

if [ $# -lt 9 ] || [ $# -gt 10 ]; then
   echo "usage: $0 <source directory> <input directory> <target directory> <target manual> <title> <release> <date> <located_in_gcs_extensions> <build_open_source_version> [<annotations>]"
   echo "       <annotations> is on or off and defaults to \$SPEC_ANNOTATIONS or on."
   exit 1
fi

SOURCE_DIR=$1
INPUT_DIR=$2
OUTPUT_DIR=$3
MANUAL=$4
TITLE=$5
RELEASE=$6
DATE=$7
LOCATED_IN_GCS_EXTENSIONS=$8
BUILD_OPEN_SOURCE_VERSION=$9

# Progress marks, ticket tags and margin notes are rendered unless they are
# switched off. "off" reproduces the frozen wording of the specification and
# is what a contract version is built with. CMake passes nine arguments, so
# the environment variable is the way to select it from a build tree.
ANNOTATIONS=${10:-${SPEC_ANNOTATIONS:-on}}

# The copyright pages carry a year range whose end has to follow the build. __DATE__ is a
# full date and cannot be used for it, so take the year off the front of it - that way the
# two can never disagree.
YEAR=${DATE%%-*}

if [ $LOCATED_IN_GCS_EXTENSIONS -eq 0 ]; then
   COMMON_DIR="../clusterscheduler/doc/markdown/specification"
else
   COMMON_DIR="../gcs-extensions/doc/markdown/specification"
fi

SETTINGS_FILE="${SOURCE_DIR}/${COMMON_DIR}/head.tex"
ANNOTATION_FILTER="${SOURCE_DIR}/${COMMON_DIR}/annotations.lua"
NO_ANNOTATION_FILE="${SOURCE_DIR}/${COMMON_DIR}/no_annotations.tex"
TITLE_PAGE="${SOURCE_DIR}/${COMMON_DIR}/titlepage.md"
COPYRIGHT_PAGE="${SOURCE_DIR}/${COMMON_DIR}/copyright.md"
DEFINITIONS_PAGE="${SOURCE_DIR}/${COMMON_DIR}/typographic_conventions.md"

# Input files for the manual itself come from clusterscheduler or gcs-extensions repository
MANUAL_FILES="${INPUT_DIR}/*.md"
OUTPUT_FILE="${OUTPUT_DIR}/${MANUAL}.pdf"

if [ ${BUILD_OPEN_SOURCE_VERSION} -eq 1 ]; then
   QSNAME="Open Cluster Scheduler"
else
   QSNAME="Gridware Cluster Scheduler"
fi
QSPREFIX_LOWER="sge"
QSPREFIX_UPPER="SGE"
QSCOMPANYNAME="HPC-Gridware"
QSCOMPANYMAIL="sales@hpc-gridware.com"

# to show available styles: pandoc --list-highlight-style
# to show if hightlighting is supported for a language: pandoc --list-highlight-languagess
PANDOC=pandoc

# The annotation filter needs the pandoc.log module, which arrived in pandoc 3.
# An older pandoc does not merely reject the filter: 2.0.6, the version EL8
# ships, dies on it with a segmentation fault, and it cannot be taught to step
# aside on its own because PANDOC_VERSION is unset before pandoc 2.1. So the
# decision is taken here, and a pandoc too old for the filter builds the
# unannotated specification instead of breaking the build.
PANDOC_VERSION_LINE=$(${PANDOC} --version 2>/dev/null | sed -n '1p')
PANDOC_MAJOR=$(echo "${PANDOC_VERSION_LINE}" | sed -n 's/^[^0-9]*\([0-9]*\).*/\1/p')
if [ -z "${PANDOC_MAJOR}" ] || [ "${PANDOC_MAJOR}" -lt 3 ]; then
   if [ "${ANNOTATIONS}" != "off" ]; then
      echo "${0##*/}: ${PANDOC_VERSION_LINE:-pandoc of unknown version} cannot run the annotation filter, building ${MANUAL}.pdf without annotations" >&2
      ANNOTATIONS=off
   fi
   ANNOTATION_FILTER=""
fi

# The marker has to be read before head.tex, which asks for it.
OPTIONS="--pdf-engine=xelatex"
if [ "${ANNOTATIONS}" = "off" ] && [ -f "${NO_ANNOTATION_FILE}" ]; then
   OPTIONS="$OPTIONS -H ${NO_ANNOTATION_FILE}"
fi
OPTIONS="$OPTIONS -H ${SETTINGS_FILE}"
# select the font family: -V CJKmainfont="<font>"
#OPTIONS="$OPTIONS --highlight-style kate"
OPTIONS="$OPTIONS --highlight-style espresso"
OPTIONE="$OPTIONS -V colorlinks=true -V urlcolor=NavyBlue -V toccolor=red"
OPTIONS="$OPTIONS --table-of-contents"
OPTIONS="$OPTIONS --toc-depth=6"
OPTIONS="$OPTIONS --number-sections"
OPTIONS="$OPTIONS -V subparagraph"
if [ -f "${ANNOTATION_FILTER}" ]; then
   OPTIONS="$OPTIONS --lua-filter=${ANNOTATION_FILTER} -M annotations=${ANNOTATIONS}"
fi

cat ${TITLE_PAGE} ${COPYRIGHT_PAGE} ${DEFINITIONS_PAGE} ${MANUAL_FILES} | \
    sed -e "s~__INPUT_DIR__~${INPUT_DIR}~g" \
        -e "s/__RELEASE__/${QSNAME} ${RELEASE}/g" \
        -e "s/__DATE__/${DATE}/g" \
        -e "s/__TITLE__/${TITLE}/g" \
        -e "s/__YEAR__/${YEAR}/g" \
        -e "s/xxQS_NAMExx/${QSNAME}/g" \
        -e "s/xxqs_name_sxx/${QSPREFIX_LOWER}/g" \
        -e "s/xxQS_NAME_Sxx/${QSPREFIX_UPPER}/g" \
        -e "s/xxQS_COMPANY_NAMExx/${QSCOMPANYNAME}/g" \
        -e "s/xxQS_COMPANY_MAILxx/${QSCOMPANYMAIL}/g" | \
    ${PANDOC} ${OPTIONS} -o ${OUTPUT_FILE} --listings



#!/bin/sh
#___INFO__MARK_BEGIN_NEW__
###########################################################################
#
#  Copyright 2026 HPC-Gridware GmbH
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
###########################################################################
#___INFO__MARK_END_NEW__

# Upload the SBOMs of an installed cluster to Dependency-Track.
#
#   dtrack_upload_sboms.sh [-n]
#
#   -n   dry run: only read from Dependency-Track, create and upload nothing
#
# The product and its version come from "qconf -help" of the installation in $SGE_ROOT, e.g.
# "GCS 9.1.7prealpha". In Dependency-Track the product version is a parent project
# ("Gridware Cluster Scheduler" / "9.1.7prealpha"), created when it does not exist yet. Every
# SBOM found below $SGE_ROOT is uploaded to a child project of it, which the upload creates:
#
#   $SGE_ROOT/3rd_party/sbom.cyclonedx.json                  -> "GCS core"
#   $SGE_ROOT/3rd_party/<component>/sbom.cyclonedx.json      -> "GCS <component>"
#   $SGE_ROOT/<component>/3rdparty-licenses/sbom.cyclonedx.json -> "GCS <component>" (dbwriter)
#
# The child projects carry the short product name, as a project is identified by name and version
# across Dependency-Track - "drmaaj" 9.1.7 of OCS and of GCS would otherwise be the same project.
#
# Reads dtrack.site and dtrack.token (an API key of a team with the permissions VIEW_PORTFOLIO,
# PORTFOLIO_MANAGEMENT, BOM_UPLOAD and PROJECT_CREATION_UPLOAD) from $HOME/.dtrack.properties.
# Needs curl and jq.

PROPERTIES="$HOME/.dtrack.properties"
# how long to wait for Dependency-Track to process one uploaded SBOM
PROCESSING_TIMEOUT=300

dry_run=false
if [ "$1" = "-n" ]; then
   dry_run=true
elif [ $# -gt 0 ]; then
   echo "usage: `basename $0` [-n]" >&2
   exit 1
fi

fail() {
   echo "error: $*" >&2
   exit 1
}

for tool in curl jq; do
   command -v $tool > /dev/null || fail "$tool is required"
done

# --- configuration ---------------------------------------------------------------------------

[ -r "$PROPERTIES" ] || fail "cannot read $PROPERTIES"
DT_SITE=`grep '^dtrack.site=' "$PROPERTIES" | cut -d= -f2- | sed 's,/*$,,'`
DT_TOKEN=`grep '^dtrack.token=' "$PROPERTIES" | cut -d= -f2-`
[ -n "$DT_SITE" ] || fail "no dtrack.site in $PROPERTIES"
[ -n "$DT_TOKEN" ] || fail "no dtrack.token in $PROPERTIES"

[ -n "$SGE_ROOT" ] || fail "SGE_ROOT is not set"
[ -d "$SGE_ROOT" ] || fail "SGE_ROOT $SGE_ROOT is no directory"
# without trailing slashes, the paths of the SBOMs are compared with it, see component_of
SGE_ROOT=`echo "$SGE_ROOT" | sed 's,/*$,,'`

# --- product and version ---------------------------------------------------------------------

ARCH=`"$SGE_ROOT/util/arch"` || fail "cannot determine the architecture"
first_line=`"$SGE_ROOT/bin/$ARCH/qconf" -help 2>&1 | head -1`
short_name=`echo "$first_line" | awk '{print $1}'`
version=`echo "$first_line" | awk '{print $2}'`
case "$short_name" in
   GCS) product="Gridware Cluster Scheduler" ;;
   OCS) product="Open Cluster Scheduler" ;;
   *)   fail "cannot determine the product from qconf -help: $first_line" ;;
esac
[ -n "$version" ] || fail "cannot determine the version from qconf -help: $first_line"
echo "product: $product $version ($SGE_ROOT)"

# --- Dependency-Track API --------------------------------------------------------------------

# curl wrapper: dt_call <method> <path> [curl args...] - prints the body, returns 1 on HTTP >= 400
dt_call() {
   method=$1
   path=$2
   shift 2
   response=`curl -s -S -w '\n%{http_code}' -X "$method" -H "X-Api-Key: $DT_TOKEN" "$@" \
              "$DT_SITE/api/v1$path"` || return 1
   http_code=`echo "$response" | tail -1`
   echo "$response" | sed '$d'
   [ "$http_code" -lt 400 ]
}

# uuid of a project, empty when it does not exist
dt_project_uuid() {
   name=`printf '%s' "$1" | jq -s -R -r @uri`
   ver=`printf '%s' "$2" | jq -s -R -r @uri`
   dt_call GET "/project/lookup?name=$name&version=$ver" 2>/dev/null |
      jq -r '.uuid // empty' 2>/dev/null
}

# a lookup of a project which does not exist fails as well - so check the access itself first,
# otherwise a wrong token or site would look like a project to be created
dt_call GET "/project?pageSize=1" > /dev/null ||
   fail "cannot access Dependency-Track at $DT_SITE - check dtrack.site and dtrack.token"

# the parent project: the product version, aggregating its children
parent_uuid=`dt_project_uuid "$product" "$version"`
if [ -n "$parent_uuid" ]; then
   echo "project \"$product\" $version exists: $parent_uuid"
elif $dry_run; then
   echo "dry run: would create project \"$product\" $version"
else
   body=`jq -n --arg name "$product" --arg version "$version" \
         '{name: $name, version: $version, classifier: "APPLICATION",
           collectionLogic: "AGGREGATE_DIRECT_CHILDREN"}'`
   result=`dt_call PUT /project -H "Content-Type: application/json" -d "$body"` ||
      fail "creating project \"$product\" $version failed: $result"
   parent_uuid=`echo "$result" | jq -r '.uuid'`
   echo "created project \"$product\" $version: $parent_uuid"
fi

# --- the SBOMs -------------------------------------------------------------------------------

# component name from the location of an SBOM, see the header
component_of() {
   dir=`dirname "$1"`
   case "$dir" in
      "$SGE_ROOT/3rd_party")      echo "core" ;;
      */3rdparty-licenses)        basename "`dirname "$dir"`" ;;
      *)                          basename "$dir" ;;
   esac
}

errors=0
sboms=`find "$SGE_ROOT" -maxdepth 3 -name sbom.cyclonedx.json -type f | sort`
[ -n "$sboms" ] || fail "no sbom.cyclonedx.json below $SGE_ROOT"

for sbom in $sboms; do
   project="$short_name `component_of "$sbom"`"
   if $dry_run; then
      uuid=`dt_project_uuid "$project" "$version"`
      echo "dry run: would upload $sbom to \"$project\" $version (${uuid:-would be created})"
      continue
   fi

   result=`dt_call POST /bom -F "projectName=$project" -F "projectVersion=$version" \
                             -F "autoCreate=true" -F "parentUUID=$parent_uuid" -F "bom=@$sbom"`
   if [ $? -ne 0 ]; then
      echo "error: uploading $sbom to \"$project\" $version failed: $result" >&2
      errors=`expr $errors + 1`
      continue
   fi
   token=`echo "$result" | jq -r '.token'`

   # the upload is processed asynchronously - wait for it, so that errors are seen here
   waited=0
   while [ $waited -lt $PROCESSING_TIMEOUT ]; do
      processing=`dt_call GET "/event/token/$token" | jq -r '.processing'`
      [ "$processing" = "false" ] && break
      sleep 2
      waited=`expr $waited + 2`
   done
   if [ "$processing" = "false" ]; then
      echo "uploaded $sbom to \"$project\" $version"
   else
      echo "error: \"$project\" $version: $sbom not processed after ${PROCESSING_TIMEOUT}s" >&2
      errors=`expr $errors + 1`
   fi
done

[ $errors -eq 0 ] || fail "$errors SBOM(s) could not be uploaded"
exit 0

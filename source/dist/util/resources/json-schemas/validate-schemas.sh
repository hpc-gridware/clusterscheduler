#!/bin/sh
#
# Validate the OCS/GCS JSON schema set.
#
# Compiles every *.schema.json file in v9.2/ against JSON Schema draft 2020-12
# with all the other schema files registered, so that cross-file "$ref"s (e.g.
# the per-type ocs-qconf-* schemas referencing ocs-qconf-common, and the
# ocs-qconf-root dispatcher referencing every type schema) are resolved. A
# successful compile proves each schema is a valid JSON Schema and that all of
# its references resolve.
#
# Requires the ajv CLI and the ajv-formats plugin (npm install -g ajv-cli
# ajv-formats); ajv-formats provides the "date-time" format used by some schemas.
# Intended to be run in CI; see .github/workflows/validate_schemas.yml.
#
# Exit status is non-zero if any schema fails to compile.

set -u

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
SCHEMA_DIR="${SCRIPT_DIR}/v9.2"

if ! command -v ajv >/dev/null 2>&1; then
   echo "ajv not found. Install it with: npm install -g ajv-cli" >&2
   exit 2
fi

cd "${SCHEMA_DIR}" || exit 2

schemas=$(ls *.schema.json 2>/dev/null)
if [ -z "${schemas}" ]; then
   echo "no *.schema.json files found in ${SCHEMA_DIR}" >&2
   exit 2
fi

rc=0
for s in ${schemas}; do
   # register every other schema so cross-file $refs resolve
   refs=""
   for r in ${schemas}; do
      [ "${r}" = "${s}" ] || refs="${refs} -r ${r}"
   done

   # shellcheck disable=SC2086
   if ajv compile -c ajv-formats --spec=draft2020 -s "${s}" ${refs} >/dev/null 2>/tmp/ajv_err.$$; then
      echo "ok:   ${s}"
   else
      echo "FAIL: ${s}"
      sed 's/^/      /' /tmp/ajv_err.$$
      rc=1
   fi
   rm -f /tmp/ajv_err.$$
done

if [ ${rc} -eq 0 ]; then
   echo "all schemas valid"
fi
exit ${rc}

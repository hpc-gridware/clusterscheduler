#!/bin/sh

jsv_on_start()
{
   return
}

jsv_on_verify()
{
   jsv_reject_wait "Cluster Scheduler is in maintainance mode and does not accept new jobs"
}

. ${SGE_ROOT}/util/resources/jsv/jsv_include.sh

jsv_main


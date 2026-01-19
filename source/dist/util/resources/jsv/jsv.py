#!/usr/bin/env python3

# ___INFO__MARK_BEGIN_NEW__
"""
Copyright 2025-2026 HPC-Gridware GmbH

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""
# ___INFO__MARK_END_NEW__

from __future__ import annotations

from typing import Optional

# Adjust path to import JSV module
from os import getenv
import sys
basepath = "%s/util/resources/jsv" % getenv("SGE_ROOT")
sys.path.append(basepath)
from JSV import JSV

jsv = JSV()


def on_start() -> None:
    jsv.send("SEND ENV")

def _to_int(s: str) -> Optional[int]:
    try:
        return int(s)
    except Exception:
        return None


def on_verify() -> None:
    do_correct = False
    do_wait = False

    # if binary job then reject
    if jsv.get_param("b") == "y":
        jsv.reject("Binary job is rejected.")
        return

    # if pe_name != "" enforce multiple-of-16 slots based on pe_min
    if jsv.get_param("pe_name") != "":
        slots_s = jsv.get_param("pe_min")
        slots = _to_int(slots_s)

        if slots is None:
            jsv.reject("Parallel job has non-numeric pe_min slot request")
            return

        if (slots % 16) > 0:
            jsv.reject("Parallel job does not request a multiple of 16 slots")
            return

    # l_hard handling: delete h_vmem -> do_wait, delete h_data -> do_correct
    if jsv.is_param("l_hard"):
        context = jsv.get_param("CONTEXT")
        has_h_vmem = jsv.sub_is_param("l_hard", "h_vmem")
        has_h_data = jsv.sub_is_param("l_hard", "h_data")

        if has_h_vmem:
            jsv.sub_del_param("l_hard", "h_vmem")
            do_wait = True
            if context == "client":
                jsv.log_info("h_vmem as hard resource requirement has been deleted")

        if has_h_data:
            jsv.sub_del_param("l_hard", "h_data")
            do_correct = True
            if context == "client":
                jsv.log_info("h_data as hard resource requirement has been deleted")

    # ac handling:
    # - if a exists: a = a + 1 else: a = 1
    # - if b exists: delete b
    # - add c (no value)
    if jsv.is_param("ac"):
        context = jsv.get_param("CONTEXT")
        has_ac_a = jsv.sub_is_param("ac", "a")
        has_ac_b = jsv.sub_is_param("ac", "b")

        if has_ac_a:
            ac_a_value_s = jsv.sub_get_param("ac", "a")
            ac_a_value = _to_int(ac_a_value_s)
            if ac_a_value is None:
                ac_a_value = 0
            new_value = ac_a_value + 1  # (fixes Tcl typo: $c_a_value)
            jsv.sub_add_param("ac", "a", str(new_value))
        else:
            jsv.sub_add_param("ac", "a", "1")

        if has_ac_b:
            jsv.sub_del_param("ac", "b")

        jsv.sub_add_param("ac", "c", "")

        do_correct = True

        if context == "client":
            jsv.log_info("ac resource requirements updated (a incremented, b removed, c added)")

    # final decision
    if do_wait:
        jsv.reject_wait("Job is rejected. It might be submitted later.")
    elif do_correct:
        jsv.correct("Job was modified before it was accepted")
    else:
        jsv.accept("Job is accepted")


jsv.on_start = on_start
jsv.on_verify = on_verify

if __name__ == "__main__":
    jsv.main()
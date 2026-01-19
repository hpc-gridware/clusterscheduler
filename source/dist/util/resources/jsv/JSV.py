#!/usr/bin/env python3

#___INFO__MARK_BEGIN_NEW__
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
#___INFO__MARK_END_NEW__

from __future__ import annotations

import os
import sys
import time
from dataclasses import dataclass, field
from typing import Callable, Dict, Optional, Set


@dataclass
class JSV:
    # ---- config ----
    logging_enabled: bool = True
    logfile: str = field(default_factory=lambda: f"/tmp/jsv_{os.getpid()}.log")

    # ---- protocol state ----
    state: str = "initialized"  # initialized | started | verifying
    quit_flag: bool = False

    # ---- storage ----
    params: Dict[str, str] = field(default_factory=dict)
    envs: Dict[str, str] = field(default_factory=dict)
    all_envs: Set[str] = field(default_factory=set)

    # ---- callbacks ----
    # Users can override these by assignment: jsv.on_start = ..., jsv.on_verify = ...
    on_start: Optional[Callable[[], None]] = None
    on_verify: Optional[Callable[[], None]] = None

    # ---------------------------
    # Low-level I/O + logging
    # ---------------------------

    def _script_log(self, msg: str) -> None:
        if not self.logging_enabled:
            return
        try:
            with open(self.logfile, "a", encoding="utf-8") as f:
                f.write(msg + "\n")
        except Exception:
            # logging must never break protocol
            pass

    def send(self, cmd: str) -> None:
        print(cmd)
        sys.stdout.flush()
        self._script_log(f"<<< {cmd}")

    def log_info(self, msg: str) -> None:
        self.send(f"LOG INFO {msg}")

    def log_warning(self, msg: str) -> None:
        self.send(f"LOG WARNING {msg}")

    def log_error(self, msg: str) -> None:
        self.send(f"LOG ERROR {msg}")

    # ---------------------------
    # Params API
    # ---------------------------

    def clear_params(self) -> None:
        self.params.clear()

    def is_param(self, suffix: str) -> bool:
        return suffix in self.params

    def get_param(self, suffix: str) -> str:
        return self.params.get(suffix, "")

    def set_param(self, suffix: str, value: str) -> None:
        self.params[suffix] = value
        self.send(f"PARAM {suffix} {value}")

    def del_param(self, suffix: str) -> None:
        if suffix in self.params:
            del self.params[suffix]
        self.send(f"PARAM {suffix}")

    # ---------------------------
    # Sub-params API (comma-separated key=value list)
    # ---------------------------

    @staticmethod
    def _parse_subparams(raw: str) -> Dict[str, str]:
        out: Dict[str, str] = {}
        if not raw:
            return out
        for token in raw.split(","):
            token = token.strip()
            if not token:
                continue
            if "=" in token:
                k, v = token.split("=", 1)
                out[k] = v
            else:
                out[token] = ""
        return out

    @staticmethod
    def _format_subparams(d: Dict[str, str]) -> str:
        # Keep insertion order (Python 3.7+) to avoid surprising reordering.
        parts: list[str] = []
        for k, v in d.items():
            parts.append(f"{k}={v}" if v else k)
        return ",".join(parts)

    def sub_is_param(self, suffix: str, sub: str) -> bool:
        return sub in self._parse_subparams(self.get_param(suffix))

    def sub_get_param(self, suffix: str, sub: str) -> str:
        return self._parse_subparams(self.get_param(suffix)).get(sub, "")

    def sub_add_param(self, suffix: str, sub: str, value: str = "") -> None:
        d: Dict[str, str] = self._parse_subparams(self.get_param(suffix)) if self.is_param(suffix) else {}
        d[sub] = value
        newval: str = self._format_subparams(d)
        self.params[suffix] = newval
        self.send(f"PARAM {suffix} {newval}")

    def sub_del_param(self, suffix: str, sub: str) -> None:
        if not self.is_param(suffix):
            return
        d: Dict[str, str] = self._parse_subparams(self.get_param(suffix))
        d.pop(sub, None)
        newval: str = self._format_subparams(d)
        self.params[suffix] = newval
        self.send(f"PARAM {suffix} {newval}")

    # ---------------------------
    # Env API
    # ---------------------------

    def clear_envs(self) -> None:
        self.envs.clear()
        self.all_envs.clear()

    def is_env(self, name: str) -> bool:
        return name in self.envs

    def get_env(self, name: str) -> str:
        return self.envs.get(name, "")

    def add_env(self, name: str, value: str) -> None:
        self.envs[name] = value
        self.all_envs.add(name)
        self.send(f"ENV ADD {name} {value}")

    def mod_env(self, name: str, value: str) -> None:
        self.envs[name] = value
        self.send(f"ENV MOD {name} {value}")

    def del_env(self, name: str) -> None:
        if name in self.envs:
            del self.envs[name]
        self.send(f"ENV DEL {name}")

    # ---------------------------
    # RESULT helpers
    # ---------------------------

    def _require_verifying(self) -> bool:
        if self.state != "verifying":
            self.send(f"ERROR JSV script will send RESULT command but is in state {self.state}")
            return False
        return True

    def accept(self, msg: str = "") -> None:
        if self._require_verifying():
            self.send(f"RESULT STATE ACCEPT {msg}".rstrip())
            self.state = "initialized"

    def correct(self, msg: str = "") -> None:
        if self._require_verifying():
            self.send(f"RESULT STATE CORRECT {msg}".rstrip())
            self.state = "initialized"

    def reject(self, msg: str = "") -> None:
        if self._require_verifying():
            self.send(f"RESULT STATE REJECT {msg}".rstrip())
            self.state = "initialized"

    def reject_wait(self, msg: str = "") -> None:
        if self._require_verifying():
            self.send(f"RESULT STATE REJECT_WAIT {msg}".rstrip())
            self.state = "initialized"

    # ---------------------------
    # Protocol command handlers
    # ---------------------------

    def _handle_start(self) -> None:
        if self.state == "initialized":
            if self.on_start:
                self.on_start()
            self.send("STARTED")
            self.state = "started"
        else:
            self.send(f"ERROR JSV script got START command but is in state {self.state}")

    def _handle_begin(self) -> None:
        if self.state == "started":
            self.state = "verifying"
            # Call verify policy, as Tcl include does
            if self.on_verify:
                self.on_verify()
            else:
                self.accept()
            # Then clear for next job cycle
            self.clear_params()
            self.clear_envs()
        else:
            self.send(f"ERROR JSV script got BEGIN command but is in state {self.state}")

    # ---------------------------
    # Main loop
    # ---------------------------

    def main(self) -> None:
        self._script_log(f"{sys.argv[0]} started on {time.ctime()}")
        self._script_log("")

        while not self.quit_flag:
            line = sys.stdin.readline()
            if not line:
                break

            line = line.rstrip("\n")
            if not line:
                continue

            self._script_log(f">>> {line}")
            parts = line.split()
            cmd = parts[0]

            if cmd == "QUIT":
                self.quit_flag = True

            elif cmd == "START":
                self._handle_start()

            elif cmd == "BEGIN":
                self._handle_begin()

            elif cmd == "PARAM":
                if self.state == "started":
                    if len(parts) < 2:
                        self.send("ERROR JSV script got PARAM command without key")
                        continue
                    key = parts[1]
                    value = " ".join(parts[2:]) if len(parts) > 2 else ""
                    self.params[key] = value
                else:
                    self.send(f"ERROR JSV script got PARAM command but is in state {self.state}")

            elif cmd == "ENV":
                if self.state == "started" and len(parts) >= 4 and parts[1] == "ADD":
                    key = parts[2]
                    value = " ".join(parts[3:])
                    self.envs[key] = value
                    self.all_envs.add(key)
                else:
                    self.send(f"ERROR JSV script got ENV command but is in state {self.state}")

            elif cmd == "SHOW":
                for k, v in self.params.items():
                    self.log_info(f"got param {k}={v}")
                for k, v in self.envs.items():
                    self.log_info(f"got env {k}={v}")

            else:
                self.send(f'ERROR JSV script got unknown command "{cmd}"')

        self._script_log(f"{sys.argv[0]} terminating on {time.ctime()}")

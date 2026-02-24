#!/usr/bin/env Rscript

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

# ============================================================
# Installation script of clustermq package for R
# ============================================================

rver <- paste(
  R.version$major,
  strsplit(R.version$minor, "\\.")[[1]][1],
  sep = "."
)

platform <- R.version$platform
libdir <- file.path("~", "R", platform, "library", rver)


userlib <- Sys.getenv("R_LIBS_USER")
if (userlib == "") {
  userlib <- file.path("~", "R",
                       paste0(R.version$platform, "-library"),
                       paste(R.version$major, strsplit(R.version$minor, "\\.")[[1]][1], sep = "."))
  Sys.setenv(R_LIBS_USER = userlib)
}

dir.create(userlib, recursive = TRUE, showWarnings = FALSE)
.libPaths(c(userlib, .libPaths()))

cat("Using library:", normalizePath(userlib), "\n")
install.packages("remotes")

# Print to stdout
cat("R version (major.minor):", rver, "\n")
cat("Platform:", platform, "\n")
cat("Library directory:", normalizePath(userlib), "\n")

install.packages("remotes")
remotes::install_github("ernst-bablick/clustermq")

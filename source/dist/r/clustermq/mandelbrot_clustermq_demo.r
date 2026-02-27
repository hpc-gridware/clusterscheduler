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
# Mandelbrot HPC Demo using clustermq with OCS/GCS)
# Output:
#   - mandelbrot_tiles/*.rds  (per-tile data)
#   - mandelbrot_color.png    (final image on MASTER working dir)
# ============================================================

suppressPackageStartupMessages(library(clustermq))

# ---- Select scheduler backend ----
options(clustermq.scheduler = "gcs")  # change to "ocs" if desired

# ---- Parameters ----
width    <- 2560   # adapt for your screen resolution
height   <- 1600
max_iter <- 3000   # increase for nicer gradients

# Common section
#xmin <- -2.2; xmax <- 1.0
#ymin <- -1.2; ymax <- 1.2

# More interesting area and also more compute intensice
xmin <- -0.74877; xmax <- -0.74872
ymin <-  0.06505; ymax <-  0.06510

tiles_x <- 16
tiles_y <- 10

workers <- 4

outdir <- "mandelbrot_tiles"
dir.create(outdir, showWarnings = FALSE, recursive = TRUE)

compute_tile <- function(tx, ty,
                         tiles_x, tiles_y,
                         width, height,
                         xmin, xmax, ymin, ymax,
                         max_iter,
                         outdir) {

  tile_id <- function(tx, ty) sprintf("tile_%02d_%02d", tx, ty)

  tile_to_pixels <- function(tx, ty, tiles_x, tiles_y, width, height) {
    x0 <- floor((tx - 1) * width  / tiles_x) + 1
    x1 <- floor(tx * width / tiles_x)
    y0 <- floor((ty - 1) * height / tiles_y) + 1
    y1 <- floor(ty * height / tiles_y)
    list(x0 = x0, x1 = x1, y0 = y0, y1 = y1)
  }

  hostname <- Sys.info()[["nodename"]]
  pid <- Sys.getpid()

  px <- tile_to_pixels(tx, ty, tiles_x, tiles_y, width, height)

  xs <- seq(xmin, xmax, length.out = width)[px$x0:px$x1]
  ys <- seq(ymin, ymax, length.out = height)[px$y0:px$y1]

  iter <- matrix(0L, nrow = length(ys), ncol = length(xs))

  t0 <- proc.time()[3]
  for (iy in seq_along(ys)) {
    cy <- ys[iy]
    for (ix in seq_along(xs)) {
      cx <- xs[ix]
      zr <- 0.0; zi <- 0.0
      k <- 0L
      while ((zr*zr + zi*zi) <= 4.0 && k < max_iter) {
        zr_new <- zr*zr - zi*zi + cx
        zi_new <- 2.0*zr*zi + cy
        zr <- zr_new; zi <- zi_new
        k <- k + 1L
      }
      iter[iy, ix] <- k
    }
  }
  t1 <- proc.time()[3]

  fn <- file.path(outdir, paste0(tile_id(tx, ty), ".rds"))
  saveRDS(list(
    tx = tx, ty = ty,
    x0 = px$x0, x1 = px$x1, y0 = px$y0, y1 = px$y1,
    iter = iter,
    host = hostname,
    pid  = pid,
    seconds = as.numeric(t1 - t0)
  ), fn)

  list(tx = tx, ty = ty, file = fn, host = hostname, pid = pid, seconds = as.numeric(t1 - t0))
}

grid <- expand.grid(tx = seq_len(tiles_x), ty = seq_len(tiles_y))

message("Submitting ", nrow(grid), " tiles using ", workers, " workers...")

meta <- Q(
  compute_tile,
  tx = grid$tx,
  ty = grid$ty,
  const = list(
    tiles_x = tiles_x, tiles_y = tiles_y,
    width = width, height = height,
    xmin = xmin, xmax = xmax, ymin = ymin, ymax = ymax,
    max_iter = max_iter,
    outdir = outdir
  ),
  n_jobs = workers,
  memory = "500",
  template=list(
    threads = 2  # add other template parameter here
  )
  #,max_calls_worker = 45
)

message("Reading tiles and assembling final image...")
message("Master working directory: ", getwd())

img <- matrix(0L, nrow = height, ncol = width)
for (m in meta) {
  t <- readRDS(m$file)
  img[t$y0:t$y1, t$x0:t$x1] <- t$iter
}

# ---- Colorful rendering (robust) ----
palette_len <- 512

# A vibrant palette available in base R (no extra packages):
# Spectral gives rich reds/blues/yellows
pal <- grDevices::hcl.colors(palette_len, palette = "Spectral", rev = TRUE)

# Non-linear mapping to spread colors across the image:
# - Use log scaling to enhance gradients
# - Keep the inside of the set (iter == max_iter) black for contrast
scaled <- log1p(img) / log1p(max_iter)
idx <- 1 + floor(scaled * (palette_len - 1))
idx[idx < 1] <- 1
idx[idx > palette_len] <- palette_len
idx <- matrix(idx, nrow = height, ncol = width)

col_mat <- matrix(pal[idx], nrow = height, ncol = width)
col_mat[img >= max_iter] <- "#000000"  # interior as black
col_mat <- col_mat[height:1, , drop = FALSE]  # flip vertically

outfile <- "mandelbrot.png"
grDevices::png(outfile, width = width, height = height)
op <- par(mar = c(0, 0, 0, 0))
plot.new()
rasterImage(as.raster(col_mat), 0, 0, 1, 1, interpolate = FALSE)
par(op)
dev.off()

message("Wrote: ", normalizePath(outfile))

# If running in RStudio, also show it in the Plots pane
is_rstudio <- function() {
  nzchar(Sys.getenv("RSTUDIO")) || nzchar(Sys.getenv("RSTUDIO_SESSION_PORT"))
}

if (is_rstudio()) {
  system2("xdg-open", normalizePath(outfile), wait = FALSE)
}

# ---- Summary ----
df <- do.call(rbind, lapply(meta, as.data.frame))
df <- df[order(df$seconds, decreasing = TRUE), ]
rownames(df) <- NULL

cat("\nTile execution summary (slowest first):\n")
print(df[, c("tx", "ty", "host", "pid", "seconds")], row.names = FALSE)

cat("\nHost distribution:\n")
print(sort(table(df$host), decreasing = TRUE))

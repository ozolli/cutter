# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Cutter is a linear cutting optimizer for Linux - a standalone alternative to SmartCut.Pro. It optimizes cutting of bars, boards, and tubes to minimize waste. It is a GTK4 desktop application built on a standalone solver core, and keeps a persistent stock/offcut inventory in SQLite. (A separate benchmark tool reuses the same core.)

## Build Commands

```bash
make              # Build the GUI (default target) -> bin/cutter-gtk
make gui          # Same as `make all`
make benchmark    # Build the benchmark tool       -> bin/cutter-benchmark
make clean        # Clean build artifacts
make install      # Install the GUI (PREFIX=/usr/local)
./bin/cutter-gtk  # Run the app
```

## Dependencies

```bash
sudo apt install libglpk-dev     # GLPK (GNU Linear Programming Kit) - LP/ILP solver
sudo apt install libcairo2-dev   # Cairo  - PDF export
sudo apt install libsqlite3-dev  # SQLite - inventory database
sudo apt install libgtk-4-dev    # GTK4   - desktop GUI
```

## Usage

`cutter-gtk` is a desktop app with views for the inventory, the pieces to cut,
the optimization run, the results (with PDF export) and the settings. The stock
parameters (saw kerf, minimum usable offcut, database path, network mode) are
configured in the Settings view and persisted to `~/.cutter/settings.conf`.

There is no command-line interface: a previous `cutter` CLI was removed. The
solver core (CSV/ODS import, column generation, PDF export, SQLite inventory) is
linked directly into the GUI.

## Architecture

**Language**: C (C11 standard)

**Core Algorithm**: Column Generation with GLPK for optimal cutting stock solutions.

```
src/
├── csp_types.h      # Core data structures (PieceDemand, StockBar, CuttingPattern,
│                    #   CSPInstance, CSPSolution) + inline helpers
├── knapsack.h/c     # Bounded knapsack (DP, binary decomposition) for the pricing subproblem
├── glpk_master.h/c  # GLPK interface for the master LP/ILP
├── colgen.h/c       # Column generation orchestration + feasibility check + post-processing
├── csv_io.h/c       # CSV import/export (incl. Odoo-format piece lists)
├── ods_io.h/c       # ODS (OpenDocument spreadsheet) import
├── db.h/c           # SQLite inventory: stock, offcuts, cutting sessions
├── settings.h/c     # App settings (kerf, min offcut, db path) loaded from config
├── pdf_export.h/c   # PDF diagram + stock list export (Cairo)
├── benchmark.c      # Standalone benchmark harness (reuses the core)
├── gui_main.c       # GUI entry point (main)
└── gui/             # GTK4 desktop app (cutter_app, cutter_window,
                     #   views/, models/) — shares the solver core
```

The solver core (`colgen`, `glpk_master`, `knapsack`, `csv_io`, `ods_io`, `pdf_export`,
`db`, `settings`) is compiled into the GUI (`bin/cutter-gtk`) and the benchmark
(`bin/cutter-benchmark`). The GUI sources live under `src/gui/` and are built by the
default `gui` target.

**Algorithm Flow**:
0. Greedy offcut pre-pass: consume offcuts (chutes) from SMALLEST to LARGEST,
   before any fresh bar, filling each as much as possible. Pieces covered this way
   are removed from the demand the master must satisfy. Offcuts are excluded from
   the LP/ILP; fresh bars handle the remaining demand.
1. Generate initial patterns (varying quantities of a single piece type per fresh bar)
2. Solve the master LP relaxation → get dual prices
3. Solve the pricing subproblem (one knapsack per fresh stock type) → find an improving pattern
4. If reduced cost is negative: add the pattern, go to step 2
5. Solve the final ILP (demands fixed to equality) for the integer solution
6. Post-process: greedily drop redundant fresh-bar usage so production matches demand exactly
   (offcut patterns are never dropped)
7. Consolidate ("panachage"): relocate pieces from poorly-filled fresh bars into the spare room
   of other compatible fresh bars (best-fit), retiring whole bars. This closes the column-
   generation integer gap (the ILP may leave a near-empty bar because the ideal mixed column was
   never generated) and lets different "boats" share one bar. It only lowers the bar count and
   preserves total production; offcut bars are left untouched. See `consolidate_fresh_bars()`.

## Key Parameters

- `saw_kerf`: Saw blade width (default 5mm) - consumed after each cut
- `min_usable_offcut`: Minimum useful offcut threshold (default 100mm)
- Knapsack uses 1mm discretization (`PRECISION` in `knapsack.c`)

## Development Notes

- French-language project (interface messages in French; keep new user-facing strings French)
- `CSPInstance` is large (~16 MB, due to the static `patterns`/`cuts` arrays). **Always
  heap-allocate it** (`calloc`) — declaring it on the stack overflows the default stack and
  segfaults. The GUI and benchmark both heap-allocate it.
- GLPK handles LP/ILP solving; artificial variables guarantee initial feasibility and signal
  true infeasibility if still used after the ILP solve.
- The knapsack recovers patterns via a compact per-(item,capacity) decision trace rather than
  copying a full pattern array per DP cell.
- Patterns are generated dynamically during column generation (capped at `MAX_PATTERNS`).
- The inventory lives in a SQLite database whose path comes from settings (`db_path`).
- Network-share databases (Synology SMB, NAS, NFS): set `db_network=1` in
  `settings.conf` (or tick "Base sur le reseau" in the GUI settings). This opens
  SQLite with the `unix-dotfile` VFS (`db_set_network_mode`), which uses a
  `<db>.lock` file instead of POSIX byte-range locks — the latter are unreliable
  on SMB/NFS and silently block writes. WAL mode is never used (it can't work on a
  network share). Concurrent writers are coordinated by the lock file, but a hard
  crash can leave a stale `<db>.lock` to remove by hand. Alternative (no app
  change): mount the share with the `nobrl` option.
- Offcut priority is a deliberate workshop rule (consume scrap smallest-first before
  buying/cutting new bars). It is implemented by the greedy pre-pass in
  `consume_offcuts_greedy()`, driven by `StockBar.is_offcut` (set in
  `db_load_inventory`), NOT by the cost objective — adding an unneeded small offcut
  always raises total cost, so cost minimisation alone cannot express it.

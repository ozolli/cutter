# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Cutter is a linear cutting optimizer for Linux - a standalone alternative to SmartCut.Pro. It optimizes cutting of bars, boards, and tubes to minimize waste. It ships as both a command-line tool and a GTK4 desktop application, sharing a common solver core, and keeps a persistent stock/offcut inventory in SQLite.

## Build Commands

```bash
make              # Build the GUI (default target)
make gui          # Build the GTK4 GUI       -> bin/cutter-gtk
make cli          # Build the CLI            -> bin/cutter
make benchmark    # Build the benchmark tool -> bin/cutter-benchmark
make demo         # Build and run the CLI demo
make clean        # Clean build artifacts
make install      # Install the CLI   (PREFIX=/usr/local)
make install-gui  # Install the GUI
./bin/cutter --help
```

## Dependencies

```bash
sudo apt install libglpk-dev     # GLPK (GNU Linear Programming Kit) - LP/ILP solver
sudo apt install libcairo2-dev   # Cairo  - PDF export
sudo apt install libsqlite3-dev  # SQLite - inventory database
sudo apt install libgtk-4-dev    # GTK4   - desktop GUI (not needed for the CLI)
```

## Usage (CLI)

The CLI is organized into subcommands:

```bash
# Demonstration
./bin/cutter --demo
./bin/cutter --demo --pdf plan.pdf

# Run a cutting session
./bin/cutter cut -p pieces.csv -s stock.csv -P plan.pdf -v
./bin/cutter cut -p pieces.csv --use-inventory --apply --save-offcuts

# Manage the stock inventory (SQLite)
./bin/cutter stock add Tube_6m 6000 --diameter 50 --thickness 3 --qty 10
./bin/cutter stock list [--all]
./bin/cutter stock remove <id> [--qty N]
./bin/cutter stock delete <id>
```

`cut` options:
- `-p, --pieces FILE`    CSV of pieces to cut (required)
- `-s, --stock FILE`     CSV of stock (or use `--use-inventory`)
- `-I, --use-inventory`  Use stock from the inventory database
- `-A, --apply`          Apply the cut to the inventory (decrement stock)
- `-S, --save-offcuts`   Save usable offcuts back into the inventory
- `-P, --pdf FILE`       Export the cutting diagram as PDF
- `-k, --kerf MM`        Saw kerf in mm (default from config)
- `-m, --min-offcut MM`  Minimum usable offcut length (default from config)
- `-v, --verbose`        Show progress (also enables solver debug output)

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
├── main.c           # CLI entry point (cut / stock / --demo)
├── benchmark.c      # Standalone benchmark harness
└── gui/             # GTK4 desktop app (cutter_app, cutter_window,
                     #   views/, models/) — shares the solver core
```

The solver core (`colgen`, `glpk_master`, `knapsack`, `csv_io`, `ods_io`, `pdf_export`,
`db`, `settings`) is compiled into both the CLI and the GUI. The GUI sources live under
`src/gui/` and are only built by the `gui` target.

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

## Key Parameters

- `saw_kerf`: Saw blade width (default 5mm) - consumed after each cut
- `min_usable_offcut`: Minimum useful offcut threshold (default 100mm)
- Knapsack uses 1mm discretization (`PRECISION` in `knapsack.c`)

## Development Notes

- French-language project (interface messages in French; keep new user-facing strings French)
- `CSPInstance` is large (~16 MB, due to the static `patterns`/`cuts` arrays). **Always
  heap-allocate it** (`calloc`) — declaring it on the stack overflows the default stack and
  segfaults. The CLI, GUI, and benchmark all heap-allocate it.
- GLPK handles LP/ILP solving; artificial variables guarantee initial feasibility and signal
  true infeasibility if still used after the ILP solve.
- The knapsack recovers patterns via a compact per-(item,capacity) decision trace rather than
  copying a full pattern array per DP cell.
- Patterns are generated dynamically during column generation (capped at `MAX_PATTERNS`).
- The inventory lives in a SQLite database whose path comes from settings (`db_path`).
- Offcut priority is a deliberate workshop rule (consume scrap smallest-first before
  buying/cutting new bars). It is implemented by the greedy pre-pass in
  `consume_offcuts_greedy()`, driven by `StockBar.is_offcut` (set in
  `db_load_inventory`), NOT by the cost objective — adding an unneeded small offcut
  always raises total cost, so cost minimisation alone cannot express it.

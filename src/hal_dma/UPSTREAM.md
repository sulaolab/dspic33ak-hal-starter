# Provenance

**The vendoring direction is reversed.** This directory used to be a vendored copy of
the standalone DMA HAL repository; since the NORA-HAL migration it is the other way
round:

    upstream audio project  ->  this starter  ->  https://github.com/sulaolab/nora-hal-dspic33ak-dma

These bytes come from the audio project that runs the code on silicon, and the
standalone repository is a **published snapshot** of this directory: every source file
under its `src/` is verified blob-identical to this folder, not the reverse (measured
2026-08-09; all four `.c`/`.h` match across the audio project, this starter, and the
snapshot). This file is the one exception — `UPSTREAM.md` is starter-local packaging and
is deliberately not published.

A reusable DMA change therefore belongs in the upstream project first and reaches this
starter by re-sync; publication follows from here. A fix made only in the standalone
repository would be a fork of validated code.

# Provenance

**The vendoring direction is reversed.** This directory used to be a vendored copy of
the standalone DMA HAL repository; since the NORA-HAL migration it is the other way
round:

    upstream audio project  ->  this starter  ->  https://github.com/sulaolab/nora-hal-dspic33ak-dma

These bytes come from the audio project that runs the code on silicon, and the
standalone repository is a **published snapshot** of this directory: its `src/` is
verified blob-identical to this folder, not the reverse.

A reusable DMA change therefore belongs in the upstream project first and reaches this
starter by re-sync; publication follows from here. A fix made only in the standalone
repository would be a fork of validated code.

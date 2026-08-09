# Provenance

**The vendoring direction is reversed.** This directory used to be a vendored copy of
the standalone SPI/I2S/TDM HAL repository; since the NORA-HAL migration it is the other
way round:

    upstream audio project  ->  this starter  ->  https://github.com/sulaolab/nora-hal-dspic33ak-spi-i2s-tdm

These bytes come from the audio project that runs the code on silicon, and the
standalone repository is a **published snapshot** of this directory: its `src/` is
verified blob-identical to this folder, not the reverse.

A reusable transport change therefore belongs in the upstream project first and reaches
this starter by re-sync; publication follows from here. A fix made only in the standalone
repository would be a fork of validated code.

One documented exception to that identity: `README.md` in this folder is de-codenamed for
publication and omits an upstream-only section, so it is deliberately *not* byte-identical
to the upstream project's copy. Every source file is. (`hal_timer/README.md` diverges the
same way.)

# Provenance

**The vendoring direction is reversed.** This directory used to be a vendored copy of
the standalone SPI/I2S/TDM HAL repository; since the NORA-HAL migration it is the other
way round:

    upstream audio project  ->  this starter  ->  https://github.com/sulaolab/nora-hal-dspic33ak-spi-i2s-tdm

These bytes come from the audio project that runs the code on silicon, and the
standalone repository is a **published snapshot** of this directory: every source file
under its `src/` is verified blob-identical to this folder, not the reverse (measured
2026-08-09; all ten `.c`/`.h` plus `nora_spi_i2s_tdm_conf.h_example` match across the
audio project, this starter, and the snapshot).

A reusable transport change therefore belongs in the upstream project first and reaches
this starter by re-sync; publication follows from here. A fix made only in the standalone
repository would be a fork of validated code.

Two documented exceptions to that identity, both non-source:

* `README.md` in this folder is de-codenamed for publication and omits an upstream-only
  section (§10, the canonical-API / no-portable-facade record, which cites `docs_internal/`
  material that does not exist outside the audio project). It is therefore deliberately
  *not* byte-identical to the upstream copy; the starter and the snapshot agree with each
  other (`636bac87`), the upstream copy is `09f38580`.
* `UPSTREAM.md` — this file — is starter-local packaging and is deliberately not published.

`hal_timer/README.md` was named here as diverging the same way; **it no longer does.**
Re-measured 2026-08-09 it is `d5cf2a24` in all three trees. The de-codenaming that made it
differ was carried upstream, so the only module README still deliberately divergent is this
one.

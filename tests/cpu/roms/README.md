# 6502 test ROMs

## 6502_functional_test.bin
- Source: Klaus2m5/6502_65C02_functional_tests, `bin_files/6502_functional_test.bin`
- Pinned commit: 7954e2dbb49c469ea286070bf46cdd71aeb29e4b
- License: GPL-3.0 (test artifact; NOT shipped in the Nessy plugin binary)
- Load address: $0000 (image spans $0000-$FFFF)
- Entry point: $0400
- Success: executes to a self-loop ("trap") at **$3469** for this standard build.
  A trap at any other address indicates a failed sub-test (the address encodes which).
  If a differently-assembled ROM is used, read its `.lst` for the success label address.
- This build has decimal-mode tests ENABLED; the km6502 harness compiles with decimal enabled to match.

# Implementation blockers and documentation gaps

This ledger records facts required by the C17 implementation that are not
fully specified by the completed Yankee Trader 3.6G analysis. Do not close a
gap by disassembling binaries or performing new reverse engineering in this
repository. Implementation stops at the affected boundary until the upstream
documentation supplies the missing contract.

## Open documentation gaps

### DOC-GAP-001: sequential character-device `PRINT` adapter

Affected coverage:

- BRUN regular-file `PRINT`/`CLOSE` and shared CLOSE-all registry
- BRUN regular-file sequential `OUTPUT OPEN`

The existing analysis specifies the one-byte device staging buffer, 24-bit
output index, column updates, statement-completion flush, and broad success,
short, carry, and carrier-result classes. It does not completely specify how
that state projects onto the implementation's physical write-provider
boundary. The implementation still needs authoritative documentation for:

- the number and order of physical writes for a multi-byte `PRINT` statement;
- the requested byte count and buffer contents of each write;
- how a carry-clear zero-byte result affects the offered byte, pending byte,
  index, and column;
- which state is retained when a value write or the completion flush fails;
  and
- the device-code/newline distinction required by rooted callers.

No character-device implementation change is authorized until that contract
is documented.

### DOC-GAP-002: `YTMAINT` radio-compaction file transaction

Affected coverage:

- YTMAINT message compaction and newspaper rotation
- `YTRMSG.DAT` radio store/compaction

The existing analysis specifies random files `#4` (`temp`) and `#5`
(`YTRMSG.DAT`), 86-byte records, the 4/4/4/72 FIELD layouts, nonzero-record
retention, full-record `PUT`s, and deterministic zero bytes at offsets 84 and
85. It does not completely specify:

- whether opening a pre-existing `temp` preserves or truncates its bytes;
- whether fewer retained records leave trailing records from a pre-existing
  `temp`;
- whether destination `PUT`s use implicit record advancement or explicit
  record numbers; and
- the exact close order for destination file `#4` and source file `#5`.

The current C owner still uses raw `fopen`/`fwrite` for the destination. It
must not be replaced with a guessed typed-random transaction until these facts
are documented.

## Explicitly deferred work

These are authorized deferrals rather than missing-analysis gaps:

- exact `LOCAL.EXE` console/menu/DORINFO/SHELL/local-framebuffer
  compatibility; and
- platform-specific OpenDoors startup, exit, and post-OPEN wait behavior.


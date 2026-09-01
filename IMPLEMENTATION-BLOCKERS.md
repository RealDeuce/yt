# Implementation blockers and documentation gaps

This ledger records facts required by the C17 implementation that are not
fully specified by the completed Yankee Trader 3.6G analysis. Do not close a
gap by disassembling binaries or performing new reverse engineering in this
repository. Implementation stops at the affected boundary until the upstream
documentation supplies the missing contract.

## Open documentation gaps

### DOC-GAP-007: command-7 later-poll AB36 composition is left open

Affected coverage:

- ship-computer command-7 avoid-list slot editor; and
- its sector replacement editor.

The completed `docs/runtime/computer-avoid-output.md` contract supplies the
ordinary successful editor shortcut and retains the five canonical terminal
outcomes for an explicit terminal poll. It then expressly leaves later
inactivity/session checks, live per-poll time refresh, post-printable carrier
loss, asynchronous F8 delivery, and carrier/F8 joins inside Ctrl-R and
save/repeat helpers as a caller-level gap.

The shared AB36 machinery models those behaviors, but the command-7 contract
does not yet state that joining the canonical global state at its two real
editor entries is sufficient, nor does it provide caller-specific state
vectors for the omitted interleavings. Native work may compose the documented
ordinary and explicit terminal-poll boundaries, but must not claim all
command-7 editor paths. Upstream documentation must either define the two
shared-state joins as the complete caller contract or supply the missing
caller-specific transitions. No binary inspection or new reverse engineering
was performed.

### DOC-GAP-006: command-2 dependency failures lack error-router identity

Affected coverage:

- ship-computer command-2/alias-23 selected-sector and friendship reads;
- its ordinary-port updater/report child; and
- its Earth-report child.

The completed contracts specify each child's exact already-emitted prefix,
FIELD residue, and persistence state, and the shared main error contract
specifies routing once BASIC `ERR`, `ERL`, and the retry statement are known.
They do not connect those two sides for command 2:

- `docs/runtime/computer-port-report-output.md` leaves physical random-file
  errors and shared runtime-error suffixes outside its bounded composition;
- `docs/runtime/port-commerce-output.md` lists the ordered failing stages but
  supplies no caller statement `ERL` or retry address; and
- `docs/runtime/earth-store-output.md` says the shared handler owns the
  `ERR`/`ERL`-dependent suffix without supplying that dependency identity.

The native `struct yt_error` consequently cannot be extended or populated at
these sites from existing evidence: the required BASIC error number, source
line, saved/retry statement identity, active handler, and the exact distinction
between retry and terminal routing are unspecified for each cut. The native
implementation stops at the already-pinned 40/87/100/102-byte pre-router
prefixes. Do not infer the missing values from host errors or inspect binaries;
upstream documentation must provide the per-stage error-router projection.

## Resolved documentation gaps

### DOC-GAP-005: command-2 surviving AB36 editor interleavings

Affected coverage:

- ship-computer command-2/alias-23 first selector and retry selectors; and
- the command-2 return into the fresh `YT:8639` computer prompt.

Resolved by the user's authoritative clarification. The shared AB36 contract
already parameterizes per-poll time refresh, local/serial/queue precedence,
typeahead residue, F8 delivery and resumed polling, and every timeout,
carrier-loss and terminal outcome. Command 2 does not require a separate
caller-specific vector for every permutation. It only joins its canonical
global state into that shared editor at the initial selector, the retry after
`02DB` clears the queue, and the fresh `8639` computer prompt. The upstream
`docs/runtime/computer-port-report-output.md:290` disclaimer was therefore a
documentation scoping defect, not missing behavioral analysis. No binary
inspection or new reverse engineering was performed.

### DOC-GAP-004: forced-Bribe surrender join raw/typed output conflict

Affected coverage:

- hostile Bribe forced-combat continuation;
- hostile Attack surrender-prefix composition; and
- completion claims that depend on the joined forced-Bribe combat oracle.

Resolved upstream by commit `35039c3e`. The joined fixture now carries the
actual COM1 device class `FCh` through
`compose_hostile_attack_surrender_prefix_world()`, so its raw and typed
direct-output serial states agree. This removes the erroneous additional LF:
the corrected fixture explicitly rejects `CR LF LF`. The formerly failing
focused test
`tests.test_transducer_join.TransducerJoinTests.test_hostile_bribe_forced_attack_composes_first_combat_region`
passes in isolation. This was a caller-composition/state-carrier defect, not
missing combat or fighter/shield-spill analysis.

### DOC-GAP-003: unshielded sector-mine missile RNG in joined fatal contracts

Affected coverage:

- anonymous sector-mine body and its caller continuations;
- direct fighter-kill raw FIELD/store/news/RNG composition; and
- direct fighter destroyed-to-fatal raw transaction.

The upstream contracts disagreed about the positive-missile branch of an
unshielded sector-mine batch:

- `docs/runtime/sector-mine-output.md` specifies one direct missile `RND`,
  identifies site `YT-SUB:679B`, and agrees with
  `tools/ytmine_output.py`, which consumes
  `batch<N>.missiles` once and applies
  `INT(RND * (batch * missiles)) + 1`.
- `docs/runtime/direct-fighter-fatal-cycle-output.md` instead lists
  `missile TIMER/RANDOMIZE/RND x3` in its joined RNG order.

Resolved by the user's authoritative clarification: the fatal-cycle sentence
is a prose error. An unshielded nonzero missile field consumes exactly one
direct `RND` at `YT-SUB:679B`, then computes
`INT(RND * (batch * missiles)) + 1` and caps the result to the current missile
stock. The three-step TIMER/RANDOMIZE/RND shrink helper applies to fighters,
carried mines, commodities, and empty holds, not missiles.

The retained direct-fighter fatal fixture has zero missiles, so its generated
artifact, 13-draw trace, and 1,597/1,900-byte transcripts are unchanged. The
native `mine_encounter()` follows the correct component model, and its
provider-driven missile step now pins exact-zero suppression, one-call
cardinality, capped arithmetic, and the single failure boundary explicitly.

### DOC-GAP-001: sequential character-device `PRINT` adapter

Affected coverage:

- BRUN regular-file `PRINT`/`CLOSE` and shared CLOSE-all registry
- BRUN regular-file sequential `OUTPUT OPEN`

Resolved upstream by commit `c08f1fca` in
`docs/runtime/brun-print-transducer.md`. The contract now specifies:

- one ordered one-byte physical write per logical byte, with the final pending
  byte offered by statement completion;
- accepted and dropped clear-carry results, including the compatibility-bit
  distinction;
- exact pending/index/column residue at value-write and completion failures;
- the absence of a synthetic carrier-loss edge; and
- the device-code newline distinctions used by rooted callers.

The shared projection is implemented by `yt_text_device_print()` with focused
per-write provider tests. Raw allocator/registration cleanup remains a mapped
implementation obligation, not a documentation gap.

### DOC-GAP-002: `YTMAINT` radio-compaction file transaction

Affected coverage:

- YTMAINT message compaction and newspaper rotation
- `YTRMSG.DAT` radio store/compaction

Resolved upstream by commit `c08f1fca` in
`docs/file-formats/YTRMSG.md`. The contract now specifies:

- the preliminary OUTPUT/CLOSE/KILL sequence that removes an old `temp`;
- a fresh RANDOM destination with the 4/4/4/72 FIELD layout;
- explicit source `GET` and dense destination `PUT` record numbers; and
- source `#5`, then destination `#4`, then KILL, then NAME ordering.

The documented transaction is implemented by `yt_radio_compact()`.

## Explicitly deferred work

These are authorized deferrals rather than missing-analysis gaps:

- exact `LOCAL.EXE` console/menu/DORINFO/SHELL/local-framebuffer
  compatibility; and
- platform-specific OpenDoors startup, exit, and post-OPEN wait behavior.

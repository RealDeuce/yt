# Implementation blockers and documentation gaps

This ledger records facts required by the C17 implementation that are not
fully specified by the completed Yankee Trader 3.6G analysis. Do not close a
gap by disassembling binaries or performing new reverse engineering in this
repository. Implementation stops at the affected boundary until the upstream
documentation supplies the missing contract.

## Open documentation gaps

### DOC-GAP-020: corrupt color-table adjacent reads

Affected coverage:

- corrupt logical-color indices whose direct `CINT(color) * 4 + base`
  calculation reads adjacent process storage.

Correction to the original entry: the completed global-state registry already
names the initialization flag at `DS:556A` and the 32-byte table at
`DS:556E`; `docs/runtime/presentation-helpers.md` supplies the eight ordinary
values. The native ordinary table is therefore process-bound and no upstream
work is needed for indices 0 through 7.

The remaining gap is the explicitly unbounded corrupt-index domain. The
registry identifies some following roots (`DS:5596/559A` string descriptors
and `DS:559E/55A2` caches), but does not identify the intervening cells at
`DS:558E/5592`, provide a complete wrapped-address/alias contract, or pin the
raw reads and resulting `CINT`/`COLOR` behavior for the reached corrupt cases.
The native implementation retains its protective range result outside 0
through 7 rather than inventing those outcomes. Upstream documentation,
generated evidence, and focused fixtures must specify that corrupt domain
before the guard can be removed. No binary inspection or new reverse
engineering was performed.

### DOC-GAP-019: planet-updater corrupt numeric record behavior

Affected coverage:

- `YT-SUB2:0A97..0FD5` planet-production updates over edited or corrupt
  planet records;
- exact raw MBF arithmetic, persistence, and failure residue for dirty-zero
  and negative numeric fields; and
- every scanner, planet-menu, computer-report, projectile, and maintenance
  caller that can pass such a record to the shared updater.

The completed `docs/gameplay/planet-economy.md` explicitly leaves overflow
and error behavior for deliberately corrupt numeric records outside its
result. The native `updater_validate_record()` currently stops with
protective `YT_RANGE` results for dirty exponent-zero values in selected
fields and for every negative nonzero field. Those guards prevent unsafe or
unsupported host arithmetic, but they are not an evidence-backed claim about
the shipped BASIC transition, ERR/ERL identity, partial process/FIELD state,
or whether a write is reached.

Upstream documentation, the canonical raw model, generated evidence, and
focused fixtures must specify these corrupt domains before the guards can be
replaced or classified as exact behavior. No binary inspection or new
reverse engineering was performed.

### DOC-GAP-018: plasma wait destination roots

Affected coverage:

- the two one-second plasma prelaunch waits;
- every half-second per-hop plasma route wait; and
- their process-image and dependency-failure residue across the plasma
  resolver.

The completed `docs/runtime/plasma-bolt-output.md` specifies the exact
SINGLE one and half-second values, call order, and that every `94FD` call
overwrites its by-reference duration cell with an absolute deadline. Neither
that document, the generated plasma evidence nor the global-state registry
names the mutable DS destination used by any of those calls. The native
implementation therefore retains typed durations at these boundaries but
cannot bind the correct process roots or claim their failure residue without
inventing addresses. Upstream documentation and generated evidence must name
the caller cells and their reuse/aliasing rules. No binary inspection or new
reverse engineering was performed.

### DOC-GAP-017: radio private-pager wait destination root

Affected coverage:

- the shared radio reader's strictly-greater-than-22 private-pager pause;
- command-6 and automatic post-login radio scans that reach that pause; and
- exact process-image residue on wait and later output/file failures.

The completed `docs/runtime/radio-message-output.md` specifies the 99-second
value, exact `94FD` polling semantics, count reset, and post-wait blank, but
does not identify the caller-owned mutable duration/deadline cell. The
global-state registry has no matching radio private-pager wait root. Existing
`ytradio.static.txt` values describe the separate radio-door 33-second wait,
not this gameplay reader's 99-second caller cell. The native reader can retain
its typed wait callback but cannot make the correct process cell authoritative
without an upstream destination address and alias/lifetime contract. No
binary inspection or new reverse engineering was performed.

### DOC-GAP-016: Xannor-victory wait destination root

Affected coverage:

- Headquarters-victory `[PAUSE]` suffix at `YT-SUB:A83E..A850`;
- authoritative process-image binding of its 99-second duration/deadline
  cell; and
- exact wait-failure residue in the joined direct, missile and plasma
  Headquarters-victory callers.

The completed `docs/runtime/xannor-victory-output.md` specifies that `A83E`
copies MBF32 99 and that `A84B` passes a caller-owned cell by reference to
the shared `94FD` wait, which replaces that same cell with the absolute
deadline. Generated `ythq-victory.static.txt` and
`ythq-victory-output.static.txt` identify `DS:A9D8` as the raw
`00 00 46 87` source value, but neither source identifies the mutable
destination root written by the copy and passed to `94FD`. The global-state
registry also has no Headquarters-victory wait root.

The native implementation can preserve the typed 99-second behavior but
cannot make the correct process cell authoritative or pin its failure
residue without inventing that destination address. Upstream documentation,
the canonical world model, generated evidence and registry must name the
destination root before this boundary can proceed. No binary inspection or
new reverse engineering was performed.

### DOC-GAP-015: zero-Headquarters repair literal conflict

Affected coverage:

- startup configuration hydration/validation at `YT-SUB:BE82..BEA0`;
- authoritative binding of process-global Headquarters cell `DS:4BD8`; and
- every later main-session consumer of the repaired Headquarters value.

The completed prose and generated raw model disagree about the value copied
when configuration offset 117 is numeric zero. The following sources say the
repair value is 85:

- `docs/runtime/startup-config-hydration.md`;
- `docs/runtime/startup-login.md`;
- `docs/gameplay/daily-maintenance.md`;
- `docs/runtime/initialization.md`;
- `docs/runtime/configuration-editor.md`;
- traversal row 10 and `ytstartup-config-hydration.static.txt`; and
- the existing typed C model and fixtures.

However, `tools/ytstartup_config_hydration.py::repair_zero_headquarters_raw`
installs literal bytes `00 40 37 8A` in both record offset 117 and
`DS:4BD8`, and `tests/test_ytstartup_config_hydration.py` explicitly asserts
those bytes. They decode as MBF32 733, not 85. The model's docstring also
says it is applying the `BE82` repair and stops before the physical PUT, so
this is not merely an unrelated shipped nonzero fixture value.

Implementation cannot make `DS:4BD8` authoritative without choosing which
of these mutually exclusive values and failure prefixes are correct. The
uncommitted Headquarters migration was reverted. Upstream documentation,
the canonical raw model, generated evidence, and regression test must be
reconciled before this boundary can proceed. No binary inspection or new
reverse engineering was performed.

### DOC-GAP-014: command-6 dependency failure projections

Affected coverage:

- ship-computer command-6 radio-log scan and private wait;
- its active-computer entry/body/fresh-prompt cycle; and
- completion claims for physical and shared-handler failures.

The completed `docs/runtime/computer-radio-log-output.md` and shared radio
reader, random-file, output, wait, prompt, input, and framebuffer contracts
fully specify the successful complete-record cycle. They compose command
typeahead, the private pager, all endpoint predicates, return hydration, the
fresh prompt, later AB36/F8 activity, and inherited framebuffer state without
requiring command-specific interleaving vectors or a fixed final image.

The command-specific document explicitly leaves unidentified the fatal
`OPEN`/`LOF`/radio-`GET`/name-`GET`/direct-output/private-wait/`CLOSE`
shared-handler suffixes, persistent `ERR 24` retries, partial random-file
records and physical device writes, and entry/return `A41C` anti-cloak
conversion or handler failures. The already-emitted prefix, successful FIELD
order, and ordinary return remain implementable, but native work must not
invent the missing BASIC `ERR`, `ERL`, retry statement, active-handler, or
partial-I/O projection at those cuts. Upstream documentation is required for
those identities. No binary inspection or new reverse engineering was
performed.

## Resolved documentation gaps

### DOC-GAP-013: command-5 fractional TEAM target

Resolved upstream by commit `f3025222`. Every raw nonzero current-player team
ID invokes the shared loader. Its raw inclusive 1..50 comparisons have no
integrality predicate, so 1.75 reaches BRUN random-record conversion and,
with sector offset 51, selects physical record 52. The existing canonical
native loader was correct; the stale radio-composer sentence was not.

### DOC-GAP-012: command-8 dependency failure projections

Resolved upstream by commit `f3025222`. Canonical physical adapter domains
supply partial I/O, parser/file state and BRUN error mapping; ERR 24/57 retry
re-enters the statement over that retained state, and other results enter the
documented shared handler or main ERL-40000 recovery. Command 8 preserves its
entry FIELD. Shared AB36/F8/framebuffer behavior composes normally.

### DOC-GAP-011: command-4 FIELD and dependency failure projections

Resolved upstream by commit `f3025222`. The scoreboard generator leaves the
FIELD image from its last successful cache, player-row or positive-team GET.
A failed return hydration preserves that image and live file state; a
successful current-player GET replaces it. Physical failures and retries use
the canonical adapter and router contracts, while shared input/framebuffer
state remains ordinary composition.

### DOC-GAP-010: navigation error-router identities and corrupt state

Resolved upstream by commit `f3025222`. Command-specific saved IP, retry
address, ERL, installed handler and admitted BRUN error domains are now pinned
for all route-builder and main navigation failure sites. Corrupt route indices
perform the documented 16-bit adjacent or wrapped DS write; predecessor
cycles and FIFO/movement loops retain their real back-edges rather than host
array bounds or finite-transcript normalization.

### DOC-GAP-006: command-2 dependency failure identities

Resolved upstream by commit `f3025222`. The selected-sector, friendship,
ordinary-port and Earth-report failure sites now have their saved IP, retry
statement, ERL, active handler and complete admitted error domains connected
to the existing main/shared routers. Canonical physical adapters supply the
accepted prefix and retained file/FIELD state at each concrete observation.

### DOC-GAP-009: navigation shared AB36 and framebuffer composition

Affected coverage:

- command-10 start and destination editors;
- command-3 destination and confirmation editors;
- route-display `POS(0)` state; and
- inherited local framebuffer/cursor results.

Resolved by the user's authoritative clarification. Navigation injects its
canonical global state at each real shared-editor boundary; the existing AB36
machinery then owns per-poll time refresh, input precedence, queue/typeahead,
F8 resumption, carrier and terminal outcomes. Route display carries the
inherited local column into the existing `POS(0)`/wrap model, and local output
reduces through the canonical inherited-framebuffer transducer. None requires
a caller-specific vector for every interleaving or one fixed initial/final
screen image. These joins remain implementation and verification work. No
binary inspection or new reverse engineering was performed.

### DOC-GAP-008: command-7 inherited framebuffer composition

Affected coverage:

- ship-computer command-7 local screen cells and cursor result; and
- its local F8/editor presentation effects.

Resolved by the user's authoritative clarification. The documented semantic
local event tape, inherited cursor/framebuffer carrier, and canonical
framebuffer transducer define the result compositionally. Exact compatibility
does not require a single fixed initial framebuffer or a separately supplied
final screenshot. The native implementation must carry and reduce that state;
this is not a missing-analysis boundary. No binary inspection or new reverse
engineering was performed.

### DOC-GAP-007: command-7 shared AB36 editor composition

Affected coverage:

- ship-computer command-7 avoid-list slot editor; and
- its sector replacement editor.

Resolved by the user's authoritative clarification. As with resolved
DOC-GAP-005, command 7 supplies canonical global state at its two real AB36
entries. The shared AB36 contract already owns later time refresh, local/
serial/queue precedence, typeahead, asynchronous F8 and resumed polling,
Ctrl-R/save-repeat helper joins, carrier loss, and terminal outcomes. The
caller does not need bespoke vectors for every permutation. No binary
inspection or new reverse engineering was performed.

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

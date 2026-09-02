# Implementation blockers and documentation gaps

This ledger records facts required by the C17 implementation that are not
fully specified by the completed Yankee Trader 3.6G analysis. Do not close a
gap by disassembling binaries or performing new reverse engineering in this
repository. Implementation stops at the affected boundary until the upstream
documentation supplies the missing contract.

## Open documentation gaps

### DOC-GAP-013: command-5 fractional TEAM target contradicts shared loader

Affected coverage:

- ship-computer command-5/alias-59 TEAM target selection;
- the target-to-body YTDATA FIELD and four-slot roster carrier; and
- corrupt/fractional TEAM joined-cycle claims.

The completed command-specific and shared dependency contracts disagree at
the TEAM loader call.  `docs/runtime/radio-composer-output.md` says that only
integral IDs 1 through 50 perform a team `GET` and that other values leave the
four cleared roster slots.  In contrast,
`docs/runtime/team-loader-world.md` says the loader's inclusive numeric range
test does not `CINT`: an in-range fractional ID proceeds through MBF32
addition and the pinned BRUN random-record conversion, with the example
offset 51 plus team 1.75 selecting physical record 52.  The canonical native
loader and its existing fixtures currently implement the latter shared
contract.

The ordinary integral TEAM path remains implementable, as do personal and
ALL targets.  Native work must not claim the corrupt/fractional TEAM join or
silently truncate the caller's raw ID until the upstream radio-composer
contract is reconciled with the shared loader.  No binary inspection or new
reverse engineering was performed.

### DOC-GAP-012: command-8 dependency failure projections

Affected coverage:

- ship-computer command-8 newspaper selector and viewer;
- its active-computer entry/body/fresh-prompt cycle; and
- completion claims for physical and shared-handler failures.

The completed `docs/runtime/computer-newspaper-output.md`, joined file-cycle
contract, and shared viewer/error contracts fully specify ordinary retries,
first-poll editor terminals, selector/viewer carrier cuts, pagination,
Ctrl-X, endpoint modes, missing-file recovery, the successful one-shot
recovery-writer failure witness, and fresh-prompt return. Potentially missing
facts are limited to dependency failure projections: persistent ERR 24/57
retry sequences, faults after a successful viewer open, physical partial
recovery-append I/O, dependency-owned runtime-error suffixes, and
fresh-prompt helper-internal error state.

Per the user's authoritative correction, asynchronous F8, recursive pager
activity, later fresh-editor polls, and inherited framebuffer behavior are
not caller-specific documentation gaps. They compose through the canonical
shared input/pager and inherited-framebuffer transducers at the real command-8
state boundaries. The native implementation must add those joins normally;
it must not demand a separate command-8 vector for every interleaving or one
fixed final screen image. Upstream documentation is required only for the
unidentified physical/error-router projections above. No binary inspection
or new reverse engineering was performed.

### DOC-GAP-011: command-4 FIELD and dependency failure projections

Affected coverage:

- ship-computer command-4 scoreboard selector, generator, and viewer;
- the joined active-computer entry/body/fresh-prompt cycle; and
- completion claims for dependency failures and failed-return FIELD state.

The completed `docs/runtime/computer-scoreboard-output.md`,
`docs/runtime/computer-file-cycles-output.md`, and shared viewer contract
fully specify the ordinary selector/generator/viewer/return cycle, canonical
first-poll editor terminals, carrier cuts, retained-scoreboard paths, and
ordinary missing-file recovery. Potentially missing facts are the physical
or partial generator/viewer I/O projections, dependency-owned shared-handler
suffixes, and the inherited 137-byte FIELD after a command-4 body when the
return player GET fails before replacement.

Per the user's authoritative correction, later AB36 polls, time refresh,
asynchronous F8/recursive pager activity, and inherited framebuffer behavior
compose through their canonical shared transducers at the real command-4
boundaries. They are implementation obligations, not requests for
caller-specific combination vectors or a fixed final framebuffer. Native
work must stop only at the unidentified FIELD/error-state seams above and
must not invent them. No binary inspection or new reverse engineering was
performed.

### DOC-GAP-010: navigation error-router identities

Affected coverage:

- ship-computer command-10 path construction;
- command-3 autopilot construction and engagement; and
- their physical/dependency failure continuations.

The completed path, autopilot, movement, input, queue, and adapter contracts
compose the normal loops, FIFO transitions, movement back-edges, corrupt-state
adapters, and canonical endpoint/framebuffer transformations. Those are
ordinary implementation work and do not require navigation-specific vectors.

The potentially genuine missing facts are the BASIC `ERR`, `ERL`, active
handler, retry statement, and resulting shared-handler suffix for each failed
physical random-record/serial operation and failed conversion, allocation, or
workspace operation. Native work may retain the documented already-emitted
prefix and exact state at each cut, but it must not synthesize the omitted
error route. Upstream documentation must provide those per-stage
error-router identities. No binary inspection or new reverse engineering was
performed.

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

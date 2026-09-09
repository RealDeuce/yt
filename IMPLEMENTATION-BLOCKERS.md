# Implementation blockers and documentation gaps

This ledger records facts required by the C17 implementation that are not
fully specified by the completed Yankee Trader 3.6G analysis. Do not close a
gap by disassembling binaries or performing new reverse engineering in this
repository. Implementation stops at the affected boundary until the upstream
documentation supplies the missing contract.

## Resolved documentation gaps

### DOC-GAP-020: corrupt color-table adjacent reads

Resolved compositionally by native commit `8fb5636`. The published rule is
the complete contract: `CINT(color) * 4 + DS:556E` selects a four-byte MBF32
operand with 16-bit address wrapping and no bounds check. The implementation
now binds the full 64-KiB process image, performs that signed address
calculation, and copies all four bytes across `FFFF -> 0000` when necessary.
It therefore does not need a separately enumerated catalog of every possible
adjacent cell. Focused fixtures pin ordinary indices, index 8, index -1, and
a wrapped four-byte read at `FFFE`.

### DOC-GAP-032: YTCONFIG 52nd YTNAME row overflow destinations

Resolved upstream by commit `aa5675db`. The four wrapped destinations are
`DS:19B2`, `1A7E`, `1B4A`, and `1C16`; the first three stores transfer their
temporary owners into the next arrays' element zero, while the fourth treats
loaded code bytes `62 00 BE B6` as a descriptor and reaches the shared
nonreturning `BRUN:0ACC` internal fatal. No BASIC ERR/ERL route or rollback is
involved.

Affected coverage:

- the YTCONFIG alias-table scan at `YTCONFIG:0E4E..0EE5`;
- a 52nd physical four-token `YTNAME.DAT` group after array index 50; and
- the exact default-fatal or continued state produced by each of its four
  sequential string stores.

The completed configuration-editor document states that the four parallel
arrays are indexed 0 through 50, that the counter is incremented before each
group, and that physical row 52 attempts out-of-bounds writes into adjacent
state. The semantic model consequently raises an abstract `OverflowError`.
Neither source identifies the four wrapped/calculated destination addresses,
the adjacent descriptors or numeric cells they alias, the order and retained
heap ownership of successful stores before a later store fails, or the exact
ERR/ERL/default-handler projection.

Those effects are observable and the native editor's current `count > 51`
guard is only a protective boundary, not a compatibility claim. The generic
BRUN dynamic-string-array contract cannot choose caller-specific destination
addresses or adjacent ownership without the missing YTCONFIG layout. Upstream
documentation, generated evidence, and focused row-52/store-failure fixtures
must publish that carrier before the guard can be replaced. No binary
inspection or new reverse engineering was performed.

### DOC-GAP-031: Xannor sector-arrival physical transaction sequencing

Resolved upstream by commit `aa5675db`. The new physical-step carrier pins
the unconditional opening GET, the mine-loss-only reload/offset-129 PUT, the
owner-label GET, the defense-gated reload and offset-81/85 PUT, every retained
failure prefix, and the later planet unlink outside the sector-arrival child.

Affected coverage:

- the address-owned `YTMAINT:71AF` sector-mine and deployed-defense arrival
  transaction;
- its `7280`, `7390`, `73C4`, `76D2`, `775B`, and `78C8` physical sites; and
- removal of the native route owner's synthetic single sector write after
  the sector, planet, and player arrival children return.

The completed semantic model pins mine-before-defense RNG and mutations, all
result rows, the positive-owner versus Mercenary label, and the final values.
The address-owned physical catalog separately names the initial sector GET,
post-mine sector reload/PUT, positive-owner player GET, and pre-defense sector
reload/PUT. It does not state the branch conditions under which the two reload/
PUT pairs are reached, which live values are copied over each freshly loaded
FIELD image, or the exact process/FIELD/output residue when any one of those
calls fails. The presentation model records news-call prefixes but does not
include these physical calls in its call tape.

Those details are observable and cannot be reconstructed from the final typed
`XannorSectorArrival` result. In particular, the native implementation
currently retains one opening sector image and performs one write only after
the later planet and player children. Replacing that with the catalogued
transaction requires knowing whether mine and defense writes are conditional,
whether a fresh reload preserves intervening fields, and exactly where the
planet child's sector-unlink write composes relative to the sector-arrival
writes. Guessing would change partial-I/O behavior and could reintroduce stale
record bytes.

Upstream documentation, a canonical ordered physical-call carrier, generated
evidence, and focused success/failure-prefix fixtures must publish those
conditions and overlays before the native transaction can be converted. No
binary inspection or new reverse engineering was performed.

### DOC-GAP-030: destroyed-mine caller projection into `FatalWorld`

Resolved upstream by commit `aa5675db`. The canonical projection now builds
`FatalWorld` directly from the inherited projectile carrier, raw mine result,
team cache, and optional returned warp result. It distinguishes the ordinary
post-news/post-sector-GET seam from the emergency-warp seam and preserves the
documented FIELD, persistence, process, pager, and news carriers.

Affected coverage:

- every admitted sector-mine result whose `DS:18B4` destroyed predicate
  transfers from `YT:0913` to common fatal entry `YT:06C4`;
- the direct emergency-warp main W and hostile W/WT continuations when mine
  damage destroys the player without a second warp; and
- the corresponding post-emergency-warp fatal continuation when the mine
  child returns through `YT-SUB:6C2E` with destroyed still nonzero.

The completed sector-mine contract says that a destroyed handoff carries the
canonical `FatalWorld`, but the published caller composition explicitly does
not claim the caller-state join into the common-fatal child. The supplied
`project_mine_fatal_entry()` adapter only checks that an independently
provided `FatalWorld.state` equals the pager state; it does not construct that
world from the inherited gameplay carrier and `RawMineResult`. The graph
likewise retains `fatal-world` as an external continuation input.

That missing projection is observable and cannot be synthesized from the
typed mine result. It must specify the exact record store, FIELD layout/record
and raw image, process cells (including raw destroyed and current-player
roots), team cache, news bytes/open-handle state, and pager/runtime state at
both fatal seams. In particular, ordinary destroyed return has completed the
final news append and final sector GET, whereas a mine-triggered emergency
warp returns without those operations and leaves the warp child's player
FIELD/persistence state.

Upstream documentation, a canonical constructor from the inherited raw world
plus the mine/warp terminal carriers, graph wiring, generated evidence, and
focused fatal-join fixtures must publish this contract before the native
caller can enter the already implemented common-fatal child. No binary
inspection or new reverse engineering was performed.

### DOC-GAP-029: shared A8D2 local string-allocation failure contract

Resolved upstream by commit `aa5675db`. The four A8D2-owned sites are
pinned at `A8EF`, `A8F4`, `A938`, and `A943`, with exact instructions,
saved IPs, statements, ERL 40001, destinations, admitted errors, and main-
handler versus internal-fatal routing. The native staged owner implements
their documented fail-before-effect response, prompt, queue, and bold
residues; its fatal adapter carries the exact saved identity into the shared
BRUN cleanup owner.

The live `session_a8d2()` path uses that same staged transformation after
the shared raw prompt and `0357` editor. Focused fixtures pin blank/Y/N/
invalid retry semantics, queue clearing, prompt clearing, every admitted
fault, and the joined main/hostile plain/ANSI failure prefixes. Root
`YT:A8D2` and all nine qualified internal transfers are verified.
AB36/0357, compatibility-uppercase, raw presentation, main/shared routing,
fatal cleanup, framebuffer, physical, and caller behavior remain
independently owned seams.

### DOC-GAP-033: YTCONFIG row-52 `BRUN:0ACC` fatal projection

Resolved upstream by commit `7454cc90`. YTCONFIG has the exact eight-byte
module label `YTCONFIG`, no statement table and therefore no source-line
clause, and saved IP `0EE5`. The shared fatal owner now composes the exact
local-only diagnostic, prompt, normal-input drain or redirected-input CR,
CLOSE-all, function-row/cursor restoration, and DOS-zero terminal while
retaining the three committed owner transfers and corrupt fourth descriptor.
The native editor uses the physical 51-descriptor-array loader directly and
reaches that terminal instead of applying its former protective row-count
guard.

Formerly affected coverage:

- the fourth physical-group-52 alias store at `YTCONFIG:0EE2`, whose saved
  return IP is `0EE5`;
- the shared `BRUN:0ACC` internal-fatal diagnostic and cleanup; and
- replacement of the native editor's remaining live `count > 51` guard.

Commit `aa5675db` supplied the four wrapped destinations and retained mutation
prefix. Commit `7454cc90` then supplied the canonical YTCONFIG `0EE5` fixture,
extended shared-fatal model, exact output bytes, and terminal cleanup carrier.
No binary inspection or new reverse engineering was performed.

### DOC-GAP-035: complete BRUN runtime error-name table

Resolved upstream by commit `59501f98` and traversal row 327. The canonical
generated `ytsub-error-output.static.txt` now publishes all 39 named error
bytes and the exact `Unprintable error` fallback for every other byte in
`00..FF`.
ERR 75 is `Path/file access error`. The native shared lookup and exhaustive
256-byte fixture consume that published table directly; YT-INIT's RUN
preflight now admits its complete ERR 53/67/75 family.

### DOC-GAP-034: main startup `YT:0133` internal-fatal location

Resolved upstream by commit `59501f98` and the completed main-startup
initialization contract. The
compact COPY frame saves IP `0136`; the statement table selects statement
`012A`, ERL 3; and the exact padded module label is `YT      `. The native
caller adapter now carries that identity into the shared `BRUN:0ACC` owner
and its focused fixture pins the complete local diagnostic and cleanup suffix.

### DOC-GAP-036: ship-computer treasury return-carrier contradiction

Resolved upstream by commit `c37a31c0`. The scalar transcript remains a
bounded fixture ending at `YT:8672`, but it is not the state authority there.
The canonical post-body world is now explicitly carried through the real
wrapper, the fresh `YT:863A` A41C hydration and prompt, the shared AB36 entry,
and the `YT:AB70` loop head for collection, report, and no-owned returns.

The native `computer_menu()` already preserves that same complete session and
database world: both computer treasury dispatches call `command_collect()` and
then continue directly to the shared fresh-prompt owner; its hydration reloads
the current player before `session_0357()` enters the shared AB36 editor. The
verified treasury, A41C, prompt, and AB36 blocks therefore close this caller
seam without treasury-specific copies of their internal behavior.

## Open documentation gaps

### DOC-GAP-043: BRUN COM1/COM2 CLOSE method contract

Affected coverage:

- the source-reachable COM1/COM2 controls closed by the serial `END` and
  `CLOSE_NO_ARGS` paths;
- signed control classes -4 and -5 dispatched to `BRUN:126D`; and
- the corresponding process-control, external-port, and failure residue.

`docs/runtime/brun-lof-close.md` proves that signed control classes -4 and -5
dispatch to `BRUN:126D`, and explicitly says that COM1/COM2 closure is reached
by serial `END` and CLOSE-all rather than the 73 rooted explicit-CLOSE calls.
It does not describe what `126D` reads or writes, which external operations it
performs, whether any result can fail nonlocally, or what control/registration/
port state survives each outcome. The generic CLOSE-all contract says only
that signed classes reuse their real table targets; that does not define this
method interface.

The native CLOSE-all reducer already preserves high-to-low registry order and
passes the signed class to a supplied method. Implementing the actual -4/-5
method by treating it as ordinary `AH=3Eh`, as unconditional release, or as
OpenDoors shutdown would invent behavior at a source-reachable boundary.

Upstream runtime documentation, the global-state registry, generated evidence,
and focused success/failure fixtures must publish the bounded `BRUN:126D`
contract before the native COM close method can be implemented. The verified
generic registry traversal, ordinary-file method, and fixed-console suffix are
unaffected. No binary inspection or new reverse engineering was performed.

### DOC-GAP-042: non-startup date-helper result process cells

Affected coverage:

- the raw by-reference result of each non-startup `YT-SUB:215B` date-helper
  call used by port updates, planet updates/creation, nearest-port reporting,
  and the command-16/17 profit bodies;
- the process game-day value that must be committed before a following
  `TIMER`, GET, or caller failure; and
- the remaining compiled YTCONFIG, YT-INIT, RMT-INIT, and YTMAINT helper
  result/caller-copy projections where their destination differs from the
  documented startup slice.

`docs/runtime/startup-date-serial-world.md` gives the complete startup result
contract at `DS:188C` and its caller copy at `DS:4CCA`. The profit-cycle and
nearest-port documents state that every successful helper call immediately
replaces a process game-day cell, including the observable case where a later
`TIMER` or GET fails. The global-state registry names only the startup result
and copy. It does not identify the non-startup by-reference destination cells,
any following caller-copy cells, or whether any of those sites intentionally
reuse `DS:188C` or another shared address.

The native semantic callers already sample the clock in the documented order
and return the correct MBF32 day value, but most do not publish that value into
the persistent 64-KiB process image. Choosing a destination from a host-side
variable or assuming the startup address is reused would invent observable
state and could change later aliases or failure residue.

Upstream documentation, the global-state registry, and generated caller
artifacts must publish each reached result address and copy/order rule before
the remaining process mutations can be implemented. The shared date
calculation, startup `DS:188C -> DS:4CCA` composition, and caller semantics are
unaffected. No binary inspection or new reverse engineering was performed.

### DOC-GAP-041: contradictory AB36 active-fault completion claim

Affected coverage:

- the root-owned active-handler, raw editor, string-heap, and event-frame fault
  carrier for `YT:AB36`;
- the claimed 79-site live dispatch denominator and its ordinary first-fault and
  secondary-fault routing; and
- the contextual B05D and B1F3 child boundaries consumed by that dispatcher.

`docs/runtime/ab36-active-fault-inventory.md` calls its 79 local opcode sites a
complete inventory, and `docs/runtime/ab36-active-fault-dispatch-map.md` says
all 79 rows have exact live producer boundaries with zero unresolved interface
rows. `docs/runtime/error-router.md` and `main-error-handling.md` repeat that
closure claim.

The completed event documents disagree. `docs/runtime/async-event-transducer.md`
still lists connection of later B1F3/AB36 first faults and secondary handler
faults as central remaining work, and `docs/runtime/f8-fault-output.md` likewise
leaves later AB36 faults outside its closed pre-B05D set. The AB36 dispatch also
imports B05D as a 26-site child, whose LOC inventory is separately contradictory
under DOC-GAP-039.

The native implementation cannot tell whether the event documents retain real
unconnected AB36 states or merely stale completion prose, nor can it bind the
contextual child denominator while DOC-GAP-039 remains open. Upstream must
reconcile the closure statement and regenerate the dispatch evidence if any of
its live rows or child boundaries change. No binary inspection or new reverse
engineering was performed.

### DOC-GAP-040: contradictory B1F3 fault denominator and completion state

Affected coverage:

- the reachable fault-site denominator for `YT:B1F3`;
- the ordinary first-fault and active-handler secondary-fault carriers; and
- whether the four main-module `COPY_STR` cuts belong to the native pager root.

`docs/runtime/pager-active-fault-output.md` publishes eleven reachable sites:
three GOSUB stack cuts, four uppercase-helper cuts, and four main-module
`COPY_STR` allocation cuts. It says the B1F3 producer, live pager-machine
connection, active transfer, and sink are closed. Its linked generated artifact,
`analysis/disassembly/ytpager-fault-output.static.txt`, likewise enumerates
eleven sites.

Two other completed documents disagree. `docs/runtime/presentation-helpers.md`
calls the sequence seven reachable faults and identifies only the three stack
checks plus four uppercase-helper cuts, omitting prompt, response, NS-notice,
and E-to-Q `COPY_STR`. `docs/runtime/async-event-transducer.md` still lists
connection of later B1F3/AB36 first faults and secondary handler faults as
central remaining work.

The native implementation cannot infer whether the dedicated eleven-site
contract supersedes stale prose or whether its claimed completion is premature.
Upstream must reconcile the site count and the first/secondary-fault completion
statement across these documents and regenerate the canonical artifact if the
eleven-site inventory changes. No binary inspection or new reverse engineering
was performed.

### DOC-GAP-039: contradictory B05D `LOC(3)` fault inventory

Affected coverage:

- the reachable-cut denominator for the shared `YT:B05D` active-fault
  transducer;
- whether `YT:B080`/saved-IP `B083` has any physical error, retry, accepted-
  prefix, or handler projection; and
- the native B05D runtime-fault identity table and active-handler carrier.

`docs/runtime/b05d-active-fault-output.md` says that `YT:B080 LOC` is an
ordinary deterministic ring-state transition and that the former LOC physical
cut is excluded by the shared address-owned proof. The linked generated
artifact, `analysis/disassembly/ytb05d-fault-output.static.txt`, still
includes `loc-device` as a physical adapter cut and labels the inventory as 26
reachable sites. The prose also continues to call the ledger 26 cuts, although
its stated categories total 25 after excluding LOC: three called-helper frame
allocations, fourteen string allocations, seven physical operations, and one
final pager GOSUB stack check.

These contracts cannot both define the native fault-site enum. Including
`loc-device` would preserve a documented source-infeasible edge; omitting it
would disagree with the canonical generated artifact and its published
denominator. Upstream must select the reachable inventory, correct the prose
and generated artifact together, and pin the resulting ordered site list/count
before the native active-fault carrier can be completed. No binary inspection
or new reverse engineering was performed.

### DOC-GAP-038: PORTNAME default BRUN fatal projections

Affected coverage:

- startup CLOSE, OPEN, FIELD, LOF, configuration GET/CVS, and RNG
  setup failures before any application output;
- LOF-zero CLOSE-all or KILL failures after the exact bell-wrapped
  missing-data rows;
- confirmation INPUT/editor failures and accepted-path close/reopen/FIELD
  failures; and
- name-generation, progress PRINT, GET/PUT, final CLOSE, and PLAY failures
  after their documented committed prefixes.

`docs/runtime/portname-output.md` explicitly ends each of these cases at a
typed runtime or physical boundary. It states that PORTNAME installs no
application `ON ERROR` handler and does not append a guessed BRUN
default-fatal message. The native controller currently falls back to
`yt_cli_error()`, which is a host diagnostic and cannot be claimed as the
legacy terminal.

The shared no-handler BRUN fatal renderer and complete 00h..FFh description
table already exist. What is missing is the PORTNAME caller projection for
each admitted failure: exact BASIC error byte, saved IP, current statement or
no-line state, module segment/label inputs, retained file/FIELD/runtime
carrier, and the selected cleanup continuation. Without those identities the
implementation cannot replace the host diagnostic with the exact shared fatal
terminal or prove which failures remain physical boundaries.

Upstream documentation, generated evidence, canonical fault projections, and
focused representative joins must publish this contract before the PORTNAME
error paths can be completed. The already exact ordinary missing-file, blank,
abort, accepted, and successful completion behavior is unaffected. No binary
inspection or new reverse engineering was performed.

### DOC-GAP-037: action-finalizer CINT failure identities

Affected coverage:

- the unconditional Anti-Cloak `CINT` in the shared `YT:A6E3` action
  finalizer after turn offset 49 has been staged; and
- the selected-cloak current-player `CINT` after cloak offset 125 has been
  staged but before the unchecked process-cache write.

`docs/runtime/action-finalizer-output.md` specifies the non-short-circuit
Anti-Cloak conversion, the separate current-player conversion, their exact
normal ordering, unchecked cache-address calculation, and retained FIELD
prefixes. The native finalizer now implements those documented normal paths,
including fractional Anti-Cloak values and wrapped process-cache addresses.

The document's failure table names the two conversion boundaries only as
`cadence CINT/cache-address error`. It does not publish either call
instruction/saved IP, current statement, ERL, admitted ERR values, installed
handler, retry identity, or main/shared error-router projection. Returning a
protective `YT_RANGE` at overflow is therefore not an exact BASIC failure
claim and cannot verify those two edges.

Upstream documentation, generated evidence, canonical error-router carriers,
and focused projections must publish both identities before these failure
paths can be completed. The already documented non-overflow finalizer behavior
is unaffected. No binary inspection or new reverse engineering was performed.

### DOC-GAP-023: current-sector scanner cloak-clear raw value

Affected coverage:

- authoritative process-array reads for the current-sector scanner's
  player-candidate loop at `YT-SUB:5E8D`; and
- the exact revealed-cloak cache mutation and residue before selector-four
  sound, later duplicate sensor slots, visible-player GETs, and dependency
  failures.

The completed `docs/runtime/current-sector-output.md` and
`docs/gameplay/command-shell.md` establish that the scanner reads the
startup-era sector and cloak arrays, emits the shimmer row for an admitted
reveal, then clears only that candidate's cloak cache before sound and later
visibility tests. The semantic `tools/ytsector_output.py` model records the
result only as numeric zero. Neither it, the generated scanner evidence, nor
the global-state registry identifies the four source bytes copied into
`DS:1A70 + 4*(candidate+52)` by this clear.

The raw value cannot safely be inferred from its numeric meaning. Completed
cache owners already use canonical `00 00 00 00`, Xannor retaliation uses
`00 00 40 00`, player death uses `00 00 7A 00`, and action-finalizer expiry
uses `00 00 A3 00`. The scanner's process reads and clear must migrate as one
unit because a duplicate mode-one sensor slot observes the completed clear;
mixing process reads with the existing typed-only clear would replay stale
cloak state.

Upstream documentation, generated raw evidence and a focused duplicate-slot
failure-prefix fixture must identify the copied source bytes. Until then the
native current-sector scanner retains its typed cache carrier. No binary
inspection or new reverse engineering was performed.

### DOC-GAP-022: Earth Anti-Cloak cache-clear raw value

Affected coverage:

- authoritative process binding of the global Earth Anti-Cloak sweep's
  positive-cloak clears at `YT-SUB:932E`; and
- exact cache residue across its target `GET`, name-conversion, output,
  sound, fade and credit-debit failure prefixes.

The completed `docs/runtime/earth-purchase-actions-output.md` and
`docs/gameplay/planet-actions.md` establish that every positive player cloak
cache entry is cleared before that player's `GET`, while zero and negative
entries remain unchanged. They identify the process cache mapping as
`DS:1A70 + 4*(record+52)`, but neither document nor the generated Earth
purchase model identifies the four raw bytes written by the clear. This
cannot safely be inferred from the numeric result: other completed cache
owners use canonical `00 00 00 00`, Xannor retaliation uses
`00 00 40 00`, player death uses `00 00 7A 00`, and action-finalizer expiry
uses `00 00 A3 00` for numerically zero values.

Upstream documentation, generated raw evidence and a focused failure-prefix
fixture must identify the copied source bytes and confirm whether every loop
iteration uses the same value. Until then the native Anti-Cloak model retains
its typed cache mutation and is not connected to the authoritative process
array. No binary inspection or new reverse engineering was performed.

### DOC-GAP-021: scoreboard team-ID scratch write boundary

Affected coverage:

- authoritative binding of the scoreboard generator's current-player team-ID
  scratch at `DS:4B8C`; and
- exact process/FIELD residue when a player score-cache GET or PUT fails.

The completed global-state registry and `docs/file-formats/YTSCORE.md` identify
`DS:4B8C` as an MBF32 value copied from player FIELD offset 89 during
`YT-SUB:3AF5..3B46`, and say that a normal return retains the last player team
ID. They do not place that copy relative to the loop's fresh player GET,
score-field overlay, and durable cache PUT. That ordering is observable: a
failed PUT either retains the preceding player's scratch or the current
player's raw team bytes. The current native generator also decodes the initial
player sweep into a host object, so choosing that stale value rather than the
fresh FIELD bytes would create a second undocumented behavior.

Upstream documentation, generated evidence, and a focused failure-prefix
fixture must identify the exact copy point and raw source before `DS:4B8C` can
be made authoritative. The separately documented sector-owner reuse of
`DS:1A40` is not blocked by this gap. No binary inspection or new reverse
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

## Previously resolved documentation gaps

### DOC-GAP-028: ADE0 allocation/GOSUB current-statement identities

Resolved upstream by commit `98f69fcd`. The 22 remaining main-owned ADE0
allocation/GOSUB sites and all four delegated uppercase-helper sites now
have exact instruction, saved-IP, current-statement, ERL, ERR and installed-
handler identities. The native fault registry contains all 26 mappings and
projects their exact main or shared terminal routes. The staged save,
uppercase, repeat-parse, repeat-build/notice, and semicolon/queue owners now
attach those identities at all 26 selectable BRUN heap/GOSUB cuts, including
arbitrary build and replacement-loop occurrences. Raw heap, descriptor,
frame, asynchronous continuation, and physical handler effects remain
implementation work rather than a documentation gap.

### DOC-GAP-027: sequential disk-read carry prefix and cursor conflict

Resolved upstream by commit `b37b9064`. The one shared `CBFD..CC52` refill
accepts a carry-set physical prefix of zero through 128 bytes and a terminal
external cursor. It overlays that prefix on the cleared buffer, retains the
zero tail, leaves the old total count and zero remaining count, does not
increment the refill index, and cannot consume the failed prefix. `EOF` and
regular-file `LINE INPUT #` use this same contract. The existing native typed
adapter and exhaustive refill fixtures already implement it.

### DOC-GAP-026: ADE0 repeat-overflow current-statement identities

Resolved upstream by commit `b37b9064`. Repeat `VAL` overflow at `AEBF`
saves IP `AEC2`; repeat conversion-to-SINGLE overflow at `AEC8` saves IP
`AECB`. Both belong to current statement `AE9C`, ERL 36000, main handler
`B2DA`, and ERR 6. The native fault registry now exposes both identities and
the live session attaches them to the existing nonlocal gameplay-resume
router.

### DOC-GAP-025: new-player identity GET/PUT error projection

Resolved upstream by commit `0c2f8ccf`. The selected-player GET is
`064B/064E`, retries statement `0640`, and has ERL 11120; the identity PUT is
`0683/0686`, retries statement `0678`, and has the same ERL. Both run under
main handler `B2DA`; GET admits ERR 52/57/70/75 and PUT additionally admits
ERR 61. ERR 57 retries only the named I/O statement, while every other
admitted error takes the main terminal route over the documented retained
FIELD and physical-I/O prefix.

### DOC-GAP-024: post-login repair raw FIELD sources

Resolved upstream by commit `0c2f8ccf`. The first predicate is cached current
sector below raw one—not turns below one—and its sole assignment copies
`DS:628A` byte-for-byte to player FIELD offset 57. The cargo repair copies
canonical zero `DS:62F4` to offsets 69 and 73, then independently copies live
maximum-holds `DS:19E8` to offsets 77 and 65. The two PUT identities,
ERR domains, retry statements, retained dirty FIELD images, and physical
failure prefixes are now pinned.

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

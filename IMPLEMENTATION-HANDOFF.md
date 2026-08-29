# Yankee Trader C17 implementation handoff

## Read this first

This document is for the fresh session that resumes work on this door. The
separate reverse-engineering effort is complete. Do **not** continue from any
frontier remembered from an earlier conversation. Read the completed analysis
at its current state and derive remaining C work from that evidence and the
continuation snapshot below.

Door root:

```text
/bbsdev/doors/yt/ng
```

Completed Yankee Trader 3.6G analysis:

```text
/bbsdev/doors/yt/3.6G
```

OpenDoors reference manual and source:

```text
/bbsdev/doors/yt/ng/third_party/opendoors
```

Reference C/OpenDoors door:

```text
/synchronet/src/sbbs/src/doors/clans-src
```

This implementation has its own Git repository. OpenDoors is pinned as the
`third_party/opendoors` submodule; do not substitute the stale Synchronet
source copy. The completed reverse-engineering tree is external evidence, not
part of this repository and not a writable implementation workspace.

## Authoritative continuation goal

Create or resume the implementation goal from this corrected text when work
starts in this repository:

> Complete the C17/OpenDoors reimplementation of Yankee Trader 3.6G in
> `/bbsdev/doors/yt/ng` to exact compatibility as defined by this handoff and
> the completed byte-pinned analysis. Map and verify every reachable
> component, qualified edge, external effect, executable, player-visible
> remote output and local result, input/runtime path, persistence mutation,
> random draw, failure prefix, and supported-platform boundary. Preserve the
> three authorized departures: platform CSPRNG source; IEEE internal floats
> with MBF serialization boundaries; and local BASIC `PLAY` as an ordered
> logical event with no physical host-audio operation. Keep compatibility,
> enhancements, hardening, and automation separate. Continue implementing and
> testing dependency-first until genuinely complete.

Treat a concrete requirement as a blocking condition when its required
behavior lacks completed reverse-engineering evidence, final analysis
artifacts conflict, or exact compatibility conflicts with another goal and no
authoritative resolution is evident. Request user direction in that case.
Formally mark the goal blocked only when the goal system's repeated-blocker
threshold has been satisfied. Do not search for more reverse-engineering
evidence speculatively; identify the exact missing or conflicting fact first.

There is no requirement for a monolithic full-session transcript test, and
one is not expected to be created. Completion proof is compositional: exact
component fixtures plus separately verified presentation, persistence, input,
randomness, runtime, and physical OpenDoors/platform boundaries.

The old thread goal object was paused during relocation and contains the old
Synchronet path and the obsolete phrase “two authorized departures.” It is
not authoritative. Use the corrected goal above in the new workspace.

## Continuation snapshot (2026-08-29)

- Standalone repository baseline:
  `840fcbfe0e39c1cb89bd6f7ad3008ad6fe278d0d` on `main`, published to
  `git@github.com:RealDeuce/yt.git`.
- OpenDoors is the `third_party/opendoors` submodule pinned to upstream commit
  `c70189d79e1743200eb01b7c7edfde3084e1a0a9`. The authoritative CMake build
  links `OpenDoors::Static`; it no longer consumes Synchronet's sibling copy.
- A fresh Release build from the standalone checkout passes all nine CTest
  suites.
- Origin-aware local/remote input arbitration is implemented in
  `src/yt_input_model.c`; `src/yt_input.c` supplies legacy and merged polling,
  and the session routes B05D, AB36, prompts, timed waits, and the radio editor
  through those adapters.
- Pager state and deterministic transitions are extracted into
  `src/yt_pager.c`. Successful plain/ANSI/mode-2 pager transactions, response
  forms, gates, nested counts, colors, terminal notices, and editor echo have
  bounded fixtures in `tests/test_presentation.c`.
- The adjacent shared `YT:02BF`/`02FC`/`0317` presentation spine now has
  explicit native owners. `02FC` always clears the newline flag before B05D,
  and `0317` composes the direct CRLF blank with that forced LFCR row; editor
  terminal notices, save/repeat notices, and mapped team rows use those
  owners instead of open-coded variants.
- The neighboring `031F` prompt and `0345`/`0357`/`036F` input wrappers also
  have explicit native owners. Post-login uses the low-time/no-terminator
  `031F` path; Team retains case-preserving `0345` input; Planet menus use
  compatibility-uppercase `0357`; and the six mapped Planet quantity paths
  use `036F`, where any uppercased `E` clears the complete response before
  `VAL`. Raw BRUN frames/string failures and remaining callers stay open.
- The shared compatibility-uppercase root `YT-SUB:[1D99,1E17)` is now mapped
  to `qb_compat_upper_n`: empty input exits immediately, and every byte
  strictly above `40h` is replaced in place by byte AND `DFh`. Native tests
  pin the empty loop and exhaust all 256 byte values; all five qualified
  successful loop/return edges are candidate and all 13 focused upstream
  raw/fault tests pass. The native exact transform does not yet reproduce the
  legacy procedure-static MBF32 length/index/temp cells or movable-string
  MID$/CHR$ allocation failures and active ERR-7/14 routing; those remain
  explicit runtime seams.
- The adjacent Team name and password helper roots `YT:[64CD,660D)` are also
  candidate. The name helper's eight edges preserve raw-length-before-
  normalization admission, the three-space empty-name result and full
  normalization before the 41-byte cap; native reducers pin two-byte, three-
  space, collapsed mixed-case and 50-byte cases plus fixed-name/MBF32-length
  record preservation. All seven password edges were already mapped to the exact direct
  blank/prompt, visible raw editor, compatibility-uppercase four-byte gate,
  styled disclosed-password reminder, fresh overlay GET, offset-113 LSET and
  PUT sequence. Native creation and raw-record fixtures cover the ordinary
  lowercase `pass` path and unrelated-byte preservation; all 93 focused Team
  oracle tests pass. Inline retry, isolated ANSI/local action output, raw
  cache/FIELD/failure state, malformed record targets and physical prefixes
  remain open.
- The shared sector-mine root `YT:[1ECC,2059)` is candidate with all 23 body
  edges mapped. Existing native owners preserve the independent hydration,
  negative durable repair with stale carried cache, strict refusal gates,
  fractional/equality admission, suppression-before-I/O, player-before-
  sector persistence and success/sound order; all 57 focused isolated/main/
  hostile oracle tests pass. A complete raw two-record FIELD/store/cache and
  ordered-failure fixture, joined main scanner bytes and physical endpoints
  remain open.
- Relevant internal `YT:B1F3..B2D9` transfer-ledger rows have candidate owners
  and pager fixtures. The TSV schema and byte-pinned opcode evidence were
  checked after those edits.
- The deployed-sector-fighter Attack/Bribe component is now native across its
  complete combat and offer bodies. Attack is post-tested, computes its
  quantum before the one-shot surrender gate, admits surrender only for owner
  greater than one, keeps forced-Bribe surrender active, distinguishes both
  faction refusals, and consumes the unconditional late draw. Accepted
  surrender appends binary-safe cached-name news before state computation and
  its final row. Combat persists raw player-before-sector overlays; accepted
  Bribe persists raw sector-before-player overlays while leaving process
  caches stale. Mercenary planet truth uses the raw MBF32 exponent. Xannor
  rewards use the exact SINGLE clamp, turns-only overlay, binary-safe display
  and news, and clearance ordering. Reducers pin owner/bonus predicates and
  unrelated-byte preservation; presentation pins the complete accepted
  surrender body in plain/ANSI, both faction refusals/sounds, reward row,
  Bribe bodies and spill endpoint. All 180 focused upstream component and
  joined-cycle tests pass. The 38-site presentation family and all 112 Attack
  plus 47 Bribe transfers are candidate; full injected physical
  WORLD/FIELD/RNG/failure transcripts remain open.
- The ordinary main-command front end now follows the exact shipped control
  grammar and presentation. Entry clears the scanner scratch after the direct
  blank, uses the shared uppercase editor, preserves whole-string `X`, `S`,
  and empty handling, then dispatches all other commands by first byte. Empty,
  invalid, instruction, Info, and Help paths use their recovered shared
  wrappers; Info no longer performs an extra hydration. Help emits the exact
  eleven 40-column direct rows and seven narrative rows. Native fixtures pin
  complete 1,212-byte plain and 1,242-byte ANSI help cycles plus invalid,
  empty-display, instruction-default, and quit-cancel branches; all 19
  focused upstream shell tests pass. The site-free presentation family and
  all 62 address-owned front-end transfers are candidate. Complete injected
  A41C/FIELD/editor/queue/carrier/local/physical failure transactions remain
  open.
- After the exact Transfer, Bank, Productivity, Clearance, Earth-front,
  Earth-purchase, Planet Rename, shared compatibility-uppercase, Team
  name/password, Planet inventory/menu-parent, shared sector-mine, YTMAINT
  entry/configuration defaults/wrapper prefixes, protected-mine and
  player-aging passes, message/news compaction, immediate/final player
  cleanup, and
  alias-compaction, port/planet/Wanderer maintenance, maintenance-route,
  maintenance-scoreboard, and Xannor
  sector/player/planet-arrival and final group-persistence slices and the
  logical F4/F5/F8/F9/F10 SysOp
  controls, both bounded runtime-error dispatchers, the Xannor victory body,
  shared port-name editor, command-N/command-B port cycles, and command-G
  Genesis cycle, Planet Ground Forces action, and the linked-planet landing
  caller/permission helper plus its separate ground-assault body, zero-link
  creation arm, Planet Thrusters controller/one-hop helper, anonymous
  sector-mine body with both caller continuations, direct fighter-kill
  wrapper, its destroyed-to-fatal cycle, the shared emergency-warp/black-hole
  body, the bounded gameplay-hazard continuation, and the ordinary movement
  parent plus its active-main M cycle, the direct player-Attack parent
  plus its active-main A cycle, the deployed-fighter Attack/Bribe bodies, and
  the ordinary main-command front end, plus Mercenary defection, movement,
  destination-selector and destination-combat transactions,
  ledger dispositions are roots
  `167 candidate / 1 missing`, transfers
  `5266 candidate / 2101 missing / 1 explicitly
  deferred`, and presentation `136 candidate / 8 missing / 6 verified`.
  Current SHA-256
  values are roots
  `64b17a8ae6550d4ef994a6e872d1f06c9ff8a6bc33ab9ab64d390da38a87e8be`,
  transfers `d104e84da4c73b85eb527c6bb69fa6997933b5f9d59fb9aba836b42633e522f6`,
  presentation `b899734f3487d364fbad0b2d5be7936334cc753b4c676d3407d6703d522b5afe`,
  and manifest
  `3552bd61c9cb9b7197aeb6fbad1f37cba69cd09f125d878d0c9072a691e8950a`.
  All immutable identity columns and TSV widths match the regenerated trusted
  snapshot.
- Three final-analysis blockers/gaps are currently known. First, the generated
  `YTCONFIG:0030..053B` startup/menu print catalog is internally inconsistent
  with its pinned reachable disassembly: it labels `02A3` as a record-1
  local-screen row although `02A3..02BB` constructs and prints
  `<A> Maximum Number of Holds:`, omits the actual `<B>` output start at
  `02BE` and `<C>` start at `02D6`, and misassigns later rows such as the
  `03C8` scoreboard-path construction. Do not implement or map the
  `ytconfig-startup-menu-output` family from that catalog until the upstream
  analysis project corrects and regenerates it. Other independent families
  remain actionable, so this does not globally block the active goal.
- Second, canonical Mercenary prose says that a moving fleet traverses one
  FIFO-route hop, while the pinned executable has empty-hop, friendly-merge,
  and winning-attack joins at `4EE7`, `4F5F`, and `51E0` that reach
  `5315 -> 4932` and advance the route-workspace cursor again. The adjacent
  and disconnected zero-successor arm is independently pinned and native;
  do not implement or map the wider multi-hop continuation until the analysis
  project reconciles the traversal contract.
- Third, the bounded maintenance-entry output model deliberately exposes
  `date-or-epoch`, `time-or-date`, `dynamic-status`, and
  `dynamic-player-status` as address-owned injected operands, but the current
  final artifacts do not assign their concrete production meanings at
  `0389`, `03D3`, `03EC`, `03FD`, and `050D`. The byte composer is exact for
  supplied operands; do not connect it to `yt_maintenance_run()` or promote
  root `YTMAINT:0073` until the upstream project identifies the values and
  their acquisition/order.
- The exact LOCAL BIOS framebuffer launcher and cross-platform OpenDoors
  startup/exit/local-console adapter boundary remain explicitly deferred by
  the user. The present stdout/ANSI `src/main_local.c` path is not exact and
  must not be promoted as if it were. A one-line OpenDoors personality in
  forced-local mode is an available future design for SysOp utilities, but it
  does not authorize any OpenDoors API/ABI/source change.
- The site-free `ytconfig-date-name-case-helpers` family is candidate despite
  the separate startup/menu catalog blocker. The `1DF1` date owner preserves
  the incoming SINGLE epoch, including fractional values, while the `2073`
  title normalizer and `228F` ASCII-only uppercase helper retain their exact
  binary/CP437 domains. This audit found and removed one native caller
  truncation: the maintenance editor had cast the stored epoch to `int`
  before date calculation and now calls the existing float-epoch,
  single-clock-sample owner directly. Native suites and all 124 focused
  YTCONFIG/helper tests pass. Raw DATE$/VAL/string descriptors, allocation and
  clock failures, and the still-unmapped caller-specific joins remain open.
- The first bounded `YTMAINT:0073` orchestration slices are now native without
  promoting that 6,185-node root. Headquarters numeric zero is durably changed
  to 85 before the later process-local scoreboard/local-screen/lottery/holds
  defaults; an isolated failure-cut fixture proves those later defaults do not
  leak into record 1. Logical sectors 1 through 7 have their mine cell cleared
  with all unrelated bytes and sector 8 preserved. The post-rotation header
  uses two fresh clock samples in TIME$-then-DATE$ order. Entry, same-day
  fallthrough, common prefix, compaction and wrapper rows are byte-pinned.
  Player aging now preserves raw-nonzero/CINT-zero name admission, negative
  cloak normalization, SINGLE charge/clamp, inclusive deletion gates and the
  expiry-before-deletion jump. The native loop previously used a generic
  fixed radio sentence and could delete an expiring inactive player in the
  same iteration; it now emits the exact fresh TIME$/DATE$ message after news
  and direct-screen output and skips deletion. A four-record physical fixture
  closes the loop-control transfers with empty, ordinary, expiry and
  normalized-negative visits, exact cache/durable results and final-record
  exit. Broader driver orchestration, raw runtime state and physical failure
  prefixes remain missing.
- The following bounded `YTMAINT:07A3..0FAE` port phase is now connected
  without promoting the maintenance driver root. The native reducer preserves
  the shipped SINGLE elapsed-day expression and anomaly clamp, DOUBLE stock
  accumulation, strict production/plague/cap/maximum predicates, conditional
  RNG order, classification and factor signs. Its output reducer uses the
  exact `STR$_SINGLE` aggregate rather than the former DOUBLE spelling. A
  two-record physical fixture pins unconditional heading rows, fresh DATE and
  TIMER samples per record, ascending PUTs, shared RNG continuity, exact
  screen-before-news aggregate bytes, and mutation of only PORT offsets
  41..84 and 101..104. The raw fixture includes an embedded NUL in the port
  name, which exposed and prevents unrelated name-field rewriting. All 34
  qualified transfers and the port-maintenance presentation family are
  candidate; shipped-scale/raw-runtime and staged failure prefixes remain
  missing.
- The connected maintenance prefix now continues through the bounded
  `YTMAINT:0FAE..1CE3` planet phase. The extracted native reducer pins the
  strict positive active marker, mixed SINGLE/DOUBLE production and bank
  arithmetic, unconditional three-draw event gate, plague-before-civil-war
  override, conditional reduction/ground/expense draws, caps, and optional
  row gates. The prior live owner emitted no direct phase/event rows and used
  DOUBLE strings for values that the executable sends through
  `STR$_SINGLE`; the new presenter emits the exact date, disaster,
  productivity, floored-ground and DOUBLE-expense rows, each direct-screen
  row before its matching news append. A three-record physical fixture pins
  inactive preservation, active-only fresh DATE/TIMER sampling, RNG
  continuity, binary-safe stored-length planet names, ascending PUTs and only
  the documented maintenance FIELD lanes. All 52 qualified transfers and the
  planet-maintenance presentation family are candidate; shipped-scale/raw-
  runtime and staged failure prefixes remain missing.
- The connected maintenance prefix now extends through the bounded
  `YTMAINT:1CE3..2069` Wanderer phase. The native transaction follows the
  shipped ascending first-link scan and optional immediate clear, samples a
  fresh date only on the missing branch, rebuilds only the documented PLANET
  lanes, emits each missing/regeneration row before its matching news append,
  and prints the success row before the first candidate GET. Candidate sectors
  accept an exact numeric zero only, so a negative link is rejected and
  consumes another bounded-random draw. The final sector and fresh planet
  overlays rewrite only the link, owner and bank lanes; a nonzero existing
  bank survives while a zero bank becomes 250000. Existing and missing raw
  physical fixtures pin those effects, retry order and preservation. All 14
  qualified transfers and the Wanderer presentation family are candidate;
  shipped-scale/raw-runtime and staged failure prefixes remain missing.
- Upstream analysis commit `ed215d63` corrected `YTMAINT:20A8` to the
  configured-headquarters SECTOR probe and established that only an exact
  zero planet link rebuilds Xannoron; a different positive link bypasses the
  reconstruction just like the expected planet-100 link. The bounded native
  `2069..23E2` home transaction now follows that gate, the conditional fresh
  date/TIMER and ground-then-bank draws, direct-screen-before-news rows,
  rebuild PLANET PUT, fresh headquarters GET/link PUT, and unconditional
  fresh PLANET GET/daily draw/PUT. Raw zero-link and positive-other-link
  fixtures pin the conditional three-versus-one draw counts, zero-bank-only
  refill, exact output/news, and preservation outside the documented rebuild,
  sector-link and daily-update lanes. All nine qualified home transfers are
  candidate; the larger Xannor presentation family remains missing until its
  top-player, group, headquarters and roaming continuations are connected.
- The connected Xannor prefix now continues through the bounded
  `YTMAINT:23E2..263A` top-player scan and hunt gate. The native transaction
  scans records 2 through the configured maximum in ascending order, admits
  raw-nonzero active markers, replaces the winner only for a strictly greater
  score, and appends the report heading after the scan. A selected winner is
  freshly reloaded before the unconditional gate draw; score below 2,500,000
  or cached cloak strictly greater than that draw suppresses the hunt, while
  equality remains eligible. The optional second draw selects the cached
  sector only when it is strictly greater than `.25`; otherwise the fresh live
  sector survives. Native fixtures pin the no-player, rejected and selected
  lanes, lower-record tie retention, exact stored-length binary-safe name
  output, draw counts, and absence of persistent mutation. All 17 qualified
  hunt transfers are candidate; the ordinary-target fallback and group
  continuations remain missing.
- The following `YTMAINT:263A..26D4` ordinary-target fallback is now native
  and connected before the first group-slot GET. Inclusive targets from 8
  through the configured sector count survive without a draw only when the
  hunt-player token is nonzero. An out-of-range target or exact-zero token
  consumes one bounded draw over `sector_count-7`, adds seven to produce the
  replacement sector, and clears the token. Native endpoint fixtures pin
  valid no-draw retention, low/high replacement results, exact one-draw
  consumption and invalid ranges. All seven qualified transfers are
  candidate; raw fractional/configuration state, provider failure, and the
  physical group-extraction continuation remain open.
- The bounded `YTMAINT:26D4..2A0C` group-load continuation now has an exact
  physical native owner. It no longer interleaves group metadata, host
  extraction and duplicate merging: all 20 reserved offset-105 slots are
  loaded and raw-cleared in ascending order first, Xannor-owner `-1` host
  defenses are extracted in a second ascending pass with only sector fighter
  and owner lanes cleared, and later co-located groups fold into the earliest
  slot in the final pass. A raw disk fixture pins other-owner preservation,
  duplicate folding, unrelated-byte preservation, and an invalid-host failure
  after every metadata PUT has already occurred. All 25 qualified extraction
  transfers are candidate; complete staged I/O/runtime faults and raw BASIC
  array/FIELD/process residue remain open.
- The Xannor prefix now continues through regeneration, headquarters reclaim,
  and headquarters relocation at `YTMAINT:2A0C..2FAB`. Regeneration preserves the shipped SINGLE
  division/`INT` boundaries, strict aggregate ceiling, and DOUBLE addition
  back to group 1; its date/DOUBLE-formatted-row/news/date prefix completes
  before group state changes. The reclaim transaction loads a positive
  defender's raw stored-length player name or uses `The Mercenaries` for a
  nonpositive hostile owner, emits attempt news before screen, retains DOUBLE
  defenders against SINGLE group arithmetic, and uses the strict-both-`>250`
  quantum with the inclusive `RND <= .5` defender-loss arm. It freshly reloads
  headquarters before a raw fighter/owner overlay and PUT, then emits result
  screen before news. Binary-name success and Mercenary failure fixtures pin
  exact rows, news bytes, draws, mutation lanes and retained exhausted-group
  location. Relocation preserves the two shipped trigger forms and no-op
  exit, retries bounded candidates under the exact fighter/owner/planet gate,
  updates process-local headquarters state before persistence, overlays and
  PUTs raw record 1 before inspecting the old sector, conditionally clears the
  old Xannoron link, freshly relinks the accepted sector, then emits news,
  direct announcement and date in order. Its raw disk fixture pins exact draw
  counts, changed lanes, unrelated-byte preservation, and the non-rollback
  failure prefix where record 1 remains committed if the later old-sector GET
  fails. All three regeneration, 26 reclaim, and 15 relocation transfers are
  candidate; raw runtime state and exhaustive failure prefixes remain open.
- The connected prefix now also consumes the one-shot revenge slot at
  `YTMAINT:2FAB..30EA`. The native transaction loads reserved metadata slot
  21, conditionally loads its positive player record, admits only a live
  sector strictly above seven, and retains the live revenge-mode sector
  separately from that player's cached target. Eligible revenge emits the
  date, appends the BEL-terminated revenge row to news, prints that row, then
  prints the date again. Every lane freshly reloads slot 21 and clears offset
  105 with the shipped dirty-zero image `00 00 28 00`; the former native
  helper instead reused its first overlay, emitted no direct rows, and wrote
  a canonical zero. Raw eligible, live-sector-seven, and numeric-zero fixtures
  pin all five qualified transfers, exact bytes and unrelated-field
  preservation. The roaming heading and group loop beginning at `30EF` remain
  the next connected frontier.
- Upstream commit `636ad6d2` resolved the roaming-split gate conflict. The
  native reducer now matches `YTMAINT:3133..315A`: either `size[g] < 1` or
  `location[g] < 1` admits the threshold test, so a positive-size group at
  location zero is overwritten by a qualifying four-draw split while exact
  size/location one bypasses it. It preserves the unrounded
  `group1 >= top_score/2000` comparison, zero-range no-draw result, group-1
  subtraction and headquarters assignment. The formerly absent `30F7/3109`
  prowl/date rows are also connected. The following physical discovery
  transaction now pins rejected-current initial redraws, one ordinary versus
  25 live-revenge candidate attempts, every candidate-sector GET, exact
  fighter/owner/planet interest, cached-revenge replacement and early exit,
  unconditional ascending cloak draws, strict equality, live-revenge cloak
  bypass, protected-sector discovery without final replacement, and complete
  failure draw totals of 52 and 1,276. The post-discovery reducer separately
  pins group-16..20/top-target precedence, unconditional group-20 override,
  and final weak-group headquarters redirection. All 45 split/discovery/
  override transfers and the distinct constant-two loop-entry edge are
  candidate; raw runtime and failure prefixes, and joined route/arrival
  composition remain open.
- Upstream commit `872959a7` resolved the immediate `YTMAINT:34B2..34DB`
  group-row presentation conflict: terminal control-byte handling applies
  only to string operands, while raw MBF32 numeric operands go through the
  shared formatter. The native live pre-route composer therefore advances
  group 2 from column 11 to 14, applies the QuickBASIC comma zone, and emits
  the exact group-2/size-1 row as 36 payload bytes followed by direct-screen
  CR (37 bytes total). Exact group-2 and group-16 fixtures pin both the short
  and wider numeric layouts.
- The live roaming controller now retains each selected target across
  successive one-hop route/arrival operations until exact SINGLE equality.
  A surviving group that finishes inside protected sectors 1..7 takes the
  `3E0B -> 31E6` fresh-target back-edge without repeating the split gate;
  zero, negative and exact-eight locations advance instead. Pure boundary
  fixtures and the live owners make all seven route-completion/retarget
  transfers candidate. The preceding first-pass gate is now separately
  pinned: exact group 20 at the retained top-player target goes through the
  completion test before using the already-built next hop. That operand is
  not the selected route target after a weak-group headquarters override.
  Its four transfers are candidate. The extracted live traversal also has a disk-backed
  `1 -> 2 -> 3` fixture proving two successive empty arrival hops, exact
  target completion, graph-cache reuse, disconnected route preservation,
  and the post-route size/location-below-one clear (including CINT-rounded
  location `0.6`). Its seven route-next/exhaustion transfers are candidate.
  The joined post-sector controller now always calls planet arrival before its
  exact `size < 1 OR location = 0` exhaustion gate. A surviving route visit
  scans every configured player record and consumes one cloak draw for each,
  even for a different sector or group 20; only an ordinary same-sector group
  with `cached cloak - .330000013 <= RND` enters player arrival, and equality
  is admitted. Thirteen post-planet/player-scan transfers are candidate; a
  two-hop/two-player fixture pins four draws without any player GET, while
  nonempty combat and retained-hunt-player joined paths remain open.
- The disconnected Xannor route lane now reproduces both observable copies
  of its dynamic error. `yt_maintenance_route_next_hop()` first appends the
  exact `3898` news line, and the Xannor route owner then uses
  `yt_maintenance_compose_xannor_path_error()` to emit the same
  `STR$_SINGLE`-formatted row at direct-screen site `38A8`. The fixture pins
  source 1, destination 5 as 62 payload bytes plus CR, address ownership and
  final column zero. With that last omitted presentation site connected, all
  152 catalogued Xannor phase/player/planet/sector sites and their already
  mapped transfers have native owners, so `ytmaint-xannor-output` is now
  candidate. All nine native suites and all 79 focused upstream
  AI/Xannor/output tests pass; the complete groups-2..20 raw transaction,
  stdout/local framebuffer reduction and staged failures remain open.
- The group-20 terminal suffix now preserves the retained hunt-player token
  for its optional second player-arrival call and owns the separate shipped
  fighter increment. The latter runs only for exact group 20 with positive
  size and location, freshly reads the CINT-selected sector, adds exactly one
  fighter, and writes it before final persistence without changing owner or
  planet link. Skip and physical-success fixtures make its five gates
  candidate; the joined retained-hunt-player predicate/call composition is
  still open.
- The retained hunt-player decision itself and the final group-loop exit now
  have explicit live owners. Any numeric-nonzero token, including negative,
  selects the second player-arrival call only for exact group 20; zero and
  group 19 bypass it. Groups 2 through 19 advance to the next split gate,
  while incrementing group 20 to terminal 21 enters final persistence. Pure
  boundary fixtures make those four second-attack gates and three loop/exit
  transfers candidate; joined physical second-call and full 2..20 ordering
  remain open.
- The final `YTMAINT:3E3C..3F72` Xannor persistence transaction now has a
  public native owner and disk-backed 20-slot fixture. It writes each group
  location to reserved offset 105 before any optional destination access,
  uses `CINT(location)` for a positive group's host record, accumulates
  multiple groups at one sector, forces owner `-1`, and preserves the planet
  link and unrelated bytes. Zero locations or sizes perform metadata-only
  writes. A range-failure cut pins the non-rollback prefix after the current
  metadata PUT; all seven persistence gates/back-edges are candidate.
- The first bounded Mercenary slice now follows the shipped
  `YTMAINT:3F73..464B` phase order instead of combining tax and hiring ahead
  of the base pass. It emits the date/blank prefix before the ascending port
  scan, accumulates tax as SINGLE with two independently evaluated
  `INT_S(treasury / 10)` expressions, skips PUT for numeric-zero treasuries,
  emits the misspelled tax row screen-first then news, and places the phase
  heading/report/base-check before base recovery. Base rebuild output/news
  precede fleet funding; positive `INT_S(tax_pool / 10)` strength creates ten
  independently placed fleets before the exact hired row/news. The corrected
  base transaction writes the rebuilt planet before candidate selection,
  admits only nonpositive planet-link candidates, links the accepted sector,
  then performs the unconditional fresh planet GET and owner/ground/bank
  floor PUT. A raw
  four-port fixture pins treasuries `99,100,101,dirty-zero` to
  `90,90,91,dirty-zero`, pool 29, strength 2 and three writes, while the
  presenter pins the exact ten-row tax/rebuild/hired byte stream and source
  addresses. A second raw fixture pins missing and existing base passes, one
  fresh date sample only for rebuild, planet-PUT-before-RNG ordering, an
  occupied candidate retry, negative-link acceptance and all explicitly
  canonicalized rebuild fields while preserving last-minute/plasma/fighters
  and the unbound tail. A third fixture pins an occupied funding retry,
  sectors 2..maximum selection, ten independent placements, 11 draws, raw
  fighter/owner-only overlays and the zero-strength no-draw path. All 36
  tax/base/funding controller transfers are candidate; raw runtime state and
  staged physical failures remain open.
- Upstream commits `55f47117` and `1248add9` reconciled all three Mercenary
  eager-RNG gates with the executable, and the native controller now follows
  them. At `46A1..46D2` every positive-fighter sector consumes its defection
  draw before owner `>1` is ANDed with `RND*100 > fighters`; nonplayer owners
  cannot defect but advance the generator. At `4847..4873` every otherwise
  eligible positive Mercenary fleet consumes the hold draw before the
  positive planet-link predicate is ANDed with `RND < stored .66`, so a
  zero-link fleet necessarily moves and begins target selection at the next
  draw. At `4FB1..4FD6` every non-merge destination consumes the selector draw
  before negative-owner is ORed with `RND > stored .95`; Xannor is attacked
  inevitably, but combat begins after that draw. The raw defection fixture
  now pins five draws across five positive defenses, including owners `-2`
  and `1`, while boundary reducers pin the link and owner predicates. Native
  raw-disk fixtures now also pin the descending movement sweep, route choice,
  merge/join behavior, selector-before-combat order, exact Won/Lost output,
  and the 201-on-201 combat quantum. A newly exposed upstream conflict blocks
  the wider composition: canonical prose says one FIFO-route hop, but
  executable joins `4EE7`, `4F5F`, and `51E0` reach `5315 -> 4932`, where the
  route-workspace cursor is used to copy another successor before the zero
  test. This appears to continue after an empty hop, friendly merge, or
  winning attack; a failed route instead reaches the zero-successor target
  install directly. Do not choose between those contracts or map
  `495A..49D1` until the analysis project reconciles them. Raw
  route/FOR/FIELD state and staged physical failures also remain open.
- The Mercenary defection controller is now fully mapped through its bounded
  `4661..47C6` loop. All ten qualified first-entry, positive/skip,
  post-draw-conjunction, news-call, terminal and ascending back-edge
  transfers are candidate. The existing five-sector raw fixture proves the
  first/final loop boundaries, five eager draws, owner `-2` and owner-one
  no-defection lanes, threshold failure, sector PUT before owner GET,
  embedded-NUL direct output before identical CRLF/DOS-EOF news, and
  unrelated-record preservation. Raw BRUN temporaries and staged physical
  failures remain open.
- The independent `YTMAINT:49EC..4C7F` routed-arrival mine slice is now
  candidate with all 20 qualified transfers mapped. It reloads the arrival
  sector, admits a nonzero reachable minefield, executes exactly one fixed-
  bound iteration with two nested draws, clamps damage to the moving force,
  and performs a fresh sector GET plus mine-only overlay/PUT only for positive
  clamped damage. It then emits the exact hit row news-before-screen and the
  killed-or-loss row in the same order. A raw disk fixture pins survivor,
  killed, zero-damage fractional-clamp, and no-mine paths; exact draw counts;
  unrelated-byte preservation; the misspelled `mercenariers`; and the CRLF/
  DOS-EOF newspaper image. Joined driver/raw runtime state and staged physical
  failures remain open.
- The following `YTMAINT:4C7F..4EC0` linked-planet absorption slice is
  candidate with all 13 qualified transfers mapped. The native transaction
  evaluates both owner predicates and the `CINT` planet link, loads the linked
  planet, truncates its fighter reserve with SINGLE `INT`, preserves the raw
  stored-length name, and writes the pinned `00 00 80 00` numeric-zero reserve
  before freshly reloading and overlaying the arrival sector. Its final force
  is one DOUBLE sum converted once to SINGLE; planet owner is deliberately
  unchanged. A binary-name disk fixture pins intermediate-hop capture versus
  same-target suppression, positive versus zero planet-fighter reports, the
  distinct screen/news ordering of the two rows, exact DOUBLE/SINGLE text,
  and mutation of only planet fighters plus sector fighters/owner. Joined
  movement state and staged physical failures remain open.
- Upstream analysis commit `ab8e6588` resolved the only open Xannor-arrival
  conflict: `YTMAINT:7522..7540` skips deployed-defense combat for exact owner
  `-1` or zero. Owner-zero defenses remain unchanged and consume no draw;
  actual defense destruction clears both fighter count and owner. The native
  `yt_maintenance_xannor_defense` reducer and its injected provider fixtures
  pin those consequences, and the live maintenance path uses that reducer.
- `YTMAINT:5811` now has a native Xannor player-arrival transaction rather
  than combat arithmetic without its observable consequences. It preserves
  the distinct fighter/shield equality predicates, emits the two loss radios
  before their respective fresh-overlay PUTs, updates live caches, composes
  Xannor death cleanup, appends and displays the exact result row, and emits
  the final killed-player radio. A real database fixture pins the complete
  killed-player news/radio/persistence transaction; both incoming joins and
  all 70 body/helper/return transfers are candidate.
- `YTMAINT:5EFD` now has a native Xannor planet-arrival transaction rather
  than arithmetic without its direct-screen copies. It preserves the
  owner-`-1`/inactive zero-draw skips, ground-before-economic draw order,
  shipped group-location large-quantum anomaly, production stock caps and
  owner/group/location/link/active-marker clears. The attack prefix now uses
  the exact SINGLE formatter, and both result rows append to news before their
  direct-screen copies. Disk-backed fixtures pin Xannor-fighter and planet-
  destroyed endings; its caller join and all 57 body/helper/return transfers
  are candidate. All nine native suites and all 77 focused maintenance
  AI/output/physical-owner tests pass.
- The logical local-SysOp slices now distinguish F4 from the player command-X
  sound path. `YT:A9E1`/`YT-SUB:C0E4` toggle the local-speaker cell, mirror
  only for nonzero mode, and emit three raw local-only prints with no serial
  or `PLAY`. `YT:B66A` preserves its exact-mode-one early return and owns the
  F9 ON status/row-24 notice versus OFF cursor-hide/CLS sequence.
  `YT-SUB:20A7` owns the F8 prompt and time-replacement arithmetic, including
  independent TIMER samples, SINGLE numeric spacing, blank preservation,
  VAL radix/non-numeric behavior, upper-only 90-minute clamp, negative
  deadlines and overflow-before-store. `YT:B6D7` now owns the exact
  time-body-before-stale-B05D order, including mutation-visible replay and
  retained replay-failure prefixes. `YT:B3FB` now owns a resumable logical
  F10 chat transducer: it captures the SINGLE-rounded remaining deadline,
  emits the raw local header through a dedicated presentation owner, samples
  local before exact-mode-zero serial override, checks carrier every poll,
  preserves the compiled lexicographic extended-key predicate, and models CR,
  raw backspace, ordinary output, wrapping and delayed ESC exit. Its normal
  suffix uses two independent TIMER samples, clears the command accumulator
  and replaces the queue with exactly CR; abort and carrier-END paths skip
  that cleanup. Their 60 logical body/call/return transfers and combined
  presentation family are candidate; physical KEY registration, raw INPUT/
  editor, complete greeting/stale-context replay, event-stack, carrier-END
  composition and exact host-framebuffer seams remain open. All 56 focused
  upstream SysOp-control/output tests pass.
- The bounded `YT:BA00` F5 owner is candidate. Its native event result pins
  the `BA00` compiler checkpoint, `BA01` no-return END, fixed shared BRUN
  cleanup/DOS-exit order, local `END_CLEANUP`, same-F5 pending residue and
  status zero. The physical cleanup observation, inherited framebuffer/file
  registry and nested event stack remain separate seams; 90 focused upstream
  process-exit/root-event tests pass.
- `YT:B2DA` now has a bounded native main-module error-dispatch owner. It
  preserves ERR-24/57 retry precedence before output, ERL-40000 precedence
  over resumable errors, the 5/6/13/15 gameplay route, exact forced-local
  SINGLE/INTEGER debug text, binary-safe missing-file construction, and the
  exact fatal `ERRORS.DOR` DATE$/TIME$ record. Its root, all 16 internal
  dispatch/call/jump transfers and presentation family are candidate; 80
  focused upstream main-error/router tests pass. Saved-IP retry execution,
  A6B4/gameplay joins, inherited B05D/pager/carrier/framebuffer state,
  physical YTNEWS/ERRORS.DOR transactions, active-handler second faults,
  captured KEY/FIFO state and the deferred BA00/normal-END seam remain open.
- `YT-SUB:45F7` now has a bounded native shared error-dispatch owner. It
  preserves ERR-24 retry precedence before output, the ordered ERL routes,
  the exact 64003/64004/64005/64006 alias/DORINFO boundaries, exact
  SINGLE/INTEGER debug text, all direct diagnostics and ERR-53/64
  refinements, the autopilot session/news row, and the three generic news
  rows. Logical destinations distinguish snoop-gated local diagnostics,
  ordinary session-plus-news output and news-only output. Its root, all 49
  internal dispatch/call/resume/terminal transfers and presentation family
  are candidate; all 90 focused upstream shared-handler/router tests pass.
  Retry execution, normal END, inherited ANSI/cache/local/remote state,
  physical YTNEWS transforms, active-handler second faults and every
  construction/output failure prefix remain open.
- `YT-SUB:52F2` now has a bounded native logical startup owner without
  touching the OpenDoors API. `yt_startup_model.c` pins the first-space-
  retaining command split; exact twelve-read BRUN grammar with discarded
  NUL, retained bare LF, CR/optional-LF, unread DOS EOF, every ERR-62
  truncation boundary, 256-byte fields and ignored line 13; port/layout/BDA
  selection; arbitrary nonzero UART divisor; destructive framing; OPEN spec;
  restored divisor; independent TIMER samples; canonical name; final clear
  mask and pre-ANSI zero-divisor state. Dedicated presentation owners cover
  all six local-only rows. An observation-driven event tape now preserves the
  exact BDA writes, pre-OPEN UART sample/restoration, serial OPEN success or
  error, local-sound store, one-second wait result, carrier byte and successful
  post-OPEN divisor restoration for local, zero-divisor, OPEN-fault,
  carrier-drop and remote-ready outcomes. The root, presentation family and
  56 logical internal transfers are candidate; all 73 focused upstream
  startup/uppercase/handler tests pass. The entry state pins installation of
  shared handler `45F7` before command inspection and exact empty-command END
  intent. Remote framing is destructively compatibility-uppercased in place
  only after a nonzero divisor, and the ordered tape now composes the single
  carrier sample with ordinary return or snoop-gated local notice, CLOSE and
  END without late UART restoration. The post-OPEN wait remains explicitly
  deferred. Exact raw heap/file-registry/framebuffer state, physical
  UART/COM/CLOSE/END accepted-prefix behavior and runtime-failure compositions
  remain open.
- Coverage prose and the root/presentation/transfer rows are reconciled with
  `yt_pager.c`, the complete successful transaction fixtures, terminal
  vectors, merged FIFO behavior, and bounded AB36 `/R`/semicolon post-submit
  transforms, save/repeat blank+B05D notice vectors, the exact paired-local
  and serial pending-input drain, and the exact successful `YT-SUB:94FD`
  SINGLE-deadline/TIMER/local/serial/midnight state machine. Press-any-key now
  uses those shared input owners rather than a duplicate merged-key timer.
  Remaining wait raw-runtime/fault/retry and physical-adapter proof,
  notice-sampled input/cuts, physical overflow/failure, asynchronous SysOp,
  startup/shutdown, and cross-platform OpenDoors gaps remain explicit.
- The pinned OpenDoors API audit corrected the old stale-copy premise:
  DOS/DOS32 and Win32-console builds support a one-row/custom personality and
  its custom-hotkey dispatcher; Win32 GUI uses its native command surface and
  Unix-like builds are headless. The physical adapter and platform-result
  fixtures still need implementation audit before those boundaries can close.
  This is not a reason to seek new Yankee Trader session transcripts.
- The vendored OpenDoors API and ABI are immutable implementation
  dependencies. Do not change `third_party/opendoors` behavior to reproduce a
  Yankee Trader convention. Adapt Yankee Trader through the documented
  OpenDoors contract; report any independent OpenDoors defect upstream in
  that project.
- The rooted `YT:03A2` startup owner now restores the silent sector-1-to-2
  route prime and its nonfatal two-blank emphasized failure diagnostic,
  preserves the optional ANSI stage, draws the exact snoop-gated local-only
  row-25 status with the first-opening empty alias, and always follows with
  the shared `YTOPEN.ASC` viewer. The row-25 fixture pins all ten local
  operations and 63-byte expression clipping. The viewer now has its
  recovered blank/notice/blank framing, key/count initialization, marker
  colors, saved-foreground cleanup and final blank. This is still candidate:
  ANSI is a whole-file shortcut and the viewer still needs exact physical
  LINE INPUT/EOF/Q, cache, missing-file and failure-prefix fixtures before
  the composed opening can be promoted.
- The shared `YT-SUB:13D0` title/registration presenter now emits its exact
  remaining eleven title rows after the startup caller's foreground-six
  blank, including centered literals with their shipped trailing spaces and
  the version row. Registered and evaluation results use the recovered
  centered-row/blank/centered-row/blank suffix and 2/10-second waits at both
  startup and main-command V callers. Invalid keys now use four local BASIC
  beep events around a forced local-only uncentered row and terminate without
  falling into normal exit. This remains candidate because `YT-SUB:A46C`
  still uses host line reading, host square root/DOUBLE arithmetic and lacks
  beta/anti-tamper plus complete physical failure behavior.
- The post-opening controller now matches the recovered `YT:0440–04A7`
  order: foreground-five `0317` Initializing, silent date serial, lockout,
  then non-denied `0317` Welcome and `02FC` Searching. A native fixture pins
  the complete 83-byte ordinary ANSI stream and final three-row pager/nonstop
  state. Matching lockout now emits the exact direct blank, BEL-wrapped
  uppercase revoked row and configured SysOp contact row with separate bold
  requests, then the ten-second wait and terminal disposition. Exact BRUN
  lockout line grammar, full denial bytes/state/effects, carrier/input cuts,
  inherited framebuffer and physical failures remain prerequisites.
- The `YT:B6EF` alias gate now preserves its silent last-match existing-map
  return and uses the recovered new-player dialogue: three `0317` notices,
  raw `031F` prompts, case-preserving `0345` alias input, exact normalization
  and rejection strings, foreground-three bold identity confirmation, and a
  complete-response `0357` comparison accepting only exact `Y`. Persistence
  precedes the queue-clearing bold/blink success notice and final blank. The
  native fixture pins the complete 308-byte ANSI accepted stream and final
  pager/style state. Rejection/retry, first-poll terminal, all carrier cuts,
  raw accumulator/file effects, endpoint/framebuffer and physical append
  failures remain explicit prerequisites.
- Alias key preparation is now a separate tested name-layer component rather
  than inline session logic. Its vectors pin real-name substitution, comma
  and quote replacement, normalized-empty retry, reserved detection before
  clipping, the ordinary one-word trailing space, the distinct 40-byte
  one-word clipping result, and the temporary appended-space accumulator.
  All rooted alias predicate transfers are now assigned; their full retry,
  terminal, carrier and physical-failure compositions remain open.
- Player admission now keeps the recovered returning-name and vacancy scans
  separate. A returning match emits the foreground-two caller blank, applies
  the same/new-day mutation and player PUT before login news, then classifies
  prior death. The exact 37-byte same-day-alive, 104-byte Xannor rebuild and
  152-byte same-day-self-denial ANSI streams are pinned. Ordinary rebuilds
  use a shared visible constructor whose blank and built row precede its
  player read and write, followed by the recovered five-second wait.
- A returning miss now starts the recovered fresh ascending raw
  `name_length < 1` vacancy scan. The full branch emits the exact rejection
  before its news append and terminal disposition. A vacancy emits the exact
  retention/constructor rows, performs constructor and identity writes in
  order, appends new-player news, and then runs the raw instruction prompt
  with blank/N/Y first-byte selection. Native fixtures pin the exact 217-byte
  default-no and 97-byte full-game streams. These admission lanes remain
  candidate: malformed/fractional scan domains, complete joined persistence,
  pager/carrier, local framebuffer, physical-failure and terminal-cleanup
  fixtures are open.
- The shared instruction Y/N reader now has explicit native output-source
  scratch state separate from the case-preserving command accumulator. Its
  transform copies and compatibility-uppercases the response, truncates only
  the copy to one byte, and classifies blank/N/Y/invalid without modifying the
  original accumulator. Focused vectors pin `no -> N`, `YES -> Y`, blank,
  leading-space invalid and capacity failures; the complete invalid-X then N
  admission stream is the recovered 279 bytes with the exact bold/normal SGR
  transition and retry queue clear. Full Y/viewer and terminal/failure joins
  remain open.
- The shared visible ship constructor now performs its recovered fresh
  configuration-record GET, target-player GET, defaults/team mutation and
  one target PUT after the two caller-visible rows. A native persistent test
  makes cached defaults disagree with record 1 and proves that record 1 wins,
  while name, stored length, score and the unbound tail survive and team is
  cleared. Raw malformed-field behavior and each ordered read/write failure
  prefix remain open.
- The new-player identity overlay is now a separate fresh-GET/PUT transaction.
  A 50-byte name fixture proves 41-byte fixed-field truncation with the full
  stored length, redundant team clear and preservation of the rebuilt ship,
  cached score and unbound tail. Constructor fixtures now also pin missing
  configuration, missing target and read-only first-PUT failure stages; the
  identity helper pins its read-only second-PUT failure without changing the
  durable target. Joined two-PUT/news images and physical partial failures
  remain open.
- Admission/login news now uses three shared typed writers rather than
  caller-local `snprintf` rows. Native physical fixtures pin the canonical
  63-byte full-game, 54-byte new-player and returning-login DOS-EOF files,
  plus bounded formatter overflow. Their caller order remains rejection then
  full-news then END, constructor/identity PUTs then new-player news then
  instructions, and daily PUT then login news then death classification.
  Joined mutation/news failure state and physical APPEND prefixes remain open.
- Returning-name comparison now applies `CINT` to the stored length and
  compares the resulting prefix of the fixed 41-byte field, retaining padded
  bytes, clamping a positive request above 41 to the field, accepting values
  that round to zero, and routing negative/overflowing conversion to error.
  Referenced-killer selection now uses the exact raw `killer > 1` gate before
  truncating the GET record and extracts the same fixed-field prefix. The
  final rounded-zero conflict was resolved by analysis commit `2f01684e`:
  only raw numeric zero skips at `1801`; raw nonzero `.4` branches through
  `CINT(0)` and `LEFT$(name,0)`, then emits the bold+blink suffix-only
  ` destroyed your ship!` row. `yt_player_killer_row`, score vectors, and an
  exact ANSI presentation vector now pin that distinction.
- The normal post-login lane now performs the two exact fresh-player repair
  stages independently: turns below one cause their own flushed PUT before a
  second GET, and holds above maximum cause the commodity reset and a second
  flushed PUT. It then runs the shared local-only row-25 status with the
  nonempty cached alias, Info, the caller-owned blank, exact 17-byte B05D
  prompt, successful `YT:07F5` 99-second wait, and direct CRLF blank before
  radio. It no longer aliases the starred 33-second `YT-SUB2:25D5`
  press-any-key utility. Repair tests pin both writes, MBF32 skip boundaries,
  preserved record bytes, and first- versus second-PUT failure residue.
- The following gameplay-reentry front now recognizes only numeric-zero
  deployed fighters as empty; negative nonzero counts continue through the
  self/owner/team predicates, and team friendship requires the current team
  to be nonzero. Pure native predicates also pin the ordered two-cell
  black-hole test and strict-positive-mine/exact-zero-suppression gate. The
  caller-owned 65-byte hostile warning is restored before `YT:09C9`.
  Friendly entry performs its fresh hydration, foreground-two
  blank, cached-time/no-terminator B05D prompt, and AB36 editor reset before
  the inactivity sample; an inherited queued byte is consumed before any
  non-local serial replacement. Focused route, warning, prompt, and input
  fixtures cover these native seams; the full Info/radio/scanner/RNG join and
  physical failure prefixes remain open.
- The fresh hostile-fighter menu at `YT:09C9` now performs its independent
  player hydration, selects foreground three, constructs the fighter ratio
  from the two DOUBLE operands, and uses the exact 35-byte no-terminator
  option prompt followed by the compatibility-uppercase editor. Empty input,
  exact `S`/`I` gates, and the complete-string `INSTR("AQBDWT", response)`
  dispatch are separated; substring aliases such as `AQ`, `QBDWT`, `DWT`,
  and `WT` remain accepted while `YES` and `A ` are invalid. The invalid
  branch owns its exact `02DB` row, queue clear, trailing direct blank, and
  prompt-only retry. Help owns its two direct blanks and nine B05D rows before
  a fresh hydration; Info similarly rehydrates on return, while Sector jumps
  through normal gameplay re-entry rather than the adjacent scanner. Native
  fixtures pin the canonical 73-byte Attack, 119-byte invalid, and 257-byte
  Help ANSI streams, pager state, local color, ratio formatting, and dispatch
  matrix. Downstream Quit/Attack/Bribe/Mine/Warp/Team action bodies retain
  their separate component gaps.
- The shared Quit owner now routes all five hostile whole-string aliases,
  exact ship-computer/planet `Q`, and first-byte main `Q` (including trailing
  text) into the same first-byte confirmation contract:
  foreground-seven `<Quit>` is a
  B05D row with no leading blank, `Are you sure (Y/N)? ` is raw and may
  repeat silently, `YES` confirms, `NO` and blank cancel, and an invalid
  first byte sets bold and clears the complete queued-command tail before
  retry. Hostile cancellation performs no scanner/warning work and starts a
  fresh independent hostile-menu hydration; computer cancellation returns
  directly to its fresh A41C prompt and is pinned at 148 ANSI/118 plain
  bytes, while main cancellation is pinned through its ANSI/plain `Qjunk`
  suffix and planet cancellation takes the direct fresh-menu join.
  Confirmation enters the existing
  normal-exit child once and marks the session terminal so the outer native
  runner cannot replay that child. `0345` now uses the already modeled
  save/repeat/semicolon post-submit transaction, so typed `Q;N;A` retains the
  exact command stacking needed by these cycles. The native hostile heading/
  prompt/N stream and first-byte decision vectors are also pinned; the joined
  normal-exit/scoreboard and physical failure prefixes remain separate gaps.
- The confirmed normal-exit owner at `YT:[0206,02BF)` no longer performs a
  synthetic player reload before Info, so the caller's cached heading name
  remains distinct from the later fresh Info FIELD. It now follows the exact
  direct blank, `031F` no-newline `Generating ScoreBoard`, four raw generator
  dots, direct blank, nonstop viewer, registration attention/ten-second wait/
  blank, and `02FC` LFCR BBS-return order. Confirmed hostile and computer
  Quit enter this owner once; the outer runner cannot replay it. Native
  presentation pins all four registered/evaluation ANSI/plain tail framings,
  and all 33 normal-exit/computer-Q oracle cases pass. Complete full-stream
  native joins, raw registration-CINT/FIELD/cache/generator state, endpoint/
  carrier/F8 cases, physical CLOSE/END and failure prefixes remain open.
- The ordinary planet prompt at `YT:[3D14,3DAD)` now uses its two distinct
  current-player hydrations, three ordered DOUBLE free-holds subtractions,
  exact `You have<STR$> free cargo holds.` `0317` row, caller blank,
  foreground-six cached-time prompt, intervening silent updater and `031F`/
  `0357` join. Exact whole-string `Q` enters the shared Quit owner and N/blank
  cancellation jumps with no adapter output into a complete new prompt pass.
  Native fixtures pin the canonical 206-byte ANSI and 186-byte plain cycles
  with final line count two. Help now owns its two framed and fifteen ordinary
  rows and is pinned through the fresh prompt at 524 bytes; invalid input uses
  the queue-clearing bold/blink helper and is pinned through reprompt at 201
  ANSI bytes. Raw distinct FIELD/updater/cache state, alternate input/mode/
  failure paths, confirmed full exit and remaining action handoffs stay open.
- Blank planet-command input now follows the executable's direct
  `YT:3DC0 -> YT:4551` default jump into Take All. The native owner retains
  the prompt updater's transient DOUBLE `Q[]`, hydrates a fresh player,
  commits all four cached-Q weapons before emitting `Taking:` and the four
  weapon rows, overlays a fresh planet from the same cached quantities, then
  transfers Equipment, Organics and Ore in descending order through fresh
  player and planet snapshots with each pair of PUTs preceding its row.
  Native reducers pin the canonical `411/7/9/13` weapon result, Equipment's
  first claim on 65 free holds, corrupt negative-free transfer, and the
  explicit DOUBLE-to-SINGLE halfway case. Presentation pins the exact
  149-byte body and complete 305-byte blank/default/body/fresh-prompt cycle.
  All 32 Take-All internal transfer rows plus the three blank/default edges
  are mapped candidate. Take One now emits all seven exact title payloads,
  including the five shipped closing-parenthesis typos, derives maximum and
  validation stock from transient `Q[]`, uses the direct blank plus `031F`/
  `036F` prompt/editor sequence, performs stock-before-capacity errors through
  `02DB`, and on acceptance applies fresh player/planet selected-field
  overlays, subtracts the transient cache, and performs the final player
  hydrate. Native fixtures pin all title and field selectors, cached-Q versus
  fresh-field behavior, the canonical 36-byte accepted body and 82-byte stock
  error; all 70 internal Take-One transfer rows are mapped candidate.
  Noncanonical raw selector/VAL memory boundaries and raw FIELD/dependency/
  carrier/mode/local failure-prefix coverage remain open.
- Planet Transfer at `YT:[4958,4F53)` now has an exact native owner. Its
  selector preserves whole-response `INSTR("CSFMB", response)`, including
  admitted `SF` fallthrough to the common tail, while blank and absent
  substrings return immediately. Cargo uses the pre-check player hydrate and
  updater snapshot, exact all-zero predicate, three-item transient catch-up
  arithmetic, fresh player/planet commits and its uniquely placed pre-loop
  blank. Plasma, missiles and mines transfer cached player values against
  cached `Q[]` into independently fresh records without validation. Fighter
  retains its DOUBLE count prompt, `036F` E-clear, SINGLE `VAL`, positive
  fractions and zero, silent bounds rejection, and cached selected-field
  arithmetic. All accepted exact branches and substring fallthrough execute
  the fresh-player/updater/selector-four-sound tail; the alternate sound arm
  remains qualified dead. Native presentation pins all ten canonical
  131/132/133/175/155/173/169/166/178/216-byte streams, reducer fixtures pin
  selection and mutation semantics, all 52 focused upstream Transfer/action/
  full-cycle tests pass, and all 68 qualified body edges are candidate.
  Raw FIELD/updater/cache joins, every ordered failure prefix, noncanonical
  VAL/runtime forms, physical endpoints and complete fresh-prompt joins remain
  open.
- Planet Bank at `YT:[4F53,50EC)` now uses the cached runtime planet name and
  player credit while obtaining its bank balance from the required fresh
  planet GET. It emits the exact title/blank/DOUBLE-availability prompt,
  cancels blank or E-cleared input before `VAL`, applies `INT_D`, tests the
  savings error before the separately rounded `D(D(C-T)+B) < 0` credit error,
  and accepts zero as a full transaction. Acceptance performs a second fresh
  planet GET/target overlay/PUT, replaces the caller credit cache, emits the
  positive-or-zero success row, calls selector-four sound, computes
  `S(D(B-T))`, and only then runs the fresh-player GET/SINGLE-add/`INT_S`/PUT
  credit helper. Native fixtures pin the canonical 213-byte accepted,
  161-byte zero, blank cancel and both error streams; reducers pin independent
  fresh fields and the 16,777,217 target/tie cases. All 42 focused management/
  action/full-cycle oracle tests pass and all 21 Bank edges are candidate.
  Raw cache/FIELD/effect joins, ordered failures, noncanonical VAL/runtime,
  endpoints and fresh-prompt composition remain open.
- Planet Productivity at `YT:[50EC,5464)` now begins with the required fresh
  player hydrate and silent updater, then emits the exact double-space
  explanation, DOUBLE credit row, explicit blank and `-+>` prompt. Blank,
  E-cleared and nonnumeric input pass through `VAL` to the common silent
  below-one return; accepted spend uses `U=D(T/250)` and emits success before
  mutating all three P caches. The recovered SINGLE old/new sums, 2500/25000
  divisors and exact plasma multiplier drive four dynamically omitted `031F`
  fragments; only a nonzero fighter delta adds the ending blank. Debit uses
  `-S(D(U*250))` through the fresh-player credit helper, then an independently
  fresh planet record receives `P-A` base rates and cached Q stocks before its
  PUT and the final player hydrate. Native fixtures pin the 241-byte canonical,
  281-byte all-fragment, 213-byte one-credit and unterminated later-only
  streams; reducers pin deltas, fresh-field preservation and the 16,777,217
  debit boundary. The same 42 focused management/action/full-cycle oracle
  tests pass and all 35 Productivity edges are candidate. Raw updater/FIELD/
  cache/effect joins, ordered failures, noncanonical runtime forms, endpoints
  and fresh-prompt cycles remain open.
- The shared Clearance routine `YT-SUB:[481C,4AD5)` and the Earth report/front
  `YT:[6CB3,7401)` now have exact native owners. Clearance preserves the four
  trigger/candidate predicate sequence, strict per-item normalization,
  interleaved announcements, percentage arithmetic, and selector-one plus
  trailing-blank framing under the authorized platform-CSPRNG departure.
  Earth reads logical port 1, observes date then time, emits the optional
  owner row, calculates all prices before mode-one clearance, preserves the
  exact report-seen redraw rules, reloads the player, and builds all nine
  fixed-column rows through the shared pager. Its dispatch performs
  `VAL`/SINGLE/`CINT`, then exact S/I, then stores fallback zero before the
  whole-response `INSTR("LM0C", response)` handoffs; the shipped
  `INAVLID CHOICE!` path redraws through clearance. Native reducers pin zero,
  ordinary, fractional and maximum discount prices, affordability, and the
  complete selector-substring grammar. Presentation pins the canonical
  652-byte no-owner/plain response-0 stream at SHA-256
  `fc61da4bf8896888ba17405bcf571deb8654b90ea3560f8a6e92bd9609d815be`,
  its final pager count 16, and Clearance plain/ANSI sound framing. All 32
  focused upstream Earth/Clearance oracle tests and all nine native CTest
  suites pass; all 53 Clearance and 63 Earth-front edges are candidate.
  Raw discount/reset/report-seen cells, injected provider failures, owner and
  sale variants, complete redraw/S/I joins, endpoint/local/carrier and every
  physical failure prefix remain open.
- The nine Earth purchase bodies at `YT:[7401,7CCF)` and their visible
  Lottery/Anti-Cloak helpers now have exact native owners. The six ordinary
  fixed-price actions use their shipped blanks, `031F` prompts, validation
  order, dedicated messages and SINGLE overlays. Common settlement now
  performs cached player PUT, fresh player debit GET/PUT, then gates a fresh
  Earth treasury GET/PUT from the cached owner and applies the one-percent
  self receipt. Anti-Cloak uses its raw confirmation, both corrupted patch
  descriptors, clear-before-GET target scan, fixed-field stored names,
  ordered colors/sounds/fade, fresh debit and no Earth receipt. Spies keeps
  affordability-before-cap, complete cap retry, one assignment blank per new
  spy, negative/duplicate target admission, signed CINT storage, active-spy
  rows, exact press UI and settlement. Lottery now owns the fresh limiter
  reads/writes, mode-zero Clearance, raw six-byte ticket retry, all six actual
  and 108 dummy draws, 116/117 waits, mode-zero backspaces, unconditional
  local cursor rewinds, multiset matches, award table, preliminary GET,
  per-match sounds, cached-name news, separate award credit and subsequent
  five-credit debit. Native presentation pins all six ordinary success
  streams, the malformed Anti-Cloak target stream, the negative-sector
  one-spy/press stream and the canonical 472-byte loss stream with 108
  backspaces; reducers pin receipt, Cloak/supply, duplicate-match and award
  arithmetic. All 27 upstream purchase-oracle tests and all nine native CTest
  suites pass. All 151 caller-body, 83 Lottery-helper and six Anti-Cloak
  helper edges are candidate. Authoritative distinct cached/fresh raw FIELD
  images, injected operation failures, complete RNG tapes, positive-award
  ANSI/local/news fixtures, every alternate/editor/pager/carrier/mode path,
  redraw joins and physical I/O/framebuffer proof remain open.
- The main missile/plasma command parent at `YT:[2059,2341)` now preserves
  the command-entry displayed-ammunition snapshot while performing the two
  shipped fresh player hydrations on every target pass. It tests no-turn
  before fresh ammunition, emits the exact missile/plasma target and quantity
  prompts, retains fractional in-range sectors, repeats invalid targets
  without refreshing the displayed snapshot, floors the quantity, and owns
  the exact no-ammunition, invalid-sector and excessive-quantity rows plus the
  accepted blank/finalizer/debit/write/typed-child order. Native model tests
  pin both prompt spellings and target/quantity boundaries; presentation pins
  the canonical no-ammunition, invalid-target retry and excessive-quantity
  byte partitions. The full raw dual-hydration
  state/effect fixture, remaining refusal partitions, active-shell joins,
  child resolvers, ordinary/fatal missile cycles and all failure prefixes
  remain candidate prerequisites.
- The ordinary main missile return now follows `YT:233E -> YT:161D` through
  the scanner-only current-sector display and fresh prompt; it no longer
  enters the gameplay hazard router. Missile opening, end-report, and the
  scanner's natural blank/sector/warp rows use the presentation transducer.
  A native joined fixture exactly pins the oracle's 331-byte plain and
  351-byte ANSI no-impact cycles, including `misile`, the LF/CR turn row,
  both scanner/prompt color transitions and the final prompt space. Raw
  FIELD/store/cache/RNG/by-reference state, terminal scanner residue, AB36
  arbitration, nonempty successors, local framebuffer and failures remain
  open.
- The projectile resolver's already recovered early terminals no longer
  bypass presentation. Missile/plasma openings, cruise end-report,
  same-sector `Missles self destructed!`, the two-blank unreachable-route
  diagnostic plus self-destruction suffix, Union Police row, black-hole
  leading blank and three-row plasma footer now retain direct-helper ANSI,
  local, mode and CRLF behavior. A native fixture pins the exact plain byte
  partitions. Defense, mine, player, planet and repeated-reroute state/effect
  compositions remain open.
- The cruise resolver's deployed-defense, sector-mine and player-impact core
  now follows the recovered physical ordering instead of mutating stale
  decoded records. Defense damage performs a fresh sector GET before its
  count/owner overlay and preserves the shipped dirty zero bytes; the mine
  phase repeats its probe GET, uses a second fresh GET for each PUT, decrements
  missiles only after a successful PUT, and re-enters after positive carried
  mines. Candidate admission now performs the ignored friendship GET before
  its separate player GET and only then applies cloak/provoker gates, without
  inventing fresh killed-by or sector checks. Ship damage uses eager scanner
  `CINT`, the dirty scanner zero, DOUBLE fighter accumulation, and fresh
  survivor/death overlays. The affected defense, mine, two-line attack,
  destroyed-player and carried-mine rows cross the bold/blink presentation
  transducer. Complete injected raw database/provider tapes and all failure
  prefixes remain required before this child can be verified.
- The cruise planet-impact tail now follows the recovered physical sequence
  rather than retaining the updater's decoded record as a persistence image.
  Admission uses `CINT` on the sector link; the production updater is followed
  by its independent planet GET, and the team predicate performs its physical
  current/candidate player GETs before the caller restores the planet FIELD.
  Numeric-zero ground forces skip their entire phase, while an admitted phase
  retains the updater's stale ore predicate, consumes its draw loop, reloads
  the planet, overlays only ground/owner, and reports only after the PUT.
  Productivity retains three eager draws per iteration, reports before its
  fresh six-field overlay/PUT, and all-zero destruction performs separate
  fresh planet and sector GET/PUT pairs with the shipped dirty
  `00 00 20 00` active/link zeroes before direct row, selector 3, and news.
  All direct planet rows now cross the presentation transducer. Complete
  injected raw-world, RNG, output/news, failure-prefix and caller-join fixtures
  remain required before this candidate child is verified.
- The plasma planet-impact tail likewise now separates the updater result from
  its independent planet GET while retaining the updater's stale ore cell.
  It emits the hit and productivity rows before persistence, then reloads and
  overlays exactly the six productivity/stock fields plus ground and the
  conditional owner clear. All-zero productivity performs independent fresh
  planet and sector unlink writes with the plasma path's canonical zeroes;
  destruction suppresses the surviving-ground row. Hit, productivity,
  destruction and surviving-ground output now crosses the presentation
  transducer. Raw injected transactions, far-array state, exact RNG arithmetic
  and every failure prefix remain open.
- The plasma deployed-defense, mine and player-impact path now mirrors its
  recovered fresh-record boundaries. Defense uses DOUBLE damage, explicit
  bold inheritance, a post-output/news sector GET, dirty `00 00 10 00`
  count/owner clears, PUT-before-Headquarters evaluation and the required
  post-defense sector reload. Mines retain entry-news-before-damage and
  destruction-news-before-bold-direct order, then use a separate fresh sector
  overlay/PUT. Player impact performs distinct entry, presentation and
  survivor/victim reads, removes invented fresh killed/sector gates, uses
  DOUBLE fighter damage, restores foreground after both bold rows, clears a
  killed victim's mines/scanner before its blink rows, and sends positive
  carried mines through the shared sector reload/mine body before restarting
  the player scan. The pre-scan planet link remains cached across the ordinary
  scan. Exact injected provider/world and every failure-prefix fixture remain
  open.
- Projectile traversal now sends the plasma per-hop row through the direct
  presentation transducer before its half-second wait, and the same-sector
  plasma shortcut applies the shipped numeric-zero by-reference origin side
  effect before resolving the destination at full energy. Defense-owner
  player access uses `CINT`; plasma retains its configured-positive-owner
  bound while cruise retains its recovered unbounded `owner > 1` GET. Cruise
  counterattack and Xannor-provoker arguments are now mutated through their
  caller pointers as soon as the resolver changes them, so later dependency
  failures retain the correct by-reference prefix rather than losing both
  latches at function return.
- Projectile identity rows no longer pass fixed-record names through C-string
  formatting. `yt_player_stored_name()` and the new planet equivalent preserve
  the exact `LEFT$` byte count, including embedded NULs, through deployed-owner
  rows, missile/plasma first attack news/direct pairs, victim-destruction and
  carried-mine warning rows, friendly-planet refusal, and hostile planet
  news/direct pairs. Native composers pin all of those binary-name shapes and
  their bounded failure results; the live resolver uses the same composers and
  byte-length news writer. Complete injected resolver transactions and their
  staged string/runtime failures remain open.
- The Xannor Headquarters victory body at `YT-SUB:[A7DE,A9B5)` now has a
  bounded native owner rather than the earlier generic text viewer and
  C-string append approximation. `XannorHQ.TXT` records use the recovered
  `LINE INPUT` grammar: NUL bytes are discarded, CR terminates and consumes an
  immediately following LF, bare LF is retained, DOS EOF ends only at record
  entry, and an unterminated final record is returned. The continuation emits
  raw `[PAUSE]`, consumes the 99-second wait, emits the direct blank and
  blink-one bold bonus, clears the synthetic queue, performs a fresh player
  GET and `floorf(SINGLE(old + 16000000))` credit PUT, emits three selector-two
  logical sounds, appends binary-safe banner/winner/banner triplets to news and
  radio, then freshly overlays current-player metadata into logical sector 21.
  Direct and missile callers still require their saved defense owner to be
  `-1`; plasma deliberately has no owner predicate. The presentation family
  and all 22 relevant caller/body/return edges are candidate, all nine native
  CTest suites pass, and all 66 focused upstream victory/world/caller/missile
  tests pass. A complete injected inherited-state/file/database/news/radio/
  wait/sound transaction, exact framebuffer, raw runtime residue, and every
  physical failure prefix remain open.
- The shared no-cancel port-name editor at `YT:[A9F0,AB36)` now owns its
  recovered dialogue and raw FIELD transaction. It emits the current cached
  name, keep-name instruction and name instruction through `0317`, the raw
  `-=> ` prompt through `031F`, and reads the entered name through the
  case-preserving `0345` editor. It canonicalizes the full input before the
  41-byte cap, uses the independent binary cached name only for an empty
  result, repeats without confirmation when both are empty, and asks the
  exact quoted A8D2 prompt otherwise. Only explicit Y accepts; N or blank
  restarts all three rows, while invalid input uses A8D2's bold/queue-clear
  retry. Acceptance overlays only name bytes 0..40 and MBF32 length bytes
  85..88 in the active FIELD, then performs one raw PUT. The later command-B
  owner/treasury write was changed to a raw two-lane overlay so it cannot
  destroy the accepted binary name. The presentation family and all 20 body/
  caller edges are candidate, the native reducer/overlay fixture passes, and
  all 13 focused upstream editor tests pass. Joined input/pager/endpoint/raw-
  FIELD state, corrupt record coercion, exact framebuffer and every physical/
  runtime failure prefix remain open; command-B and command-N are the
  separate candidates described next.
- The command-N port-rename wrapper at `YT:[286C,292A)` is now connected to
  that editor and to its mandatory current-sector continuation. It performs
  the independent entry player hydration, sector GET, raw-exponent no-port
  gate, compiled SINGLE physical-port calculation, distinct BRUN 24-bit
  random-record conversion, numeric owner test, and SINGLE relative-port
  Earth test in the shipped order. Live-mode CINT applies later only to the
  stored name length. Its refusal rows are exactly `No port here!`, `This isn't
  your port!`, and `Can't rename Earth!` through `02DB`; every refusal and
  successful editor PUT then enters one cached current-sector scan before the
  command loop's single fresh hydration and main prompt. Native reducer tests
  pin positive fractional BRUN truncation and the smallest-above-one value
  that rounds back to the Earth
  predicate. The presentation family and all 15 wrapper edges are candidate,
  and all 37 focused isolated/joined upstream tests pass. A complete joined
  prompt/FIELD/cache/editor/scanner/reentry fixture, the uncoerced raw player-
  record cell, corrupt runtime domains,
  exact framebuffer and every physical/failure prefix remain open.
- The command-B port-purchase wrapper at `YT:[2341,270C)` is now connected to
  its Earth/ordinary report children, shared editor, persistence chain and
  mandatory current-sector continuation. It independently hydrates the
  buyer, performs the raw-exponent no-port gate and BRUN physical-record
  selection, preserves the early ordinary owner and final report name, and
  computes the sale price through the shipped SINGLE/DOUBLE sequence. The
  exact already-owned, price, unaffordable, owner/offer, decline, sold,
  seller-transfer and two success rows are emitted in compiled order.
  Accepted persistence is non-atomic: fresh seller credit/count, personal
  radio, optional rounded-relative-port editor, fresh title/treasury overlay,
  then fresh buyer credit/count. Every completed path uses one cached sector
  scan followed by the command loop's single fresh hydration and prompt.
  Raw overlays preserve unrelated FIELD bytes and an accepted binary port
  name. Native arithmetic/overlay tests and all 66 focused buy/Earth upstream
  tests pass; the presentation family and all 45 wrapper edges are candidate.
  A complete injected prompt/FIELD/cache/report/editor/seller/radio/title/
  buyer/scanner/reentry transaction, stale/corrupt/alias variants, exact
  ANSI/plain/local framebuffer and every physical/failure prefix remain open.
- The main command-G whole-universe Genesis wrapper at `YT:[2728,286C)` now
  replaces the former host-style yes/no approximation. It saves the resident
  trader name, performs the body's independent player hydration before any
  output, emits the two exact prophecy rows and dynamic raw A8D2 prompt, then
  applies the `>300` cutoff only after valid input. Disabled input is forced
  to N and shares the exact decline row. The insufficient branch uses two
  shipped rows with SINGLE `STR$` formatting and SINGLE subtraction; success
  emits the direct blank and two separately bolded rows before writing
  `COMMAND$ CR LF EOF` and requesting replacement launch of RMT-INIT.
  Ordinary exits return directly to the command loop's single fresh hydration
  and prompt without a sector scan, while successful replacement has no
  in-process continuation. Binary prompt/shortfall reducers and canonical
  297-byte plain/321-byte ANSI accepted-body fixtures pass; all 23 focused
  upstream body/cycle tests pass, and the presentation family plus all 21
  wrapper edges are candidate. A complete injected prompt/FIELD/cache/input/
  file-registry/RUN/reentry transaction, native exact alternate-branch bodies,
  local framebuffer/endpoints and every physical/failure prefix remain open.
- The Planet Ground Forces body at `YT:[3389,3515)` now replaces its generic
  prompt and silent-rejection approximation. It independently reads the entry
  planet and player, emits the foreground-six direct blank and exact dynamic
  `031F` prompt, applies 036F's E filtering, converts `INT_D(VAL())` to SINGLE,
  and computes remaining forces through two separately rounded SINGLE
  operations. Rejection emits the shipped misspelled `Insuficient forces!`
  attention row. Acceptance freshly reads the planet, overlays desired forces
  and raw dirty-zero ownership, emits the positive success row before applying
  live ownership and selector-four sound, writes the planet, performs a second
  player hydration, then changes only the fresh FIELD ground-force lane and
  writes it without altering the hydrated cache. Native fixtures pin the
  canonical 111-byte plain positive body, dirty-zero/positive-owner stages and
  unrelated-byte preservation; all 42 focused upstream body/cycle tests pass,
  and the presentation family plus all 20 body edges are candidate. A complete
  injected planet-menu/FIELD/cache/input/effect/redraw transaction, alternate
  branches, ANSI/local endpoints and every physical/failure prefix remain open.
- The linked-planet caller at `YT:[3515,369D)` and permission helper at
  `YT-SUB:[8C7F,8F2F)` now replace the generic landing approximation. The
  caller owns its title, independent player hydration, numeric-zero-only
  creation boundary, physical planet expression, foreground-six landing row,
  pre-helper carried-force cache, exact denial sensor/A8D2/031F/036F input and
  the separate ground-assault handoff. Permission now uses an independent
  updater and planet GET, immediate/self/fresh-current-and-owner team checks,
  raw fractional-owner BRUN record selection, a separate live-owner-status
  GET, and the exact vacancy and denial paths. Vacancy preserves the first
  planet snapshot's ground count across its two-second wait, fresh planet GET
  and two unconditional RNG draws, overlays only owner and ground in that
  fresh raw record, writes it, then performs the five-second wait. Native
  reducers pin physical/fractional/wrapped records, owner/vacancy predicates,
  SINGLE attrition, exact dynamic rows, commitment flooring/bounds and
  unrelated-byte preservation; the canonical plain denial/decline stream is
  exact and all 27 focused upstream landing tests pass. The presentation
  family and all 63 caller/helper edges are candidate. A complete injected
  database/FIELD/cache/wait/sound/input/assault transaction, ANSI/local/mode/
  carrier variants, malformed-link continuation, dirty result-cell state and
  every physical/failure prefix remain open.
- The independently bounded ground-assault body at
  `YT-SUB:[8F35,9328)` now replaces its abbreviated combat/news/output
  implementation. It emits the entry blank before saving foreground and
  initializing its result, runs the updater and independent planet/name GET,
  independently hydrates and raw-debits the current player, and persists that
  debit before exact binary attack news and all combat display. Each round
  consumes side then amount, uses separate SINGLE multiply/subtract and INT/
  clamp operations, emits the exact foreground-three or foreground-four row,
  and calls selector two only after a defender-damage row. Foreground is
  restored before the common terminal blank. Victory orders defenses and
  optional capture display/news/sounds before a fresh raw owner/ground planet
  PUT; the both-zero fallback clears both lanes without capture. Defeat
  fresh-reads and writes only integral ground forces before its news and
  visible blink/bold failure row. Reducers pin all binary row builders, both
  round polarities and unrelated-byte preservation; native presentation pins
  the canonical 117-byte plain victory and 97-byte plain defeat, all 14
  focused upstream tests pass, and all 38 body edges plus the presentation
  family are candidate. A complete injected database/FIELD/cache/provider/
  news/sound transaction, ANSI/local/mode endpoints, native both-zero/
  nonprogress/failure cuts and every physical prefix remain open.
- The zero-link creation arm at `YT:[36A0,39B1)` now replaces the generic
  Genesis-device purchase shortcut. It owns the direct blank and two offer
  rows, independent second player hydration, DOUBLE credit row, strict
  25,000-credit gate and raw A8D2 purchase prompt. Allocation scans physical
  offset-plus-two through the live marker using BRUN record conversion and
  composes the existing exact Rename as its first durable mutation; protected,
  empty and reserved-name returns exit creation without resuming the scan.
  Success fresh-reads and raw-overlays the thirteen initializer lanes,
  retaining the shipped dirty zeros at missiles and mines, then independently
  links the fresh sector, samples DATE before TIMER and timestamps a fresh
  planet, and debits a fresh player record. News uses the trader cache saved
  before the second hydration and precedes the quoted created row, selector
  four and advice row. Native reducers pin dynamic row/news builders,
  timestamp/fresh-credit arithmetic, dirty-zero lanes and unrelated-byte
  preservation; the canonical plain success body is exactly 336 bytes, all
  23 focused upstream creation tests pass, and all 33 arm edges plus its
  presentation family are candidate. A complete injected FIELD/cache/
  concurrent-snapshot/file/provider transaction, poor/decline/full/protected/
  ANSI/local/mode variants, copied/live-marker seam, malformed aliases and
  every physical failure prefix remain open.
- The Planet Thrusters controller at `YT:[2FB4,3389)` and its one-hop
  helper at `YT-SUB:[80C2,8568)` now replace the host-style movement
  shortcut. The controller emits the exact cost notice before its mode-zero
  scanner, applies the shipped destination editor and equality-before-range
  gates, builds the avoid-aware route, preserves the mixed B05D/direct route
  grammar and pager resets, performs the fresh-turn load and raw confirmation,
  and concatenates every movement token before its ordered helper call. The
  helper derives the moving planet from the second source GET and preserves
  Xannoron refusal, occupied forced explosion, the strict stress predicate,
  the zero-fighter continuation defect, two-draw fighter shrink, Wanderer
  physical/requested destination split, hazard friendship restoration, and
  raw single-lane sector/planet/player writes. A stop reaches the outer
  gameplay re-entry without A41C; natural route exhaustion hydrates first.
  Reducers pin all dynamic rows, SINGLE arithmetic, exact four-NUL-plus-37-
  space name erasure and unrelated-byte preservation; native presentation
  pins the canonical 350-byte plain controller/final-hop body, all 42 focused
  upstream controller/helper tests pass, and all 109 bounded transfers plus
  the controller presentation family are candidate. A complete injected
  menu/scanner/controller/helper/re-entry FIELD/store/cache/RNG transaction,
  alternate native branches, ANSI/local/queue/pager/corrupt-state variants,
  TIMER reseed metadata, recursive hazards and every shared-runtime/physical
  failure prefix remain open.
- The anonymous sector-mine body at `YT-SUB:[63E1,6C8F)` now owns its exact
  unpaged entry, warning sound, entry news, per-batch sector decrement,
  explosion framing and shield-versus-unshielded damage split. Surviving
  shields consume the scanner draw even when no scanner exists;
  disintegration skips it; the unshielded path destroys a nonzero scanner
  without a draw and persists raw MBF32 `00 00 48 00`. Fighters, carried
  mines, commodities and empty holds retain their three-draw shrink
  cardinality under the authorized CSPRNG-source departure, while cloak and
  missiles keep their direct one-draw arithmetic. Each player commit starts
  with a second fresh GET and overlays only touched raw fields. Selector two
  precedes the unconditional emergency draw, destroyed ships can still warp,
  and the body no longer consumes its caller's common-death continuation.
  The ordinary `YT:0937` caller owns admission; the positive-original-mine
  post-kill `YT:2F68` caller bypasses that admission, and both route a
  destroyed return through common death. Reducers pin arithmetic, every row,
  the dirty scanner zero and unrelated-byte preservation; native
  presentation pins the exact 379-byte plain body, all 22 focused upstream
  tests pass, and all 90 body transfers plus both five-edge caller
  continuations and the presentation family are candidate. A complete
  injected session/FIELD/store/news/RNG transaction, TIMER/reseed metadata,
  ANSI/local/corrupt-state variants and every dependency/physical failure
  prefix remain open.
- The direct fighter-kill wrapper at `YT:[2E80,2F83)` now returns with zero
  additional effects when the target retains positive shields. Its kill arm
  dispatches selector three before a fresh victim GET, then snapshots the raw
  carried-mine value and binary fixed-field name through the compiled
  `CINT`/`LEFT$` semantics before shared death and salvage. Nonpositive saved
  mines now skip the complete deployment tail. Positive saved mines perform
  one fresh sector GET and raw offset-129 full-value add, then emit the exact
  `02DB` paged warning before appending the same bytes to news and invoking
  the anonymous mine body without ordinary admission. The saved binary name
  survives both children, so embedded NULs are retained. Native fixtures pin
  the row builder and exact 48-byte plain warning framing; all 20 focused
  upstream wrapper tests pass, all 15 transfers and its presentation family
  are candidate, and the caller—not the mine body—owns the destroyed-to-
  common-fatal disposition. A complete injected raw combat/death/salvage/
  mine composition, full 403/738-byte native transcripts, local/pager/
  carrier modes and every dependency/physical failure prefix remain open.
- The direct fighter destroyed-to-fatal cycle now preserves the mine body's
  nonzero destruction result without inserting output, RNG, I/O or state
  normalization. It inherits the mine background, pager and input state into
  the exact common-fatal `02DB` row, then performs the current-player reload,
  selector-three sound, self-death and one five-second wait in that order.
  `fatal_wait_complete` prevents the outer session owner from repeating the
  wait, and the result enters normal exit directly without confirmation.
  Native presentation pins the exact plain `CRLF + fatal LFCR + BEL` splice
  under inherited background one; all 11 focused upstream cycle tests pass,
  and the presentation family is candidate. A complete injected 1,597/1,900-
  byte raw transaction, its exact 133-event local tape, full Info/scoreboard/
  death/news/FIELD/cache state, lazy nonfatal/emergency branches and every
  carrier/dependency/physical failure prefix remain open.
- The shared emergency-warp child at `YT-SUB:[9FC4,A400)` now emits the exact
  wormhole and temperature gauge instead of the prior coarse dot loop. Its
  duration draws occur after the gauge open, the SINGLE counter begins at
  one, every tick uses `*` with the shipped green/yellow/blinking-red color
  thresholds and a 0.330000013-second shared wait, and heat 31 wins before
  the duration exit. Two post-gauge blanks precede the independent fresh
  player GET and destination/HQ/jitter draws. Normal and meltdown output,
  including the selector-one versus selector-six-plus-five-selector-five
  split, now uses the presentation transducer. The fresh raw player image
  receives only sector and turns; the process cache changes only after a
  successful PUT/flush, and the child no longer invents a queue clear. Native
  fixtures pin the exact 377/517-byte normal and 390/992-byte meltdown
  streams, arithmetic and unrelated-byte preservation; all 20 focused
  upstream tests pass. Root `YT-SUB:9FC4`, all 53 body transfers and the four
  black-hole caller/join edges are candidate. A full injected provider/FIELD/
  cache/wait/input/local/failure transaction, corrupt numeric domains and
  physical endpoints remain open.
- With the black-hole and mine children exact, the bounded gameplay-hazard
  family is now candidate. One native `sector_entry` loop owns the scanner
  and admission once, carries the black-hole or mine child's live state into
  its following scan, composes the mine's optional emergency warp, and uses
  the returned destruction flag alone for the common-fatal split. No return
  edge replays scanner output, admission, RNG or persistence. All 16 focused
  upstream composition tests pass across the canonical 489/649-byte black-
  hole, 234/324-byte mine and 583/799-byte mine-warp lanes. A complete joined
  raw FIELD/cache/RNG/news/wait/local transaction, recursive second hazards
  and every dependency/physical failure prefix remain open.
- The player-counterlaunch wrapper now uses the pinned MBF64 score factor,
  preserving the legacy score-100,000 result of one rather than a host-
  decimal result of two, and performs the required second fresh target GET
  before applying the stale first-snapshot missile debit to that fresh raw
  record. Its direct blank/bold row now crosses the presentation transducer,
  with plain and ANSI fixtures. The fixed-field `LEFT$` name conversion is
  placed after the actor/cloak swap; binary-safe terminal/news composers and
  the byte-length news writer retain embedded NULs, and the child destination
  retains its SINGLE value. A failed nested child now leaves the swapped
  actor/name/cloak context in place. Normal wrapper restoration uses raw
  MBF32 exponent truth for killed-by. After counterlaunch and Xannor
  wrappers, a destruction result now enters the common fatal notice/reload/
  sound/self-death bridge instead of skipping directly to the later wait and
  exit tail. The complete 1,875-byte fatal state/output/effect composition and
  wrapper failure prefixes remain open.
- Upstream commit `1248add9` reconciled the fatal-cycle prose with the
  artifact and test. The authoritative complete natural vector is 1,875 plain
  bytes with SHA-256
  `ae296b210cd269c4a50825da58eb0ec4d66e380510a68b46063467833afd79a6`
  and 2,057 ANSI bytes with SHA-256
  `1e3eb952a59b455d911f3b064386775a0ea60649a7ca32643c059b173007688c`.
  The evidence conflict is closed; the native complete joined state/output/
  effect fixture remains implementation work.
- The Xannor retaliation wrapper now preserves the shipped externally
  observable draw boundary under the authorized CSPRNG departure: three
  nested count draws precede the direct blank, the original actor is saved
  and the Xannor context installed, and only then is the mandatory fourth
  destination draw consumed. A provoker still overwrites that candidate with
  the untruncated SINGLE current sector. Its blank/bold row now uses the
  presentation transducer, and plain/ANSI fixtures pin the representative
  stream. Child failure retains the swapped context; normal return alone
  restores it before the final GET, raw killed-by truth and four-second wait.
  Complete injected WORLD/FIELD/cache/by-reference/latch and provider-failure
  fixtures remain open.
- The current/adjacent scanner now owns its separate initial-three line
  counter, exact per-row increments, cross-sector carry, strict `>15` pause,
  zero reset, and `YT-SUB:6341` 15-second wait. Adjacent mode ends with the
  original final player GET instead of rendering the current sector again.
- Emergency-warp heat ticks now call the shared successful `YT-SUB:94FD`
  model with the stored MBF32 `0.330000013...` duration. They no longer use an
  uninterruptible host millisecond delay.
- Existing salvage, victory, plasma-prelude/hop, Xannor-retaliation, and
  player-counterattack waits are assigned to their qualified static caller
  edges and now stop on native wait-adapter failure rather than silently
  continuing into later persistence or output.
- The deployed-combat and sector-mine fatal families now use one centralized
  successful common-fatal bridge: foreground-three `YT:02DB` framing,
  current-player reload, selector-three sound, current player as both killer
  and victim, the exact-once five-second wait, and the existing unprompted
  normal-exit disposition. The bounded presentation fixture pins plain/ANSI
  remote bytes, CRLF/LFCR split, forced `;5;1`, ANSI-versus-plain bold/blink
  residue, pager count, and physical local 7/0 reset. A deterministic full
  death/FIELD/news/exit fixture, partial failures, pager/input variants, and
  the other six shipped fatal caller joins remain explicit prerequisites;
  already-processed remote projectile death still uses the shared destroyed-
  session continuation without repeating the wait.
- The game-level registration, returning-player rebuild, lockout denial, and
  evaluation normal-exit reminder now own their qualified 10/2/5/10/10
  second waits and propagate wait failure before their later continuations.
  This does not enter the separately deferred OpenDoors serial startup or
  platform shutdown configuration phase.
- The qualified wait inventory now has no unclassified edge. The post-OPEN
  `YT-SUB:5893` one-second edge is explicitly deferred with the OpenDoors
  remote-startup/partial-cleanup phase. The apparent `YT-SUB:24A6` conflict
  was traced to a localized external oracle defect: its predecessor fixture
  retains `DS:A2FC = 00 00 40 82` (MBF32 3), no qualified predecessor writes
  that constant, and `2499` copies those bytes into the wait argument. Native
  startup uses the shipped three-second duration. The startup-opening models,
  global-state registry, generated evidence, focused fixtures, prose, and
  traversal row 307 were corrected upstream in analysis commit `690c9a40`.
- The Earth lottery now executes its qualified pre-roll, inner animation,
  post-digits, positive-award, and unconditional caller waits with the pinned
  MBF32 durations. Exact dummy-byte/backspace/local-cursor animation remains
  an explicit component prerequisite rather than being claimed by the timing
  work.
- The Earth front now recognizes only exact complete `S` and `I` tokens for
  its recovered adjacent-scanner and Info children, then executes each
  qualified nine-second wait before redraw. The complete byte-exact Earth
  report/table/menu/editor composition remains an explicit prerequisite.
- The vacant-planet governor now owns both qualified waits: two seconds after
  its governor row/sound and five seconds after its fresh-record attrition
  overlay and write. The intervening fresh GET, two unconditional draws, and
  shipped `govornment` rows are no longer skipped.
- The radio reader now preserves the raw SINGLE reader-mode contract rather
  than reducing it to a C Boolean. It selects the heading from numeric zero,
  evaluates the compiled signed-word expression through `CINT`, preserves
  the corrupt nonzero `.49` heading/eligibility/no-PUT split, and performs the
  inclusive zero-filled past-EOF probe. Every visible record resolves both
  fixed-field names through `CINT(name_length)` before the pair comparison;
  binary-safe changed-pair headers and all 74 body bytes use the exact direct
  presentation helpers. Its private strict-`>22` counter still owns raw
  `[Pause]`, the 99-second wait, direct blank, and zero reset. Only numeric
  mode zero mutates after that sequence, using SINGLE `counter-2` or the
  shipped dirty MBF zero. Native decision/header/mutation vectors and the
  canonical 45-byte empty and 139-byte personal streams are pinned. Partial
  random-file records and shared OPEN/GET/player-GET/PUT/CLOSE failure
  compositions remain explicit gaps.
- The hostile Attack admission prefix now follows the connected
  `YT:[0B20,0BB5)` authority. It emits `<Attack>` before testing the freshly
  hydrated ship-fighter cache, skips the amount editor only for the strict
  `< 1` no-fighters branch, otherwise uses the exact `031F` prompt and `036F`
  numeric editor, stores the successful `VAL` result through MBF32 `CSNG`,
  and applies the strict too-many test before the shared sub-one gate. The
  three early exits have canonical 56/80/44-byte ANSI action partitions and
  re-enter the existing fresh hostile-menu hydration. Complete raw scratch
  inheritance, joined database failure prefixes, and the admitted sector
  GET/combat/surrender/persistence continuations remain open.
- The next admitted defenders-remain spine now performs the opening sector
  GET (refreshing only the owner used by combat), combat A41C player refresh,
  selector-two sound, post-tested attrition, exact two common loss rows,
  player-before-sector fresh-GET persistence, post-persistence blank,
  mandatory late random draw, and direct fresh hostile-menu return. The
  native presentation fixture pins its canonical 139-byte ANSI action
  partition, while pure fixtures pin quantum flooring and the strict stored
  `.45` damage polarity. A deterministic injected-RNG full native
  WORLD/FIELD/store fixture and all adjacent surrender/spill/news/reward/
  death/clearance/cleared paths remain open.
- The shared `YT-SUB:[9C6B,9D3D)` fighter/shield spill child is now a
  reusable native reducer plus exact row formatter. It preserves the DOUBLE
  fighter argument, uses strict positive pretests, selects quantum 100 only
  when both operands exceed 100, sends equality at `.5` to fighter damage,
  does not clamp, and always emits the two direct unpaged result rows. The
  deployed-fighter Attack caller now owns the exact attention, direct blank,
  spill, and pre-persistence join. Direct-player Attack still needs to be
  connected to this shared child, and injected-RNG/failure/raw-world caller
  fixtures remain open.
- The hostile defenders-cleared tail now has the documented positive-loss
  rehydration/news ordering, ordinary-owner surrender dialogue and default
  acceptance, accepted transfer/sound/row/news sequence, refusal-to-same-
  iteration attrition, mandatory late draw, defeated row, and direct scanner
  continuation. The minimal scanner headings use exact `Sector:` and
  `Warps lead to:` SINGLE formatting. Full injected ordinary/accepted/
  refused WORLD/FIELD/store/news/RNG fixtures and the scanner's complete
  provenance-aware ANSI/local presentation conversion remain open, so the
  joined cleared cycle remains a candidate rather than verified.
- The hostile Bribe owner dispatch now represents the exact accepted and
  quiet-return lanes. Ordinary refusal emits its attention row before its
  one strict force draw; Mercenary planet refusal consumes no draw; and a
  zero-link Mercenary consumes both precheck draws before testing the sticky
  cell. Empty editor output returns before `VAL` or the third draw. A
  nonempty offer uses `VAL` plus MBF32 `CSNG`, always consumes the threshold
  draw after the above-credits comparison, preserves the emitted DOUBLE
  multiplication/addition association, and accepts equality. Success emits
  the exact row and selector-one sound, persists sector owner/fighters first,
  hydrates the player afresh, then persists fighter/credit changes before
  the `YT:081F` scanner/router return. Native fixtures pin the exact 132/188,
  35/49, 94/94, and 34/48-byte plain/ANSI action bodies and the strict
  predicate boundaries. Full injected RNG/raw WORLD/FIELD/store/failure
  joins, corrupt sticky/conversion paths, and forced/sub-one/fatal Bribe
  continuations remain open.
- Forced Bribe now preserves the distinct post-output commitment and fatal
  seams. The Mercenary life-demand and rejected-offer rows are exact; both
  then store `CSNG(cached ship fighters)` before testing original ship
  DOUBLE below one and shields SINGLE below one. Xannor and ordinary-player
  force deliberately skip that fatal test. A nonfatal rounded commitment
  below one returns directly to a fresh hostile menu without the `081F`
  scanner, while a Mercenary fatal result enters the existing common-fatal
  row/reload/sound/self-death/wait/exit owner. Reducer fixtures pin the
  Mercenary-versus-ordinary fatal distinction and near-one DOUBLE/SINGLE
  split; presentation pins the 41/55-byte life and 134/148-byte rejection
  bodies. Complete injected raw process/FIELD/RNG/two-A41C joins and a full
  native death/news/normal-exit transcript remain open, as does the forced
  commitment-at-least-one combat continuation.
- The shared sector-mine body now independently hydrates the player, repairs
  a negative durable mine field without changing the cached negative value,
  applies the exact no-mine and cached-sector Union gates, and uses the
  shipped direct blank, SINGLE-formatted prompt, `036F`, `VAL`, and MBF32
  amount store. Strict amount predicates accept equality and positive
  fractions. Acceptance sets self-mine suppression before I/O, debits and
  flushes the player before a fresh sector GET/add/PUT, leaves the process
  player cache stale, and emits the foreground-six bold/blink success row
  before selector-four sound. The hostile D/DW/DWT wrapper returns through
  `YT:081F`; native fixtures pin its 73/119 accepted, 44/44 cancel, 23/37
  no-mine, and 51/65 Union action bodies. Main-command D instead takes its
  distinct direct `YT:161D` foreground-one mode-zero current-sector scanner
  and fresh-main-prompt continuation without hazard routing. Full raw
  FIELD/preservation, suppression/failure, hostile scanner/hazard/menu, and
  joined main scanner/prompt fixtures remain open.
- The shared direct emergency-warp wrapper at `YT:[1593,161D)` is now
  distinct from its `YT-SUB:9FC4` warp child. Main first-byte W and hostile
  whole-string W/WT both enter an independent fresh-player/no-turn gate,
  with zero and negative turns taking the exact bold/blink no-turn notice
  and positive fractions admitted. The parent then emits its two exact
  foreground-seven warnings and direct blanks, runs the raw first-byte
  A8D2 confirmation loop, invokes the child only for Y, and sends no-turn,
  decline, and successful-child results through `YT:081F`. Native fixtures
  pin the 167-byte plain and 223-byte ANSI N-decline bodies and the no-turn
  body. A deterministic accepted-child RNG/FIELD/store transcript, invalid
  retries, physical/failure prefixes, and full main/hostile reentry joins
  remain open.
- The Team front at `YT:[55D6,5796)` now uses its ordinary visible
  team/captain resolver and promotion transaction, pager reset, two
  independent post-resolver player hydrations, exact teamless/member/
  captain rows, cached-time no-newline prompt, and case-preserving editor.
  The choice is stored through MBF32, captain then team CINTs precede the
  five strict validation terms, rejection emits the exact bold/blink row
  before a fresh resolver iteration, and exact raw `1` alone takes
  `YT:57A4 -> YT:081F`. Both main and hostile callers now preserve that
  scanner/router continuation; the hostile T edge itself remains
  zero-effect. The canonical teamless front is pinned at 105 bytes in plain
  and ANSI-cache modes. Exact raw `2` now enters Create's heading and
  ascending 1-through-50 scan, then the shared name helper performs the
  raw-length gate, normalization and fresh name PUT. Fresh player membership
  and roster PUT phases precede the shared uppercase password retry/reminder
  and fresh password PUT; exact cached actor/name news precedes the
  foreground-three success row. Join now performs its independent live-team
  listing, contains-E selection editor, selected/dead/full gates, ignored
  pre-display GET, cached password comparison and audit-before-invalid row.
  Success performs player PUT before a fresh overlay roster PUT, then news,
  foreground-three success and the join audit. The canonical plain Create
  and Join bodies are pinned byte-for-byte. Quit now uses the exact raw
  first-byte confirmation, writes player team zero while retaining the old
  process cache, performs explicit old-overlay and loader-buffer roster
  mutation, clears last-member metadata when needed, emits the unstyled
  success row, audits remaining members, and only then clears the process
  cache. Its canonical plain body is also pinned. Choice 5 now performs the
  exact nested teammate search: each match emits the fixed 41-byte player
  field and independently scans defenses and linked planets owned by that
  teammate, with raw list output and no later `None Found`. Its canonical
  resource body is pinned at 219 bytes and the no-teammate body at 58.
  Choice 6 now hydrates the initial player/sector snapshots, formats all
  status values as DOUBLE, applies `036F` then `VAL`/`CSNG`, reprompts only
  above the initial carried-fighter cache, commits a fresh sector overlay
  from the initial defense count, then subtracts from fighter offset 61 of
  an independently fresh player while retaining that snapshot's sector.
  Its accepted and zero-defense bodies are pinned at 141 and 49 bytes, and
  native reducers pin preservation of every unrelated fresh FIELD byte.
  Choice 7 now reloads the player and team overlay, scans exactly four
  cached roster slots while skipping only nonpositive/current-player
  entries, renders each candidate through the fixed-name/CINT helper, and
  uses the exact raw first-byte Y/N loop. Acceptance performs the second
  candidate GET, persists only player team offset 89 as zero, clears the
  selected cache, then performs a fresh team GET and overlays all four
  roster fields before the success notice. Native fixtures pin the exact
  39-byte reject/end and 80-byte ANSI accept bodies plus unrelated raw-byte
  preservation. Full member/captain/promotion/invalid front and joined
  membership/resource/Banish database-failure, ANSI/carrier/redraw fixtures,
  and the hostile 387/473-byte cycles remain open.
- The port docking controller at `YT:[660D,67CC)` now owns its exact
  title-first ordering, foreground-three fresh-turn gate, initial sector and
  physical-port calculation, numeric-zero-only no-port alert, direct-blank/
  no-newline `Docking, ` prelude, shared finalizer call, redundant selected
  port GET, and post-finalizer Earth/ordinary split. The ordinary lane
  rereads the updater sector, composes the existing updater/report children,
  schedules negative factors 1..3 before positive factors 1..3, remembers
  whether any nonzero-maximum quantity prompt was reached, and emits the
  exact dynamic refusal plus fresh DOUBLE credits/empty-holds row before
  main-command gameplay reentry. Native fixtures pin both no-port endpoint
  modes, the ordinary docking/turn concatenation, refusal/status framing,
  exact-zero link gate, and `1,3,2` schedule for `(-60,74,-66)`. The
  authoritative raw-store/cache join across finalizer, updater, report and
  trade children, nested planet-menu reentry, corrupt record coercions and
  all failure/physical endpoint prefixes remain open.
- The fixed-price commodity child at `YT:[7CCF,8443)` now begins with its
  independent fresh player GET, retains the updater's cached port quantities,
  computes the direction and maximum with the recovered SINGLE/DOUBLE
  boundaries, and implements the exact status rows, prompt-only retry loop,
  ordered validation notices, offer, raw confirmation, cancellation and
  success paths. Accepted trades preserve the recovered non-transactional
  order: optional fresh-port treasury PUT, fresh-player credit PUT, another
  fresh-player three-hold PUT, then fresh-port selected-stock PUT calculated
  from the cached quantity. A native fixture pins the canonical 249-byte
  buy-three-Ore stream; raw reducers pin preservation outside offsets 89,
  81, 69/73/77 and the selected 49/53/57 stock field. The complete sell,
  cancel, retry and zero-maximum native compositions, malformed/overflow
  conversion boundaries, physical database order and every partial-failure
  prefix remain open.
- The ship-computer front at `YT:[8443,8959)` now owns activation and sound,
  the exact `Time:` prompt/editor/default/truncate sequence, the 17-row help
  layout, and the recovered comparison order rather than a command
  dictionary. Exact commands precede the inherited
  `INSTR("+!LMP?123459", key)` selector, so aliases such as `!L`, `LM`,
  `MP`, `P?`, `?1`, `23`, `34`, `45`, and `59` reach their documented
  handoffs while exact `!` and `12` retain precedence. Command 14 now performs
  its second player GET, exact three filter rows, blank-to-All substring
  selection, team/ownership gates and commodity direction prompt. A native
  fixture pins the canonical 674-byte plain activation/help/reprompt and
  pager count 11; reducer tests pin all twelve inherited selector positions.
- The shared computer return at `YT:[8639,8672)` now uses a dedicated native
  hydration seam: fresh player GET, anti-cloak-gated current-player cloak-
  cache refresh, shared scratch clear, inherited-color blank, foreground-one
  prompt and the existing compatibility-uppercase/default/two-byte editor
  normalization execute in recovered order. Native fixtures pin isolated
  52-byte ANSI and 42-byte plain prompts plus mode-one zero-serial and mode-
  two blank-only partitions; all 69 upstream return-machine/prompt/front
  cases pass. Raw A41C FIELD/cache/scratch and failures, low-time/carrier/key
  branches, final framebuffer state and the other 52 A41C callers remain
  separate open work.
- Computer command 1 now follows the recovered non-returning join: exact
  one-byte selection emits `<Computer deactivated>` through `0317`, returns
  `enter_sector` to the main controller, and immediately runs one sector-
  entry pass rather than another computer prompt. Native fixtures pin the
  canonical quiet-sector 165-byte ANSI and 145-byte plain cycles through the
  first main-editor poll; all 11 upstream command-1 cases pass. Raw FIELD/
  cache/scratch/RNG state, hostile composition, black-hole/mine handoffs,
  queue/endpoint variants and scanner/hazard failures remain open.
  Planet command `C` now uses the same non-returning controller owner rather
  than the invented orbit-only refusal: the position-five substring route
  enters activation, selector-four sound, an independent player hydration,
  and the first Computer prompt without a planet redraw or adapter bytes.
  Native fixtures match the oracle's complete 178-byte ANSI/music and
  146-byte plain entry streams. Planet Leave and every ordinary landing
  result that targets `YT:081F` now propagate the existing sector-entry
  signal through `command_land`; Computer `L` shares the same outcome.
  The neighboring exact whole-string `D` lane now has a native joined proof:
  prompt/editor, the shared 585-byte nine-row inventory body, zero-byte
  return adapter, and fresh prompt match the canonical 742-byte stream in
  both ANSI-cache-matched and plain modes. Successful linked landing now
  calls that same inventory owner before the first prompt, restoring the
  `YT:39B1` join; its inventory-plus-prompt suffix is 662 bytes.
  Exact whole-string `S` now calls the mode-one adjacent sensor over the six
  inherited warp-cache cells; it no longer renders the current sector. The
  all-zero joined cycle matches 271 ANSI/music and 205 plain bytes through
  redraw. Exact whole-string `N` now enters the recovered Rename body with
  physical protection equations, `031F`/case-preserving `0345`, the shared
  canonicalizer, exact reserved alert, raw quoted `A8D2` confirmation,
  restart, 41-byte cap and fresh-record overlay. Native fixtures pin its
  48-byte protected, 92-byte reserved and 84-byte Nova/Y accepted bodies,
  plus the canonical 373-byte 256-byte-name and 339-byte 256-byte-answer
  streams. The internal editor destinations now admit those long successful
  inputs. Reducers pin normal and fractional protection equations, reserved-
  before-cap normalization and a fresh malformed 137-byte record whose only
  changed fields are the fixed name and MBF32 length. All 27 Rename transfers,
  its presentation family and root `YT:5464` are candidate, and all 39 focused
  Rename/action/full-cycle oracle tests pass. Joined restart/typeahead and raw
  current-record/frame state, mode/local/carrier/runtime cuts, physical
  failures and the explicit 32768-byte BRUN overflow boundary remain open.
  The parent inventory/menu root `YT:39BA` is now candidate as well: all 83
  bounded `YT:[39BA,3FA0)` edges are mapped, including the exact whole-string
  I gate, non-I fallthrough and zero-adapter Info return. The 585-byte
  inventory, 524-byte Help, 201-byte invalid and existing D/Q/S/C/default
  joins provide native presentation evidence, while 94 focused upstream
  menu/action/external/display/quit/full-cycle tests pass. A complete native
  Info/promotion child, nonzero Sensors, remaining actions, authoritative raw
  A41C/updater/FIELD/cache/RNG state and physical prefixes remain open.
  Raw scanner scratch and argument residues, final local effects,
  invalid/reactivation/filter branch transcripts, the Earth caller join,
  terminal paths and every failure prefix remain open.
- Computer command `S` now uses the six process-local cached warp cells at
  its scanner boundary instead of rereading the current sector. The native
  mode-one wrapper copies those cells before iteration, emits the recovered
  direct-blank/foreground-seven bold heading/selector-four sound/foreground-
  one loop/direct-blank/foreground-seven bold trailer sequence, reloads the
  current player, restores the saved logical foreground, and enters the
  shared fresh computer prompt. Native all-zero joined fixtures match the
  canonical 211-byte ANSI and 135-byte plain streams, and all 16 upstream
  command-S cycle cases pass. The nonzero per-sector scanner body is still
  only a structural candidate: exact rows, raw FIELD/string/scan-mode/cache/
  cloak/RNG effects, waits, endpoints and failure prefixes remain open.
- Computer command 7 at `YT:[8959,8C52)` now renders every one of the 30
  process-local avoid slots in the recovered ten-row, three-column layout,
  including the construct-21-before-output-1 ordering and two fixed 20-byte
  cells followed by one `02FC` cell. It uses both exact `036F` editors,
  performs the slot range check before `CINT`, stores the sector as an
  unrounded MBF32 value, mutates before output, and emits the recovered
  locked/available combinations before the shared fresh prompt. A native
  fixture pins the canonical 744-byte all-zero blank-exit cycle and final
  pager count one. Accepted fractional/status transcripts, raw MBF/FIELD
  joins, ANSI/mode/local effects and all terminal/failure prefixes remain
  open.
- Computer command 2/alias `23` at `YT:[8D5F,8EFD)` now uses the
  case-preserving editor, exact blank cancellation, VAL→INT→SINGLE bounds
  and `02DB` retry rather than the generic numeric prompt. It performs the
  selected-sector GET, unconditional friendship predicate with its fresh
  current/owner reads, and the entry-team-cache deployed-fighter visibility
  formula before dispatching Earth or rereading the sector for the ordinary
  updater/report pair. A native fixture pins the canonical 69-byte
  no-information body. Full ordinary/Earth joined streams, authoritative
  FIELD/raw friendship residue, physical persistence order, endpoint and
  failure-prefix compositions remain open.
- Computer command 10 and the shared command-3 navigation lane at
  `YT:[8EFD,932F)` now use the recovered two-editor path entry, retained
  `9999` marker behavior, range/equality notices, avoid-aware route display,
  fresh turn gate, exact raw confirmation, engagement notices, route-program
  queue prefix and final current-sector warp refresh. Native presentation
  fixtures pin the canonical 170-byte direct path body and 239-byte accepted
  one-hop autopilot body, including mixed line endings and the route-loop
  pager resets. The three upstream component/cycle suites pass all 61 cases.
  Exact raw route-builder FIFO/predecessor/avoid/FIELD behavior, processing
  the remaining physical warp slots after discovery, local `POS(0)` wrapping,
  raw marker/cache/string/queue residue, corrupt workspaces, all alternate
  branches/endpoints and every partial-failure prefix remain open.
- Computer command 4 at `YT:[932F,93A0)` now clears its selector state,
  emits the exact 69-byte whole-response old/update prompt through `031F`/
  `0357`, and treats only transformed exact `O` as old. The updated branch
  concatenates its heading with four raw progress dots emitted at the
  recovered generator phase boundaries, then enters the configured shared
  viewer. A native fixture pins the canonical 131-byte updated-`NUL` body;
  all 28 upstream scoreboard and joined file-cycle oracle cases pass. Full
  retained-bulletin playback/fresh-prompt state, raw cached-score/FIELD/
  queue joins, all mode/editor/carrier endpoints, exact file handles and
  every phase-specific physical failure prefix remain open.
- The command-5/alias-59 radio target front at `YT:[93A1,9548)` and its
  personal search at `YT:[9E86,9F7F)` now use the exact `0317` warm-up,
  direct target blank, `031F` plus case-preserving `0345` target editor,
  normalized whole-target branches, fresh team hydration, raw `A8D2`
  personal confirmation, tuning rows, limit notice and final body-handoff
  blank. The personal scan is ascending through the inclusive configured
  upper bound, tests the raw name-length exponent, performs case-sensitive
  binary-safe `INSTR` across all 41 fixed bytes, and preserves embedded NULs
  in the raw `LEFT$` confirmation prompt; explicit `N` alone continues the
  scan. Native fixtures pin that fixed-string seam and the canonical 94-byte
  blank-target body.
  The body at `YT:[9548,9E86)` now resets pager state at numbered-line entry,
  uses the recovered menu framing and whole-response editor, implements raw
  Abort and numeric/quoted Edit flows, retains recipient-major ALL news/radio
  ordering, emits the styled success row, and constructs the exact 86-byte
  YTRMSG record. Native fixtures pin the canonical 7-byte first-empty and
  99-byte plain personal-send bodies plus the personal/broadcast record
  bytes; all 60 upstream target/body/joined radio suites pass. Raw fixed-name/
  FIELD/team/draft/pager state, accepted target streams, custom-poll timing,
  wrap cleanup, backspace local `LOCATE`, complete Edit/List/Continue variants,
  physical writer prefixes and every endpoint/failure remain open. The
  canonical blank-target command-to-fresh-prompt cycle is now pinned as one
  181-byte native stream and promoted to candidate; joined personal/ALL/TEAM
  send cycles remain open.
- The generic gameplay radio-record writer at `YT-SUB:[2561,260E)` is now
  candidate with all six qualified body transfers mapped. Its native record
  reducer pins the exact recipient-`-2` counter selection, recipient/sender
  MBF32 fields, short-field padding, binary payload preservation and the
  reachable 75-byte input truncation to the 74-byte FIELD in a complete
  86-byte record. The exact legacy CLOSE-5/random-OPEN/FIELD/LOF/PUT/CLOSE
  physical sequence, shared file-number aliasing, all caller compositions
  and staged file/error-router failures remain open.
- The private direct-Attack casualty-radio helper at `YT-SUB:[250B,255B)`
  is now candidate with all three qualified body transfers mapped. The live
  C path no longer formats the numeric defender loss normally: it serializes
  the DOUBLE to MBF64 and deliberately passes its low four bytes through the
  MBF32 `STR$` formatter, reproducing ordinary integral loss as `0` and the
  `16777217` raw-alias boundary as `.5`. It preserves raw name bytes and uses
  the binary-safe generic writer handoff; all 29 focused direct-Attack oracle
  tests pass. Full raw DS pointer/frame state, caller chronology, physical
  writer errors and the joined casualty/result component remain open.
- The stale root label for `YT-SUB:[4B67,4BAA)` has been corrected: this is
  the shared fresh-player credit GET/add/`INT_S`/PUT helper, not the main
  command selector. Its sole qualified body return is now candidate through
  the native fresh hydration, SINGLE credit overlay and player write used by
  the exact Bank/Productivity compositions. Reducers pin the rounding and
  large-value boundaries, and 103 focused Bank/Productivity/creation/Earth/
  trade/Xannor oracle tests pass. An isolated injected raw helper fixture,
  consolidation of every remaining caller, raw frame residue and physical/
  shared-error failure prefixes remain open.
- Clearance row helper root `YT-SUB:4A82` is now candidate; its two
  qualified output/return transfers were already mapped by the clearance
  component. Exact normalization, percentage rendering, canonical ANSI/plain
  output and all four continuation roles are represented, and all 15 focused
  oracle tests pass. Raw string/frame state, full item/mode/local streams and
  physical/provider/shared-error failures remain open.
- The stale `YT-SUB:0D23` root identity has also been corrected: it is a
  nearest-port report helper inside `YT-SUB:[00B2,0E3F)`, not the separate
  point-to-point builder at `1016`. All 18 qualified helper edges were already
  owned by the nearest-port formatter/scanner, and all 34 focused command-14
  body/cycle oracle tests pass. A deterministic native raw port/clock/price/
  pager fixture and physical/input/output/error prefixes remain open.
- The PORTNAME entry root, helper roots `02B5`, `03A0`, and `0442`, all 39
  qualified transfers, and its 68-site presentation family are candidate.
  The native application now pins the exact CR-only startup, missing-data,
  blank, abort, progress and completion bytes; BASIC one-string parsing and
  redo; first-byte destructive-uppercase confirmation; update-or-create and
  redundant-close behavior; SINGLE loop and 24-bit record calculations;
  literal Earth plus exact generator draw placement; output-before-GET
  failure prefixes; name/length-only raw overlays; database-close ordering;
  and the authorized logical PLAY event. Native process fixtures preserve
  unrelated bytes and all 23 focused upstream PORTNAME tests pass. Physical
  BRUN framebuffer/editor/Ctrl-C/EOF/default-fatal state, raw FIELD/file and
  string descriptors, provider faults and partial console/file/close/END
  adapters remain open.
- YTCONFIG helper roots `1DF1`, `2073`, and `228F` are now candidate with
  all 89 qualified helper-body transfers mapped. Native fixtures pin the
  DATE$ calendar/epoch anomalies, bytewise title normalization including
  embedded NUL and apostrophe state, and exhaustive ASCII-only uppercase
  domain; all 17 upstream YTCONFIG tests pass. The four date and eight string
  caller joins, raw BRUN movable-string/numeric state, physical clock and
  failure prefixes remain open with the larger editor composition.
- Shared initializer roots `YT-INIT:004F`, `RMT-INIT:07F3`,
  `YT-INIT:23E4`, and `RMT-INIT:307E` are candidate with all 34 qualified
  helper-body transfers mapped. The bounded-random owner now has injected
  lower/mid/maximum and invalid-bound fixtures; both initializers share the
  exact tested 908-token port-name owner. All 33 upstream initializer tests
  pass. Caller joins, raw BRUN RND/DATA/string state, provider failures and
  composed world/persistence effects remain open.
- The shared YT-INIT operator-input root `009C`, its sole return and both
  confirmation/scoreboard caller joins are candidate. Native fixtures pin
  exact one-byte `Y`/`y` admission, suffix/empty/N rejection, decline before
  any data-file mutation, the blank scoreboard default and accepted-input EOF
  after truncation. Canonical BRUN B5 editing/echo/cursor state, the nonempty
  79-byte cap, raw descriptors, Break/runtime cuts and the physical console
  adapter remain open.
- Initializer date roots `YT-INIT:00B5` and `RMT-INIT:2339`, plus the remote
  identity title normalizer `RMT-INIT:2B42`, are candidate with all 128
  qualified helper-body transfers mapped to the already exact native date
  and byte-transform owners. Caller joins, raw DATE$/VAL/movable-string
  state, physical clock and runtime failure prefixes remain open.
- YT-INIT now has a deterministic complete-world fixture driven by the
  recovered 24-bit BASIC RNG sequence and a fixed clock. It matches all
  31,297 draws, final state `9F26F4`, and an independent digest of the full
  432,235-byte upstream database image. The comparison found and fixed one
  native serializer defect: YT ports 2..1000 must retain canonical zero at
  offset 101; only the RMT family writes the dirty-zero residue there. Roots
  `YT-INIT:13D3`/`15EA` and all 19 body transfers are now candidate. Raw BFS
  arrays, failure cuts, physical writes and the separate RMT graph remain
  open.
- RMT-INIT now has the corresponding dynamic-layout complete-world fixture:
  245 recovered-LCG draws, final state `3FED05`, and the independently pinned
  4,110-byte database image. Roots `RMT-INIT:2DBE`/`3031` and all 21 body
  transfers are candidate. Repair-bearing RMT vectors now pin both database
  mutation and the exact error/link presentation rows; raw BFS arrays,
  provider/allocation failures and physical write prefixes remain open.
- The successful initializer record-binding/default-normalization roots
  `YT-INIT:03C9` and `RMT-INIT:26D4` are candidate with all 14 qualified
  helper-body transfers mapped into the same complete-image and standalone
  RMT fixtures. Raw FIELD/file-number/CVS state and physical/failure prefixes
  remain open.
- RMT-INIT output helper roots `264D`, `268F`, and `269E` now own the complete
  successful reset/progress presenter as well as completion. The injected
  address-owned calls occur at their byte-pinned mutation boundaries; a
  stateful width-80 PRINT reducer retains independent local/serial columns,
  pre-value wrap, comma zones, terminal control bytes, local/serial order and
  the explicit `1113` serial-only wrap. Independent recovered-LCG fixtures
  pin a 59-call/1,266-byte local tape, two dynamic remote tapes with exact
  long-link/repair/wrap sites, and a 100-sector/30-port tape with exactly two
  sector dots and two port dots. Failure fixtures cut at `0863`, `0907`,
  `0AB6`, and `0B7B`, pinning preservation/truncation, draw count and durable
  config-write boundaries. `RMT-INIT:2305..232D` is no longer an unknown
  delay: static MBF32 constants are exactly 1 and 2222, so the empty FOR loop
  admits and increments 2,222 values and exits with 2223. The native logical
  loop runs after the remote return row and before cleanup; no unpinned host
  sleep was invented. Sixty-two additional reset/progress/delay transfers
  and `rmt-init-whole-program-output` are candidate. Raw BRUN descriptor,
  float-stack, framebuffer and physical-failure state, a deterministic joined
  physical middle, and Win32 execution remain open.
- All 389 qualified RMT-INIT transfers now have candidate owners. While
  closing the remaining controller rows, the two internal date-helper caller
  joins exposed a real observation collapse: native code had reused one date
  for startup, last-maintenance and port timestamps. The initializer now
  samples `08D6`, `09CD`, and `17E6` separately at their actual phase
  boundaries. A sequenced Jan-1/Jan-2/Jan-10 fixture pins epoch 26,
  last-maintenance serial 1, port production timestamp 0, and exactly 17 clock
  calls including the seven TIME$/DATE$ banner pairs. The other newly mapped
  rows cover the serial post-rewrite dead arms, post-truncation binder,
  player loop, graph generation/verification/shortcuts, sector/port/planet/
  Xannor record loops, helper caller joins and terminal paths. They remain
  candidate pending their declared raw-state, alternate and failure-prefix
  prerequisites.
- The bounded RMT-INIT handoff/standalone entry front now has exact native
  ownership for all nine qualified transfers through `014D` and both
  standalone/remote joins at `0165..0167`. Pure and
  executable fixtures pin missing/empty random-open classification, nonempty
  CR/LF/DOS-EOF first-line extraction, two leading blanks, banner, following
  blank, semicolon prompt, normal immediate-response paint, strict whole-string
  `Y` or `y` admission, blank/N/suffix rejection, accepted extra blank,
  ordinary decline END with the created empty handoff retained, and accepted
  deletion before old-data validation, plus the standalone DORINFO/UART bypass
  with `The Sysop` credit. A valid COM1 handoff now also traverses the bounded
  parser, physical platform adapter and public OpenDoors session through both
  the missing-old terminal and nonempty-old ordinary completion tail.
  Canonical BRUN B5 key/editor/cursor/framebuffer, Break/EOF,
  embedded-NUL/raw descriptor/heap/file registry and every physical/failure
  prefix remain open; the accepted path now joins the reset/progress owner.
- RMT-INIT's DORINFO-to-UART block now has an observation-driven native
  reducer with 37 newly mapped qualified transfers. It pins optional-colon/
  final-byte COM parsing; strict COM1..4 admission; COM1/3 versus COM2/4 BRUN
  aliases; COM3/4 BDA byte writes; the exact `03F8-offset` register layout;
  ordered LCR/IER/DLM/DLL sampling and restoration; SINGLE
  `115200/divisor`; zero-divisor ERR 11 before restoration; destructive
  7-bit framing selection (even original `7,N,O` becomes E/7/1); literal
  1200-baud OPEN specification; OPEN-error truncation; the low-byte-256
  no-carry reconstruction corner; and all eight post-OPEN UART events.
  Fixtures cover COM0/5 local, COM4/1200 success, COM3/divisor-7936 and exact
  OPEN/zero-divisor failure tapes. The supported-platform adapter now obtains
  an actual terminal/handle speed, admits only exact legacy divisors, prepares
  fixed 1200 with the destructive N8/E7 framing, lets OpenDoors attach, then
  restores the observed speed and reapplies it after shutdown. FreeBSD and
  Linux use the same POSIX path; the physical fixtures now execute on the
  current FreeBSD host at 38400/E7. Enabling them outside Linux exposed tty
  `ONLCR` translation, so the Yankee Trader platform layer now disables
  output post-processing to retain byte-transparent COM writes. Win32 opens and owns the COM
  handle, configures it, and passes it through the published
  `od_open_handle`; physical Win32 execution remains open. No OpenDoors
  source, API, or ABI was changed. Raw DOS BDA/register results, nonstandard
  divisor policy, descriptor-corruption lanes, shared error disposition and
  staged failure prefixes remain open.
- The decoded RMT remote-status range predicate and jumps at `05B2`, `05B4`
  and `05E8` now join the UART reducer to the shared native composer. It pins
  the exact direct `Local Console Mode` row and five-value
  `Opening COM port 2 at 2400 baud` numeric row, both with CR. The printed baud
  is explicitly the sampled UART result, not the DORINFO framing-line number.
  The provisional DORINFO-baud seam is gone: the executable uses the physical
  platform observation, and the joined POSIX process fixture proves the
  local-only status row does not enter the remote byte stream. Win32 local
  framebuffer and physical failure-prefix fixtures remain open.
- RMT-INIT's eight DORINFO reads no longer pass through host `fgets` line
  semantics. `yt_rmt_dorinfo_parse` now applies the bounded BRUN grammar:
  NUL discard, bare-LF retention, CR plus optional-LF termination, unread DOS
  EOF, unterminated final-field acceptance, exact ERR-62 failed-field/cursor
  results, and no ninth-line consumption. Pure fixtures cover every one of
  the eight truncation cuts. A subprocess fixture joins an embedded-NUL
  DORINFO through COM0 local status, normalized alias lookup and the exact
  missing-old terminal row. A second subprocess fixture covers valid COM1,
  physical 38400 observation, E7 setup, OpenDoors attachment and the exact
  remote missing-old row; a third physical process path now reaches the exact
  nonempty-old completion tail. Raw file/device/descriptors, large
  dynamic-string fields and per-read failures remain open; the valid remote
  path now joins the reset/progress owner and physical completion suffix.
- The following RMT remote-identity normalizer and alias scan at `071F..07E2`
  now has all six qualified transfers mapped. An injected owner pins combined
  first/last trim-collapse-title normalization, complete ordered scanning,
  case-sensitive exact normalized-real-name comparison, last duplicate match
  wins, and empty credit without a real-name fallback. Joined DORINFO/YTNAME
  process fixtures, raw descriptors/per-row INPUT/EOF/CP437 state, physical
  file prefixes and the preceding UART composition remain open.
- RMT-INIT old-data validation root `27BF` is candidate with all 32 qualified
  body transfers and its three entry binder/validator/rebuild joins mapped.
  Native fixtures pin the LOF-zero decision before any GET,
  configuration decode or RNG draw; two direct local blanks; the address-owned
  remote-first/local-always BEL row at `281C -> 264D`; random-open creation;
  deletion; and normal END. They also pin the nonzero decode/reset path,
  old-HQ write versus preservation, inclusive no-player and multi-player cloak
  passes, positive raw sign-bit mutation, negative preservation, every
  old-setting normalization boundary, dynamic layout, fixed date, one HQ draw
  and the complete deterministic RMT database. Physical POSIX process
  fixtures now pin the missing-old terminal, reset prefix and ordinary
  completion/cleanup tail. Ordered reset/progress output, dynamic repair and
  progress tapes, four destructive presentation cuts and the exact bounded
  delay are joined natively. Raw FIELD/file/cache and CLOSE-all registry
  state, a deterministic physical middle, Win32 framebuffer, and every
  remaining partial I/O/runtime prefix remain open.
- YTMAINT bounded-random roots `78F3` and `7939` are candidate with all
  eight helper-body transfers mapped. Native injected vectors pin one-draw
  and repeated-narrowing cardinality and boundary results under the authorized
  replacement-CSPRNG source; all 71 focused maintenance-AI/BRUN-RNG oracle
  tests pass. Caller joins, physical TIMER reseeding, raw cells and provider
  failure prefixes remain open.
- YTMAINT route-neighbor helper root `3953`, all six physical-slot caller
  joins and all six body/return transfers are candidate. The extracted native
  owner pins zero and already-visited skips, predecessor writes, FIFO append
  order and safe range/capacity rejection; the valid BFS now uses the shipped
  predecessor workspace itself as its visited marker. Parent path root `34EB`,
  raw 0..3000 SINGLE/CINT workspace state, corrupt indices, allocation/runtime
  failures, database reads and not-found output remain open.
- YTMAINT message/news compactor root `672D`, its caller join and all seven
  helper/body/return transfers are candidate. The native binary fixture pins
  decoded numeric-zero omission, ordered nonzero retention, raw bytes 0..83
  copying with emitted zero bytes 84..85, incomplete-tail omission, TEMP
  replacement, and unconditional whole-current-news rotation over yesterday.
  The clean-install composition remains intact; all nine native CTests and all
  39 focused YTRMSG/maintenance-daily oracle tests pass. The exact dynamic
  date-heading and `Compressing Message Base's` output, raw
  RANDOM/FIELD/LOF/GET/file-number state, missing-file variants, and every
  physical OPEN/CLOSE/GET/PUT/KILL/NAME/runtime prefix and failure remain
  open.
- YTMAINT immediate combat-death root `6924`, its Xannor caller join and all
  20 body/return transfers are candidate. A real native YTDATA fixture pins
  cache clearing, killer `-1`, sector/ground/team clearing, owned-port and
  treasury release, deployed-defense transfer to owner `-2` without changing
  its fighter count, duplicate roster removal, unrelated port/defense
  preservation, and deliberate retention of the victim's active marker,
  name, ship resources, credits, cloak, score, and planet. All nine native
  CTests and all 77 focused maintenance-daily/AI oracle tests pass. Raw
  FIELD/cache/config/loop/MBF/frame state, corrupt team/record/killer inputs,
  all staged physical failure prefixes, and the complete Xannor
  combat/news/radio/output caller composition remain open.
- YTMAINT final expired-player root `6C0D`, its caller join and all 32
  body/helper/return transfers are candidate. A real native
  YTDATA/YTRMSG/YTNAME fixture pins cache and lottery clearing, duplicate
  roster removal, planet release, stale-player-byte preservation, deployed
  defense clearing, recipient-or-sender radio invalidation, unrelated record
  preservation, dangling killer repair to `-98`, deliberate port
  nonmutation, and the separate `7039` alias successor. The fixture exposed
  and corrected a native adapter bug: opening YTRMSG as `a+b` forced each
  intended in-place counter write to EOF and repeated matching records
  forever; `r+b` with create-on-missing now matches RANDOM update behavior.
  All nine native CTests and all 56 focused daily/YTRMSG/YTNAME oracle tests
  pass. Raw FIELD/cache/config/loop/MBF/frame state, corrupt IDs and bounds,
  all staged physical failure prefixes, and the caller's deletion-news/alias
  physical composition remain open.
- YTMAINT expired-player alias-compactor root `7039`, its caller join and all
  six body/return transfers are candidate. The public native owner now pins
  the stable player name retained across the preceding length clear, removes
  every exact case-sensitive fields-three/four alias match, preserves
  real-name-only and case-mismatched rows in their original order, writes one
  DOS EOF, and consumes `tempwork` through the successful delete/rename.
  All nine native CTests and all 45 focused YTNAME/maintenance-daily oracle
  tests pass. Raw INPUT#/EOF/parser/movable-string/file-number state,
  malformed/incomplete/quoted-row execution, and every physical
  CLOSE/OPEN/PRINT#/KILL/NAME accepted-prefix, failure, and error-routing
  endpoint remain open.
- YTMAINT date root `79E3` is candidate with all 47 helper-body transfers
  mapped to the shared native date owner. Native fixtures now traverse all
  twelve month lanes, leap/current-year paths, two-digit rollover,
  fractional epochs and the compiled one-prior-year early exit, including
  epoch 26 plus 01/01/30 = 366 rather than 1462; all 28
  focused maintenance-daily oracle tests pass. The apparent source-level
  multi-year `FOR` loop is not a multi-year runtime loop: the shipped
  `7CD1..7CDE` compare exits after its first iteration whenever the current
  year differs from the epoch. Upstream analysis commit `482493c4` and
  traversal row 309 now pin the same result. Raw DATE$/substring/VAL/MBF state, physical
  clock/failure prefixes and all seven caller joins remain open.
- YTMAINT maintenance route root `34EB`, both Xannor/Mercenary caller joins,
  and all 30 body/helper/return transfers are candidate. A real initialized
  2,004-sector native database fixture pins the same-sector zero return before
  cache allocation, first-use caching of every sector's six physical warps,
  reuse after the on-disk graph is changed, physical-slot/FIFO equal-hop
  selection, zero-neighbor rejection, successful first hop, the live
  group-20 zero-target not-found lane, and the exact failed-route YTNEWS
  line/spacing with CRLF and one DOS EOF.
  All nine native CTests and all 61 focused path/maintenance-AI oracle tests
  pass. Raw 3,001-cell MBF32 predecessor plus aliased FIFO/route workspaces,
  same-sector unrelated residue, corrupt/fractional indices, dynamic-array/
  FIELD/cache/config/frame state, every staged physical failure prefix, and
  the complete faction caller compositions remain open.
- YTMAINT scoreboard root `7DB8`, its Super Lottery caller join and all 73
  body/return transfers are candidate. The maintenance owner now completes
  the shipped close-reopen readback after the existing score/cache/file
  generator. Deterministic native fixtures pin empty/Xannor and all-zero
  failure prefixes, exact `NUL` to retained `yttemp`, full CRLF/DOS-EOF
  grammar, alive/dead players, deployed-defense scoring, durable score-cache
  writes, player/team reorder and names, and exact 19/23-row logical screen
  replay. All nine native CTests and all 45 focused scoreboard/lottery/output
  oracle tests pass. Raw static arrays/FIELD/file-registry/process residue,
  corrupt numeric/team/owner inputs, staged physical failures, and the whole
  joined Super Lottery suffix/CLOSE-all/END composition remain open.
- YTMAINT Super Lottery `535A..57D1` now has an exact native controller and
  physical transaction fixture. All 15 qualified gate/selector/writer/date
  transfers at `538B..5780` plus the `57C6` body return are candidate; the
  scoreboard call was already mapped. Success and all four no-winner exits pin the exact 12/2/3/4/1 draw
  prefixes, one-based player/planet/sector selection, raw-nonzero/CINT player
  name handling, binary-safe screen/news output, personal radio payload and
  headers, and the constructor's eight SINGLE draws. The raw PLANET image
  proves the second fresh GET, 41-byte LSET, full pre-truncation length,
  dirty `00 00 3B 00` stock/missile zeroes, canonical mine zero, preserved
  plasma/fighters/unrelated bytes, and PLANET PUT before fresh SECTOR
  GET/link PUT. The common final-marker suffix now freshly reads
  configuration record 1 before overlaying only offset 81; this corrected a
  stale-cache write that could have reverted intervening configuration
  changes. The authorized CSPRNG departure omits the three physical
  `RANDOMIZE TIMER` effects but preserves the reached draw sequence. The
  database is now closed before the exact retained-date/completion wrapper
  rows rather than printing a detached completion line from `main()`. The
  presentation family is candidate; raw runtime/FIELD/file state, injected
  failure prefixes and exact physical CLOSE-all/END state remain open.
- YTMAINT shared writer roots `57D1`, `7155`, and `7D0B` are candidate
  with all seven qualified helper-body transfers mapped. A native fixture
  pins repeated news lines with CRLF and one DOS EOF plus ordered personal
  and broadcast 86-byte radio records, including counters 1/20, MBF32
  headers, 72-byte clipping/padding and the two-byte zero tail; all 21
  focused YTNEWS/YTRMSG oracle tests pass. The three-site YTMAINT news-writer
  presentation family is now candidate on the same binary-safe physical
  fixture. Exact CLOSE/RANDOM/FIELD/file-5
  registry state, raw PRINT#/LOF temporaries, all caller joins and every
  staged OPEN/PRINT/PUT/CLOSE failure prefix remain open.
- Computer command 6 now has a native joined-cycle presentation fixture for
  the canonical mode-one empty radio log: active computer prompt and echo,
  direct reader blank/`Log of messages sent/recieved.`/`None Found.` rows,
  successful return hydration boundary and fresh computer prompt total the
  recovered 134 bytes. The underlying reader's visibility, nonmutation,
  private pagination and binary header/counter foundations remain separately
  pinned. Physical radio/name GET/FIELD/CLOSE state, a message-bearing joined
  cycle, typeahead, endpoint/wait failures, shared-handler prefixes and final
  local framebuffer remain open.
- Computer command 8 at `YT:[8CA4,8D2F)` now owns its one leading blank,
  exact 63-byte `031F` prompt, whole-response `0357` T/Y retry loop, lowercase
  current `ytnews.dat` versus uppercase `YTYNEWS.DAT` selection, shared viewer
  call and controller return. A native fixture pins the canonical 90-byte
  successfully empty current-news body, and all 31 upstream newspaper/shared
  file-cycle cases pass. Retained 523/398-byte newspaper bodies and joined
  fresh prompts, raw source/FIELD/queue/color state, pagination/Ctrl-X,
  physical recovery append and every endpoint/failure prefix remain open.
- Computer command 9 at `YT:[9F7F,A1F8)` now uses the fresh strict-positive
  `A89B` turn gate, exact `031F`/`036F` prompt and numeric selection, styled
  excessive-sector retry, unconditional fighter-owner friendship join, and
  the limited/unavailable/full decision structure. Its full path carries the
  updater's live P/Q/A cache through the exact typed 49-byte inventory rows.
  Native fixtures pin the ANSI no-turn/range/limited variants and canonical
  625-byte full body; all 82 upstream body/cycle/input/selection cases pass.
  Authoritative raw world/FIELD/cache composition, the invalid-link stale-
  current-planet full path, a native 710-byte joined cycle, endpoint/editor/
  carrier variants, physical storage ordering and every failure prefix remain
  open.
- Computer commands 11 and 13 now use their distinct recovered finder bodies
  and return lanes. The fighter finder mixes one unterminated `031F` search
  row with direct blanks, fixed-width fields, `02FC` rows, ascending sector
  order and the post-row exact-Q gate. The planet finder uses its foreground-
  2 direct prelude, foreground-3 scan, all 41 raw name bytes and bold direct
  rows. Native fixtures pin the canonical plain command-to-fresh-prompt
  cycles at 170 and 227 bytes, and all 27 upstream body/cycle cases pass.
  ANSI/NONE/pager-Q cycles, raw FIELD/scanner scratch and corrupt-link record
  coercion, command-13 ES/direction residue, endpoints and every physical
  failure prefix remain open.
- The shared owned-port treasury body at `YT-SUB:[8901,8C7F)` now uses only
  direct presentation, tests the stored treasury exponent, binds raw port
  names through the recovered `CINT`/`LEFT$`/fixed-width sequence, and writes
  the shipped dirty-zero clear `00 00 20 00` before its fresh-player
  settlement tail. Native fixtures pin the canonical 348-byte command-12
  report, 339-byte hidden-`!` collection, 362-byte main-`$` scanner/fresh-
  prompt cycle and ANSI no-owned result; all 64 upstream body/computer/main-
  cycle cases pass. Main `$` now explicitly takes its mandatory sector-entry
  join. Exact injected raw database/cache, MBF56 accumulation/rounding and
  scanner state, ANSI joined cycles, endpoint/local results and every partial
  physical failure prefix remain open.
- Computer command 15 at `YT:[A381,A41C)` now uses the styled `02DB` zero-
  spy notice and the recovered one-blank, ascending bold-`02FC` row loop with
  SINGLE counter and signed-INTEGER target formatting. Native fixtures pin
  the canonical 199-byte ANSI and 155-byte plain two-spy cycles plus the
  152-byte ANSI zero-spy cycle through the fresh prompt; all 28 upstream
  body/cycle cases pass. Count-one/three, ANSI-disabled retained style,
  queue/key/carrier/mode/local endpoint variants, the post-hire join,
  authoritative cache/FIELD/scratch state and every failure prefix remain
  open.
- Computer commands 16 and 17 now have exact ordinary joined presentation
  fixtures and corrected pricing observation order. Every reached port calls
  the date helper immediately before its own `TIMER` projection; command 17
  preserves a fractional cached source-sector SINGLE for the displayed
  four-byte field while using the positive truncated record for its sector
  read. The adjacent two-row cycle is pinned at 312 ANSI/226 plain bytes;
  the global three-row encounter-order/two-column CP437 cycle is pinned at
  340/228 bytes. All 44 upstream body/cycle cases pass. Authoritative entry/
  return A41C FIELD/cache state, physical record expressions, full date/
  TIMER tapes, zero/no-result/corrupt paths, the 44-result pager, endpoints
  and every physical/error prefix remain open.
- Main command `F` at `YT:[1CFE,1EC3)` now emits its title before fresh
  player hydration, applies the exact Union and foreign-force gates, DOUBLE
  available row, `031F`/`036F` desired-count editor and VAL→INT→SINGLE
  conversion, then writes fresh sector before fresh player and emits the
  success row/sound. Native fixtures pin the canonical 246-byte ANSI and
  214-byte plain lowercase/trailing-command cycles through the fresh prompt;
  all 29 upstream body/cycle cases pass. Exact refusal/cancel/zero/
  insufficient streams, injected raw cache/FIELD snapshots, partial writes,
  endpoints and every failure prefix remain open.
- The shared `A89B`/`A6E3` dependency now uses strict-positive fresh-turn
  admission and stages only the documented fresh player FIELD slices: turn
  offset 49 and, on cadence, cloak offset 125 including the shipped dirty
  clamp zero. Positive cloak output uses foreground-seven `031F`, restores
  only foreground, and emits two direct blanks; the expired path uses the
  exact attention/sound helper and its extra blank. Player persistence
  precedes the strict-`<51` foreground-three bold/blink turn row, which now
  uses `02FC` and therefore ends `LF CR` before the unconditional trigger
  draw and Xannor/destruction/post-GET routing. Native presentation pins the
  positive-cloak/turn sequence. A full injected raw cache/store/RNG/spy/
  Xannor fixture, expired-cloak ANSI/sound, conversion faults and all
  pager/carrier/I/O prefixes remain open.

- The first YT-INIT whole-program presentation slice is now native. The
  executable no longer asks for the scoreboard path before it knows the
  values it displays: after exact confirmation it emits the opening rows,
  truncates `YTDATA.DAT`, samples the one startup clock and one headquarters
  draw into `struct yt_initializer_preparation`, presents those values, and
  only then reads the scoreboard pathname. `yt_initialize_yt_prepared()`
  carries that state into persistence without a duplicate clock or RNG draw;
  the pre-existing `yt_initialize_yt()` entry retains its generic behavior.
  The YT-only presenter owns line, inline, comma-zone, locate and logical-PLAY
  events without changing OpenDoors or its ABI. The successful fixed-clock /
  recovered-LCG fixture pins 15 clock calls, 31,297 draws/final `9F26F4`, the
  unchanged 432,235-byte database/FNV, an 8,379-event current tape/FNV
  `E72AA6DB06FEF26C`, 35
  long-warp triples, 2,003 verification triples and 1,000 port number/name
  pairs. Raw BRUN flow resolves `0B09`: `LSET` leaves `DI` at FIELD descriptor
  `A546`, successful `PUT` preserves it, and `0B07` supplies it to
  `PRINT_STR_NL`; the row is therefore the four offset-53 bytes for MBF32 51,
  `00 00 4C 86`, plus its newline. A failure at `0ABB` leaves the already
  truncated database empty. A failure injected at `0B1B` stops after that
  blank and the raw row, consumes no post-headquarters draw and leaves exactly
  the 137-byte configuration record durable. Normal termination emits the
  shipped status rows and records
  `L64cgaL1p1p1p1` as the authorized no-audio logical PLAY event before the
  existing process-replacement handoff. The qualified `YT-INIT:23DA` RUN edge
  is now candidate through `main_yt_init.c`, `yt_platform_spawn()`, and the
  composed clean-install fixture: output is flushed, sibling `ytmaint`
  replaces the initializer, and maintenance output/effects must complete
  before the launched process returns. POSIX already uses `execv`. The Win32
  replacement branch previously launched YTMAINT and immediately exited zero;
  it now waits, closes the process handles, and exits with YTMAINT's status.
  The same audit fixed a pre-existing preprocessor nesting error that had left
  Win32 without the private serial-state type; `yt_platform.c` now passes a
  strict MinGW C17 `-Wall -Wextra -Werror` object compile as well as the native
  build.
  Direct Win32 execution plus raw BRUN PLAY-stop/path/preflight/cleanup/loader
  state and partial failures remain open.
- Upstream commit `1248add9` corrected the generated YT-INIT presentation
  model to include the two ordinary rows at `YT-INIT:08BF..090D`:
  descriptor `DS:A986` resolves to `  # of turns per day:` followed by live
  `500`, and descriptor `DS:A9A0` resolves to
  `  # of times per day a user may play the lottery:` followed by live `5`.
  The accepted tape now has 31 events, ends at QB row 22/column 5, and has
  framebuffer SHA-256
  `a5ff53fac6772feda7309e3ec7c59f1453a2bc6e28c5333d699c6b007194acd7`;
  the combined prefix plus accepted tape has 40 events, ends at row 24/column
  5, and hashes to
  `40123d9a64d9bffbb3f7eda52b10ce1d136cedbad0d4c328fe72bbe0a1924ac1`.
  The C presenter already followed the executable and pins both rows. Every
  reached direct-screen PRINT value site is owned; the remaining reported
  sites are the seven `PRINT #` news rows and Dummy alias row already owned by
  the exact file builders.
- The ordinary movement parent `YT:[1ADF,1CFD)` and its active-main M-cycle
  composition are now candidate. `command_move()` uses the six cached warp
  cells without an extra sector GET, builds the exact zero-skipping physical-
  order `Warps lead to` row, repeats only for whole transformed response `M`,
  preserves fractional exact-warp admission, and emits the exact same-sector,
  nonadjacent and dangerous-destination confirmation rows through their shared
  presentation/input owners. Accepted movement runs the action finalizer,
  clears self-mine suppression, freshly reloads the player, overlays only raw
  sector bytes 57..60, persists before updating the process cache, and reports
  whether a hop occurred. Main, Planet and Computer callers now enter sector
  reentry only after an actual hop; the command shell uses its loop-top A41C as
  the single following-prompt hydration. Native fixtures pin arithmetic,
  unrelated-byte preservation and the canonical 89-byte safe-parent stream;
  all 24 isolated-parent and 15 active-cycle upstream tests and all nine native
  suites pass. All 44 qualified parent transfers and both presentation
  families are candidate. A complete native 205/235-byte joined cycle,
  independent raw shell/gate/danger/finalizer/router FIELD/cache/RNG/queue
  state, typed terminal/hazard/hostile joins, and the Earth predecessor remain
  open.
- The direct player-Attack parent `YT:[292A,2E80)` and active-main A-cycle
  composition are now candidate. The former coarse live path omitted the
  `<Attack>` title and required hydrations, used different prompts and rows,
  wrote casualties in a different order, skipped the durable precombat
  reserve, formatted the real defender loss in the casualty radio, displayed
  the final current total rather than the stale post-radio reserve, and sent
  every result through sector reentry. The native owner now scans cached
  candidates in ascending order, preserves teammate/decline exhaustion,
  performs the exact target/current validation reads, overlays the reserve
  before sound and RNG, reproduces the MBF64-low-four-byte SINGLE radio alias,
  performs current-before-target casualty writes, emits the DOUBLE result
  rows, persists shield spill through fresh target/current images, and falls
  into the existing fighter-kill/fatal child. Only candidate exhaustion enters
  the existing sector/hazard loop; direct results return to the loop-top fresh
  prompt. Binary-safe row, stale-value, rounding and unrelated-byte fixtures
  are native; all 29 isolated-body and 34 joined cycle/hazard upstream tests
  plus all nine native suites pass. All 74 parent transfers and the active-
  cycle presentation family are candidate. Exact native 327/370-byte active
  and admitted-hazard joined fixtures, independent raw A41C/FIELD/cache/RNG/
  radio state, local framebuffer and every staged failure prefix remain open.

## The task

Reimplement Yankee Trader 3.6G in C17 using OpenDoors. The target is Windows,
Linux, FreeBSD, macOS, and other platforms supported by the selected
interfaces.

Exact compatibility is the default requirement:

- From the player's point of view, the native door must be indistinguishable
  from 3.6G for every normal, alternate, error, interruption, timeout,
  carrier-loss, and SysOp-triggered path documented by the analysis.
- The native utilities must retain the same SysOp functionality and
  persistent side effects. Their native interface need not imitate DOS where
  the user has explicitly relaxed that requirement, but behavior and files
  still must.
- Never decide how the game “should” behave. If the completed analysis does
  not answer a behavior needed for implementation, stop and ask the user so
  the analysis can be updated.
- Do not replace an exact child operation with a convenient aggregate result.
  Preserve operation order, intermediate persistence, output order, random
  draws, interruptible boundaries, error prefixes, and non-atomic failure
  behavior.
- Do not describe a subsystem as complete merely because its happy-path game
  rule exists in C. Completion requires comparison against the completed
  analysis and proportionate tests.

There are three intentional high-level departures from the original:

1. Use a good platform CSPRNG instead of BRUN's poor generator. Preserve the
   documented number, placement, and use of random draws; replace only the
   source of each new uniform value. `src/yt_random.c` currently draws 24
   random bits from the platform entropy provider and converts them to a
   `float` in `[0,1)`.
2. Use `float` and `double` internally, but use MBF32/MBF64 exclusively at
   legacy serialization boundaries. Do not introduce IEEE floating-point
   bytes into any compatible file.
3. Retain every reached local BASIC `PLAY` operation as an ordered logical
   presentation event, but perform no host audio operation for it. OpenDoors
   exposes no portable local ANSI-music or BASIC `PLAY` endpoint, and the user
   explicitly selected this no-op supported-platform boundary on 2026-08-26.
   Remote ANSI-music and BEL bytes remain exact and are not covered by this
   departure.

## Source authority

Use evidence in this order:

1. The completed byte-pinned 3.6G analysis and its generated artifacts.
2. Original executable observations and retained fixtures linked by that
   analysis.
3. Original documentation and change history, only with the precedence
   assigned to them by the analysis.
4. Existing C code in this door.

The C code is an implementation candidate, not an authority. Some of it was
written before exact presentation, BRUN behavior, error edges, and connected
control flow were known.

Start with these analysis indices, at their current post-completion content:

```text
/bbsdev/doors/yt/3.6G/docs/STATUS.md
/bbsdev/doors/yt/3.6G/docs/architecture/reverse-engineering-traversal.md
/bbsdev/doors/yt/3.6G/docs/architecture/player-output-coverage.md
/bbsdev/doors/yt/3.6G/docs/architecture/root-coverage.md
/bbsdev/doors/yt/3.6G/docs/architecture/global-state-registry.md
/bbsdev/doors/yt/3.6G/docs/file-formats/file-lifecycle.md
/bbsdev/doors/yt/3.6G/docs/runtime/used-brun-operations.md
```

Do not treat a high-level semantic oracle as a substitute for connected raw
world, I/O, output, and return evidence. The analysis often retains both;
use the connected component for implementation and the higher-level result as
an independent cross-check where appropriate.

## First actions after resuming

1. Read this document and the completed analysis indices above.
2. Confirm the analysis tree is at its completed state. Ignore the former
   returning-login/Xannor frontier; it was still moving when this document was
   written.
3. Run the native baseline:

   ```sh
   cd /bbsdev/doors/yt/ng
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
   cmake --build build
   ctest --test-dir build --output-on-failure
   ```

4. Create an implementation-coverage ledger in this door. Map every final
   analysis component, reachable edge, external effect, and executable to:

   - the C function that implements it;
   - the exact test or fixture that proves it;
   - `missing`, `candidate`, `verified`, or `explicitly deferred` status; and
   - any prerequisite shared runtime, input, output, file, or numeric work.

   Do this before making broad claims or opportunistic gameplay edits. The
   analysis coverage ledgers prove analysis ownership, not C implementation
   ownership.
5. Audit from shared foundations outward. When a common primitive is wrong,
   fix and test it before repairing every caller separately.
6. Keep exact compatibility, exploit hardening, terminal enhancements, and
   native automation as separate scopes. Do not let an enhancement silently
   alter compatibility mode.

## Current door contents

The build produces these seven programs:

```text
yt
ytmaint
ytconfig
yt-init
rmt-init
portname
local
```

The authoritative build is in `CMakeLists.txt`. It consumes the pinned
`third_party/opendoors` checkout through the upstream `OpenDoors::Static`
CMake target. `GNUmakefile` is only a convenience wrapper around that same
build graph; it is not an independent legacy integration.

The main implementation areas are:

| Area | Current files | Present state, not a completion claim |
|---|---|---|
| MBF and QuickBASIC helpers | `src/qb.c`, `src/qb.h`, `src/yt_score_format.c` | MBF32/64 conversion, INT/FIX/CINT/VAL, string normalization, generic numeric output, and a dedicated PRINT USING scoreboard formatter exist. Their edge domains still require final-analysis audit. |
| Raw records and files | `src/yt_data.c`, `src/yt_file.c`, `src/yt_text.c`, `src/yt_names.c` | 137-byte records, 86-byte radio records, DOS EOF text files, case-insensitive DOS-style path resolution, alias files, and mutation-in-place APIs exist. Typed encoders retain the original raw record and rewrite only values that compare changed, protecting unbound bytes and noncanonical zero residue in many cases. |
| Platform layer and RNG | `src/yt_platform.c`, `src/yt_random.c` | Windows entropy/process/clock code and Unix-like FreeBSD/macOS/Linux branches exist. Only the present FreeBSD build has been exercised in this handoff. |
| Initializers | `src/yt_init.c`, `src/main_yt_init.c`, `src/main_rmt_init.c` | World generation, auxiliary files, direct-versus-maintenance handoff, and RMT reset paths exist. A clean-install subprocess test now exists, but full fault ordering and every randomized byte have not thereby been proven. |
| Maintenance | `src/yt_maint.c` | Player expiry, port/planet production, radio compaction, news rotation, scoreboard, Wanderer, Xannor, mercenary, and lottery code exists. Its size and breadth are not evidence that all final analysis branches or prefixes match. |
| Configuration and utilities | `src/main_ytconfig.c`, `src/main_portname.c`, `src/main_local.c` | Broad interactive behavior exists and has limited executable-level tests. Exact transcripts, errors, and all mutation boundaries still need ledger-based verification. |
| Scoreboard | `src/yt_score.c`, `src/yt_score_format.c` | Generation and cached-score writes exist. Boundary fixtures cover many PRINT USING overflow and exponent forms. Compare the entire final score contract, including failures and partial output. |
| OpenDoors adapter | `src/yt_door.c`, `src/yt_input.c`, `src/yt_input_model.c`, `src/yt_output.c`, `src/main_yt.c` | The player session now obtains origin-tagged raw OpenDoors events through a split local/remote FIFO for the distinct B05D and AB36 selection predicates. The pure arbitration and B05D queue mutations have deterministic fixtures, but physical overflow/failure behavior, remaining input consumers, startup/shutdown, and cross-platform behavior are not closed. |
| Player session | `src/yt_session.c` | Roughly 9,600 lines cover admission, movement, hazards, combat, ports, Earth, planets, teams, missiles, plasma, radio, computer reports, and exit. Cruise/plasma owners now map their recovered opening, route/reroute, defense, mine, player, planet, news, sound, wait, and terminal paths; the action finalizer, shared port-treasury body, movement danger scanner, team audit ordering, press-any-key presentation, both profit reports, the Info panel, the silent planet-production updater, and the nearest-port layer scanner have also been reconciled structurally. The six launch-time commodity-base draws are now restored through the replacement CSPRNG boundary. Treat all of it as candidate code requiring compositional comparison with completed connected analysis. There will be no monolithic end-to-end session transcript requirement; use deterministic native component fixtures plus separately verified presentation, persistence, input, RNG, and physical-adapter boundaries. |
| Original package assets | `src/yt_assets.inc`, `src/main_assetgen.c`, `data/` | Eight distributed files are regenerated and checksum-tested. |

There are few `TODO` markers. Their absence is not evidence of closure.

## Current test baseline

The native baseline command is:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

It ran:

```text
test_qb: ok
test_data: ok
test_sound: ok
input-model tests passed
test_presentation: ok
test_assets: ok
test_score: ok
test_clean_install: ok
test_utilities: ok
```

The tests currently prove useful foundations:

- selected MBF32/MBF64 vectors and QuickBASIC numeric/string helpers;
- bounded B05D/AB36 input-source arbitration, source FIFO, B05D control-key
  queue mutations, and editor endpoint routing;
- basic raw-record preservation, CSPRNG injection, random-file I/O, and DOS
  EOF text append behavior;
- exact sizes and checksums of the eight distributed package assets;
- selected scoreboard PRINT USING boundaries, the 603-byte empty-game
  scoreboard, cached scores, and partial-file behavior;
- an actual `yt-init` subprocess in a fresh package-like directory, followed
  by the sibling `ytmaint`, with expected output markers, core file sizes,
  configuration defaults, player defaults, fixed sector/ports/planets, and
  the early-EOF destructive truncation boundary; and
- limited executable behavior for YTCONFIG, standalone RMT-INIT, and LOCAL,
  plus the logical PORTNAME application transcript, parser and raw mutation
  transaction below its still-open physical BRUN adapters.

They do **not** currently prove:

- any complete remote or local player transcript;
- ANSI/plain output bytes, framebuffer results, pagination, or arbitrary
  user/file text safety;
- OpenDoors initialization, command-line parsing, carrier loss, timeout,
  chat, menu/toolbar commands, SysOp actions, or shutdown;
- caller-specific sound placement through deterministic native component fixtures (the dispatcher and 59-site static inventory now have exact focused tests/mapping);
- exact session mutation and random-draw order across gameplay branches;
- runtime error routing, partial physical I/O, interruption points, or
  non-atomic failure prefixes across the whole door;
- full `ytmaint`, YTCONFIG, RMT-INIT, or LOCAL transcripts and fault families,
  plus PORTNAME's physical BRUN/editor/framebuffer and complete fault-adapter
  families;
- Windows, Linux, or macOS builds; or
- mixed operation in which original and native binaries alternately mutate
  the same files.

The clean-install test intentionally checks structure and invariants rather
than a single full-world hash because the replacement RNG is intentional and
the clock is live. Add deterministic-provider/time fixtures when exact
randomized mutation order must be proved.

## Known high-priority gaps

### 1. Player session is broad but not compatibility-audited

`src/yt_session.c` contains much of the game's apparent functionality, but
many strings and presentations visibly predate the final exact analysis. It
also uses many ordinary C `%.9g`/`%.15g` formats. Those are not general
substitutes for BRUN numeric conversion or QuickBASIC PRINT USING.

Do not perform a spot review and call the session complete. Use the completed
player-output and rooted-control-flow ledgers to verify every entry, branch,
join, output byte stream, local result, state mutation, file effect, random
draw, and exit. Replace shortcuts with shared exact components where the
analysis shows shared code.

### 2. General OpenDoors output is knowingly unsuitable

`src/yt_output.c` currently sends all output through
`od_disp(..., TRUE)`. On Win32 the local display does not parse ANSI on that
path; ESC and following bytes become visible glyphs. This already makes
`YTOPEN.ANS` wrong locally. The same path is unsafe for unsanitized player,
BBS, registration, message, and external-file text.

Read `OPENDOORS-IO.md` in full before changing any I/O call. In summary:

- Intended NUL-free ANSI/AVATAR presentation should normally use
  `od_disp_emu(text, TRUE)`, with `od_no_ra_codes = TRUE` and every other
  relevant OpenDoors global set deliberately. In that configuration the
  intended terminal commands are interpreted for the Win32 local display
  while the original string is transmitted remotely.
- Use `od_disp()` with local echo only for exact counted output that is proven
  to contain no terminal-emulator commands. Its special reasons for existing
  in this door are counted or embedded-NUL data and remote-only output, not
  general presentation.
- Arbitrary data must follow the original validation/sanitization rules; do
  not invent filtering and do not treat terminal emulation as sanitization.
  A blanket switch from `od_disp()` to `od_disp_emu()` would let hostile or
  incomplete sequences in that data alter the local screen and the
  emulator's persistent parser state.
- `od_printf()`, `od_input_str()`, `od_disp_emu()`, and `od_send_file()` each
  add behavior and are not wholesale replacements.
- The installed `CP437UTF8` option must remain off for byte-exact CP437
  sessions.

The ineffective pre-`od_init()` assignment to `od_always_clear` is also
documented there. Its correct post-init value must come from completed
behavioral evidence, not preference.

### 3. Sound dispatcher and caller integration are implemented candidates

`src/yt_sound.c`, `src/yt_presentation.c`, `src/yt_output.c`, and
`src/yt_session.c` now implement the recovered dispatcher, the `X` toggle,
and every qualified live member of the 59-static-caller inventory.  The
alternate `YT:4F2D` selector-1 transfer arm is recorded as qualified dead:
the completed transfer oracle proves that the action initializes its private
flag to zero and never changes it, so only `YT:4F48` selector 4 is admitted
without asynchronous memory corruption.  This remains candidate rather than
verified whole-session behavior until caller-specific transcript/effect and
physical-endpoint fixtures are joined.

Legacy ANSI music must be sent as exact counted **remote-only** bytes.
OpenDoors does not parse it correctly for its Win32 local display:

- `od_disp(..., TRUE)` renders the sequence as glyphs;
- `od_disp_emu()` treats legacy `CSI M` as Delete Line and then renders PLAY
  bytes; and
- the terminal byte `0x0e` is Shift Out on a real VT-compatible terminal.

Local BASIC `PLAY` is retained in the ordered logical presentation timeline
but deliberately causes no host audio operation, per authorized departure 3
above.

`TERMINAL-COMPATIBILITY.md` records a possible positively detected CTerm
`CSI |` enhancement and the raw-input demultiplexer it would require. That is
an enhancement, not permission to alter compatibility mode. No fallback
policy has been selected, so do not implement one by judgment.

### 4. OpenDoors SysOp integration is incomplete

The pinned OpenDoors revision supports the DOS-style personality and custom
hotkey dispatcher under DOS and the Win32 console subsystem. Its built-in
one-row personality (or a custom one-line personality) leaves a local status
row and provides an `ON KEY`-style place to connect the legacy F4-F10
controls. The native utilities may also use OpenDoors in forced-local mode to
share this local input surface. Exact legacy utility-local presentation
remains optional; the actions, persistent effects, and player-visible
consequences do not. The Win32 GUI subsystem can instead use OpenDoors'
buttons, menus, accelerators, and chat facilities, while the Unix-like
backend remains headless.

The SysOp view need not be identical to DOS, but every action must have the
correct consequence for the remote player and game state. In particular,
OpenDoors time adjustment currently changes `od_control.user_timelimit`,
while `src/yt_session.c` enforces its own `session_deadline`; these can
diverge. Chat, exit/hangup, time changes, carrier handling, and any relevant
local presentation must be connected and tested. The exact legacy behavior
is in the completed `docs/runtime/sysop-controls.md` analysis; the OpenDoors
selection constraints are in `OPENDOORS-IO.md`.

This adapter choice does not by itself authorize the explicitly deferred
cross-platform startup/exit phase. Install it when work reaches the relevant
main-door or utility startup boundary, with the same pre-init cleanup and
partial-init requirements as every other OpenDoors configuration call.

### 5. Cross-platform OpenDoors startup and exit remain deferred

The user explicitly deferred final OpenDoors startup/exit work on platforms
other than the currently exercised one until later work. `src/main_yt.c` and
`src/yt_door.c` contain a provisional WinMain/main split, command-line
prefill/parsing, cleanup hook, `od_init()`, `od_exit()`, and final `exit()`.
Do not assume it is approved merely because it builds.

Preserve these non-negotiable facts when that phase is authorized:

- OpenDoors can call `exit()` during initialization and ordinary I/O.
- Install all cleanup state and `od_before_exit` before any call that may
  initialize OpenDoors.
- `od_parse_cmd_line()` has different Win32 and non-Win32 signatures and
  subtle ownership/ordering rules.
- It must be called exactly as required relative to `od_init()`.
- Avoid double initialization and double shutdown.
- Preserve a valid cleanup path for partially opened game state.

Until that phase, do not let unrelated compatibility work casually rewrite
these boundaries.

### 6. Error and persistence behavior needs explicit coverage

The original programs often write output, mutate a FIELD buffer, perform a
`PUT`, append a line, delete a file, or rename a file before a later
operation fails. Those prefixes are observable and frequently non-atomic.
The C code generally propagates a single `yt_error`, but that alone does not
reproduce BRUN error selection, retries, cleanup, or already-committed
effects.

Use the final file lifecycle and error-router analysis. Test injected
failures at each documented physical operation, not just a generic failure
at the outer function.

## Persistent compatibility rules

- `YTDATA.DAT` records are exactly 137 bytes. Preserve all fields not written
  by the reached operation, including bytes 133-136 and any layout-specific
  residue.
- `YTRMSG.DAT` records are exactly 86 bytes, including the documented
  maintenance compaction behavior.
- Preserve MBF exponent-zero values when the original operation does not
  rewrite them. Numeric equality is not always byte equality.
- Preserve text CRLF, DOS EOF, padding, truncation, append positioning, and
  per-line durability exactly.
- Preserve each executable's literal filename spelling and operation order.
  The native filesystem compatibility layer must provide DOS-style
  case-insensitive lookup on case-sensitive hosts without casually treating
  distinct host directory entries as interchangeable. Audit ambiguous
  case-colliding files explicitly.
- The native and original binaries must be able to alternate over the same
  files. Test both directions with documented fixtures: original-created
  files mutated natively, and native-created files consumed and mutated by
  the original binaries.
- Do not add fields to legacy records, assign meanings to unbound tails, or
  add required sidecar state for exact mode.

The earlier fresh-install failure was caused by an incorrect maintenance
rotation implementation, not by permission to skip rotation. The analysis
establishes that YTMAINT opens/closes both current and yesterday news in
append mode before deleting yesterday and renaming current. Thus missing
files are created before the destructive operations. `rotate_news()` now
models that order, and `tests/test_clean_install.c` exercises a fresh
`yt-init` handoff. Retain this regression.

## Existing design notes in this door

These documents are requirements or discussion records, not proof that their
features are implemented:

- `OPENDOORS-IO.md`: complete OpenDoors I/O function-selection study and
  current call-site audit. Treat its warnings as active until code and tests
  prove otherwise.
- `TERMINAL-COMPATIBILITY.md`: possible CTerm/SyncTERM ANSI-music capability
  detection. No policy decision has authorized it.
- `EXPLOIT-MITIGATIONS.md`: exact-mode exploit behavior and possible future
  hardened rules. Do not implement hardened behavior without an explicit
  ruleset decision.
- `NATIVE-AUTOMATION.md`: inventory of terminal scripts/macros that should
  eventually have native, terminal-independent equivalents. These are
  extensions, must use ordinary game operations and costs, and are not a
  substitute for completing exact compatibility.

The important separation is:

```text
exact 3.6G compatibility
    != exploit-hardened rules
    != terminal-safety enhancements
    != native player automation
```

Do not combine these in one unlabelled mode.

## Recommended verification order

This is an engineering dependency order, not a decision about game behavior:

1. Build the final implementation-coverage ledger from the completed
   analysis.
2. Verify and extend MBF, BRUN numeric formatting, PRINT USING, date/time,
   string, path, raw-record, and file-operation foundations.
3. Verify initializers and noninteractive maintenance with deterministic
   clock/RNG/I/O providers and byte-level fixtures, including every failure
   prefix.
4. Replace the general output shortcut with provenance-aware exact output
   paths. Make carefully configured `od_disp_emu()` the normal path for
   intended ANSI/AVATAR presentation, reserve `od_disp()` for its narrower
   counted/plain and remote-only roles, and add remote-byte plus Win32
   local-framebuffer tests.
5. Add caller-specific component/effect fixtures around the implemented sound dispatcher and its 59-site static inventory.
6. Build reusable input/session-runtime adapters for exact queue, pager,
   timeout, carrier, and SysOp consequences.
7. Audit startup/login and then each reachable session component in rooted
   control-flow order, composing shared exact children instead of creating
   caller-specific approximations.
8. Verify YTCONFIG, PORTNAME, RMT-INIT, LOCAL, and YTMAINT whole-program
   transcripts and persistent effects.
9. Complete the explicitly deferred platform-specific OpenDoors startup and
   exit work when authorized.
10. Only after exact compatibility is closed, ask for decisions needed by
    terminal enhancements, hardened rules, and native automation.

For each component, tests should cover at least:

- exact remote bytes for ANSI and plain modes;
- the correct local result, especially the Win32 80x25 display;
- accepted, rejected, empty, repeated, queued, local, and remote input;
- normal, alternate, END, carrier-loss, timeout, and SysOp-triggered exits;
- raw before/after records and auxiliary files;
- random draw order with an injected provider;
- time/date boundaries, including BRUN's recovered oddities;
- every documented partial physical I/O and error edge; and
- continuation into the real caller without replaying predecessor output or
  mutation.

## Stop conditions

Stop and ask the user when:

- the completed analysis lacks a value, branch, byte sequence, failure
  ordering, platform consequence, or external observation needed by the C
  implementation;
- two final analysis artifacts disagree;
- an apparent fix requires choosing between exact behavior and a safer,
  cleaner, or better-balanced behavior;
- terminal capability fallback, hardened-rule constants, automation
  semantics, or new persistent state requires a policy decision; or
- completing a task would require expanding into the explicitly deferred
  OpenDoors startup/exit phase without authorization.

Do not fill these gaps with conventional behavior, likely intent, host API
defaults, or personal judgment.

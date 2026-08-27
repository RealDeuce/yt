# Native implementation coverage

This is the implementation ledger required by `IMPLEMENTATION-HANDOFF.md`.
It measures the C17/OpenDoors candidate against the completed byte-pinned
3.6G analysis.  It does not reinterpret the analysis coverage ledgers as
native implementation evidence and does not combine unlike inventories into
an overall percentage.

## Status rules

| Status | Meaning |
|---|---|
| `missing` | No exact native C owner and exact native test are mapped yet. Existing nearby code does not change this status. |
| `candidate` | A named C owner exists, but comparison and/or exact native testing is incomplete. |
| `verified` | The declared bounded component has an exact native test or fixture and matches the final analysis for that scope. |
| `explicitly deferred` | The handoff or user explicitly deferred this scope; it is not silently counted as done. |

Any row promoted to `verified` must name the implementing C function and the
specific native test or fixture. A whole-program smoke test cannot verify an
unrelated child root, qualified transfer, failure prefix, or presentation
family.

Session proof is compositional. A monolithic interactive-session transcript
is not required: bounded deterministic native fixtures may prove connected
components, while presentation, persistence, input/runtime, RNG placement,
and physical adapters are proved independently and joined through their
declared seams.

## Final-analysis denominators

The generated companion ledgers pin these independent final inventories:

| Axis | Ledger | Denominator | Initial native disposition |
|---|---|---:|---|
| Reachable address-owned roots | `implementation-coverage/roots.tsv` | 168 | Program entries and mapped child owners are `candidate`; unmapped children remain `missing`, and none are promoted without exact native evidence. |
| Qualified transfers | `implementation-coverage/transfers.tsv` | 7,368 | Mapped edges are `candidate`; unmapped edges remain `missing`. Current mapped slices include the 59-site sound-call inventory (with one qualified-dead edge), movement danger scanning, team radio audit, shared press-any-key handling, both computer profit reports, all seven incoming Info-panel calls, the planet-production updater with its eleven callers, the complete 143-edge nearest-port body plus its incoming call, the 48-edge shared date-helper body (including two invariantly dead edges) plus mapped startup and port-updater calls, and the 29-edge ordinary-port updater/report pair with all six incoming calls. |
| Presentation families | `implementation-coverage/presentation.tsv` | 150 families owning 3,815 sites | Six site-free catalogs are recorded as such; all behavior-owning families begin `missing`. |
| Literal references | final `player-output-coverage.json` pinned by `manifest.json` | 2,060 | Credited through their owning presentation component, never independently by string-search resemblance. |

`implementation-coverage/manifest.json` pins the SHA-256 of every imported
analysis artifact and refuses regeneration if the completed denominators
change. Regenerate with:

```sh
python3 tools/update_implementation_coverage.py
```

## Executable ownership

These are entry-level candidates only. Their listed tests prove the narrower
baseline stated in the handoff, not whole-program compatibility.

| Legacy executable | Native owner | Current evidence | Status | Principal prerequisites |
|---|---|---|---|---|
| `YT.EXE` | `src/main_yt.c::main`, `src/yt_door.c`, `src/yt_session.c` | bounded numeric, data, sound, presentation, asset, score, install, and utility fixtures; session behavior is proved compositionally | `candidate` | complete every rooted component and edge plus their presentation, persistence, input/runtime, RNG, failure, and physical-adapter seams; no monolithic session fixture |
| `YTMAINT.EXE` | `src/yt_maint.c::yt_maintenance_run` | `test_clean_install` covers the automatic fresh-install path | `candidate` | deterministic clock/RNG/I/O, full transcript/effects, every non-atomic failure prefix |
| `YTCONFIG.EXE` | `src/main_ytconfig.c::main` | limited `test_utilities::test_ytconfig` | `candidate` | exact transcript, all editors, mutation boundaries and faults |
| `YT-INIT.EXE` | `src/main_yt_init.c::main`, `src/yt_init.c` | `test_clean_install` | `candidate` | deterministic randomized byte/order fixtures and physical-failure prefixes |
| `RMT-INIT.EXE` | `src/main_rmt_init.c::main`, `src/yt_init.c` | limited `test_utilities::test_rmt_init` | `candidate` | standalone/handoff transcripts, reset distinctions, destructive errors |
| `PORTNAME.EXE` | `src/main_portname.c::main` | limited `test_utilities::test_portname` | `candidate` | all 908 names/draw ordering, corrupt record conversion, per-record fault boundaries |
| `LOCAL.EXE` | `src/main_local.c::main` | limited `test_utilities::test_local` | `candidate` | exact console/file/SHELL result domain and child boundary |

## Shared prerequisites

| Component | Native owner | Native evidence | Status | Required before promotion |
|---|---|---|---|---|
| MBF32/MBF64 serialization | `src/qb.c::{qb_mbf32_*,qb_mbf64_*}` | exact boundary/ties-even vectors plus 16,384 deterministic raw/host oracle cases in `tests/test_qb.c`; dirty-zero, underflow and overflow preservation in `tests/test_data.c` | `verified` | operation-specific arithmetic overflow and runtime-error routing remain separate components |
| BRUN `INT`/`FIX`/`CINT` arithmetic | `src/qb.c::{qb_cint,qb_cint_mode}` | nearest/half-away and live-mode-`04h` floor boundary vectors in `tests/test_qb.c` | `candidate` | complete raw malformed-MBF source handling, special mode-byte residues, overflow state and runtime-error routing; audit all callers for the live conversion mode |
| BRUN `VAL` semantic grammar/result | `src/qb_format.c::qb_val` | recovered decimal/radix/underflow/overflow vectors and exact raw-MBF64 results in `tests/test_qb.c` | `verified` | allocator/heap/CPU scratch and ERR-6 routing remain part of the separate public runtime transition |
| General BRUN `STR$`/numeric core | `src/qb_format.c::{qb_str_integer,qb_str_single,qb_str_double,qb_str_mbf32,qb_str_mbf64,qb_print_*}` | recovered boundary/double-rounding vectors and 8,192 deterministic raw-MBF oracle vectors in `tests/test_qb.c` | `verified` | caller BASIC-type mapping, comma/newline destination routing and complete PRINT statements remain separate components |
| Scoreboard `PRINT USING` | `src/yt_score_format.c` | final adjacent boundary, sign-order and AA2D guard-stream vectors plus 40,960 deterministic raw-MBF/field oracle cases in `tests/test_score.c` | `verified` | row/file write failure prefixes remain part of scoreboard generation rather than the numeric formatter |
| BRUN `DATE$`/`TIME$` value formatting | `src/yt_config.c::{yt_format_date,yt_format_time}` | all 43,830 valid DOS dates and all 8,640,000 DOS time tuples in `tests/test_score.c` | `verified` | allocator/heap/CPU residue and caller-specific observation ordering remain separate runtime components |
| Shared game-day serial helper | `src/yt_config.c::{date_serial_epoch,yt_date_serial,yt_current_date_serial}` | bounded same-year, adjacent/stale epoch, year-100 leap, fractional-epoch, and injected-clock fixtures in `tests/test_score.c::check_date_serial` | `candidate` | physical clock failure and BRUN DATE$/substring/VAL allocation, scratch, by-reference, and caller-copy residues remain separate seams; unreconciled planet-creation and Earth-store callers remain missing |
| `YT:B05D` paged text/terminator seam | `src/yt_session.c::session_b05d`, `src/yt_input.c::yt_input_poll_legacy`, `src/yt_input_model.c`, `src/yt_presentation.c::{yt_present_paged_text,yt_present_paged_finish}` | exact raw-text then `LF/CR` remote split, local newline/color reset, newline suppression, mode-specific local/remote arbitration, source FIFO, extended-key rejection, and Ctrl-X/Ctrl-R/printable/CR queue fixtures in `tests/test_{presentation,input_model}.c` | `candidate` | carrier/physical partial writes, complete nested pager prompt families, runtime failure routing, and integration fixtures over the session owner |
| Shared name/uppercase byte transforms | `src/qb.c::{qb_title_case_n,qb_ascii_upper_n,qb_compat_upper_n}` | exact punctuation/apostrophe examples, embedded-NUL vector, and exhaustive 256-byte ASCII/compatibility transform checks in `tests/test_qb.c`; callers split between YT-SUB compatibility uppercase and LOCAL/YTCONFIG ASCII-only uppercase | `verified` | movable-string heap allocation/faults and caller-specific raw-length gates remain separate components |
| Raw 137/86-byte records | `src/yt_data.c`, `src/yt_game.c` | selected preservation in `tests/test_data.c` | `candidate` | every layout-qualified alias and byte-preservation mutation |
| Successful BRUN sequential APPEND image | `src/yt_text.c::yt_text_append_line` | final-128-byte boundary/earliest-marker, ignored `length-129`, missing-marker, embedded-NUL, embedded-EOF and repeat-append fixtures in `tests/test_data.c` | `verified` | 128-byte intermediate PRINT commits and physical failure prefixes remain a separate adapter component |
| YTNAME four-string `INPUT #` value grammar | `src/yt_names.c::{input_token,yt_names_load}` | cross-row groups, quoted comma/CRLF, doubled quote, NUL, LF-CR, EOF-tail, incomplete-group and exact-comparison fixtures in `tests/test_utilities.c` | `candidate` | dynamic strings above the native row capacity and allocator/store/failure prefixes are not yet represented |
| DOS-style file/text operations | `src/yt_file.c`, `src/yt_text.c` | exact-spelling preference and ambiguous case-collision checks plus selected read/write/delete cases in `tests/test_data.c` | `candidate` | nested path components, buffering, partial operations, retry/error mapping |
| Replacement CSPRNG provider | `src/yt_random.c::{yt_random_next,yt_random_market_bases}` | injected-provider tests in `tests/test_data.c`, including the exact six-draw launch-base calculation | `candidate` | verify draw placement/use for remaining callers; deterministic provider-error tests |
| Distributed immutable assets | `src/yt_assets.inc`, `data/` | exact sizes/checksums in `tests/test_assets.c` | `verified` | none for the declared eight-file byte scope |
| General OpenDoors output | `src/yt_output.c::{yt_out_bytes,yt_out_remote_bytes,yt_out_present_result}` | ordered replay-sink and PC-attribute vectors in `tests/test_presentation.c`; optimized/debug OpenDoors link | `candidate` | presentation replay now uses remote-only `od_disp` plus local-only counted `ODScrn` operations; add physical-result and Win32 framebuffer tests and migrate remaining provenance classes |
| Shared color/direct-output/attention transducers | `src/yt_presentation.c` | exact ANSI/plain color cache, equal-color normalization, live-CINT-mode conversion, snoop/mode endpoint routing, CRLF framing, ordered attention/toggle endpoint timelines, right/fixed/centered width and ANSI-disabled bold-residue vectors in `tests/test_presentation.c` | `candidate` | replace fixed native result capacities with exact dynamic-string/failure behavior; prove physical OpenDoors writes and partial failures; connect all callers; local BASIC `PLAY` is the authorized ordered-event/no-host-audio departure |
| Sound dispatcher, 59-site caller inventory, and main `X` toggle | `src/yt_sound.c::{yt_sound_dispatch,yt_sound_toggle}`, `src/yt_session.c::{session_sound,session_attention,command_shell}` | exact nine-cue, ANSI/plain, live-CINT-mode gates, stale-scratch, toggle/mirror and conversion-failure-prefix vectors in `tests/test_sound.c`; all qualified-live static callsites are mapped to session operations; remote ANSI/BEL uses `yt_out_remote_bytes` | `candidate` | add caller-specific component/effect fixtures, direct-line physical-failure tests, OpenDoors remote-write/Win32 framebuffer tests, and joined scanner/re-entry ordering; `YT:4F2D` selector 1 is qualified dead absent asynchronous corruption, while local BASIC `PLAY` is retained logically and intentionally silent |
| Cruise-missile and plasma-bolt resolvers | `src/yt_session.c::{launch_projectile,missile_sector,missile_planet_impact,plasma_sector,plasma_planet_impact}` | completed analysis contracts are mapped to native launch, routing/rerouting, defense, mine, player, planet, news, sound, wait, and terminal paths; release/debug builds and broad tests compile the owners | `candidate` | add exact deterministic component/effect fixtures; verify every early terminal, physical failure prefix, special-attacker label, corrupt numeric state, presentation latch, and shared death/salvage/HQ join |
| Info panel and captain resolver | `src/yt_session.c::{show_ship,info_refresh_time,info_team_lines,info_ordinary_row,info_commodity_row}` | recovered call order, repeated GET/promotion PUT sequence, exact CP437 geometry, BASIC numeric types, commodity color transitions, and seven incoming callers are represented; debug suite passes | `candidate` | add bounded deterministic branch/state/effect and failure-prefix fixtures, corrupt-state behavior, and physical framebuffer proof |
| Planet-production updater | `src/yt_session.c::planet_update` | ordinary reachable formulas and fourteen-field preservation are represented; DATE now precedes GET, the sole TIMER follows quantity/rate hydration, and all eleven callers plus fifteen internal transfers are mapped | `candidate` | add bounded deterministic raw-record/effect and failure-prefix fixtures; raw dirty-zero arithmetic residue follows the authorized IEEE-internal-float departure |
| Nearest-port layer scanner | `src/yt_session.c::{computer_nearest_ports,nearest_more,nearest_market_cell,project_port_market}` | ordinary BFS discovery order, descending ties, filters, DATE/GET/TIMER ordering, projected rows, private pager, final blank, one incoming call, and all 143 internal transfers are mapped | `candidate` | add bounded deterministic row/filter/pager/effect and failure-prefix fixtures; retain separate input-adapter proof for local/remote arbitration and corrupt raw-record conversions |
| Ordinary-port updater and commerce report | `src/yt_session.c::{port_update,port_report,port_owner_row,port_report_length,port_report_right}` | all three updater/report caller pairs and 29 internal transfers are mapped; updater preserves DOUBLE projected stocks separately from SINGLE persistence, and report composes cached owner/treasury, fresh player, fresh port name, DATE/TIME, mixed line framing, typed numeric fields, and color rows | `candidate` | add bounded deterministic raw-record/arithmetic/report/effect and failure-prefix fixtures; exact input arbitration and physical adapter prefixes remain separate; fixed-price trade is its own child component |
| Input/session runtime | `src/yt_input.c`, `src/yt_input_model.c`, helpers in `src/yt_session.c` | bounded origin-aware arbitration/queue fixtures in `tests/test_input_model.c` and exact editor endpoint fixtures in `tests/test_presentation.c::test_editor_echo` | `candidate` | complete editor save/repeat/semicolon/wait fixtures, pager transaction fixtures, timeout/carrier notices, physical queue overflow and partial-I/O results, interruption and SysOp consequences |
| Cross-platform OpenDoors startup/exit | `src/main_yt.c`, `src/yt_door.c` | none | `explicitly deferred` | user authorization plus platform-specific init/exit and partial-cleanup tests |

## Persistent and external effects

Each row is an independently observable legacy boundary. `candidate` means
the named native code touches it; it does not imply exact operation order or
fault behavior.

| External identity/effect | Native owner(s) | Native evidence | Status | Missing proof |
|---|---|---|---|---|
| `YTDATA.DAT` random records | `yt_file.c`, `yt_game.c`, initializer/maintenance/session callers | structure checks in `test_clean_install`; selected record tests | `candidate` | every GET/PUT prefix, FIELD identity, record residue, mixed original/native mutation |
| `YTNAME.DAT` alias rows/replacements | `yt_names.c`, `main_ytconfig.c`, `yt_maint.c`, initializer/local/session callers | limited utility/clean-install tests | `candidate` | exact grammar, all-match behavior, fan-out order, kill-before-rename faults |
| `YTNEWS.DAT` append/current news | `yt_maint.c::yt_news_append`, session append helper, initializers | fresh-install markers | `candidate` | all 101 constructions, DOS EOF buffering, per-call durability/failures |
| `YTYNEWS.DAT` rotation | `yt_maint.c::rotate_news` | fresh-install regression | `candidate` | complete open/close/kill/rename physical result matrix |
| `YTRMSG.DAT` radio store/compaction | `yt_session.c`, `yt_maint.c::yt_radio_compact` | size-only fresh-install check | `candidate` | all writers/readers, counter behavior, exact 72-byte compaction bug and failures |
| configured scoreboard / `YTSCORE.ASC` | `yt_score.c::yt_score_generate` | extensive selected formatter/file tests | `candidate` | full ranking/cache/error/partial-file contract and viewer join |
| `YT.REG` registration transaction | `yt_session.c::registration` | none | `candidate` | exact create/delete/read/validation/END/hang paths |
| `LOCKOUT.DAT` startup transaction | `yt_session.c::lockout` | none | `candidate` | exact create/reopen/parser/match/denial order and faults |
| `XANNORHQ.TXT` victory input | `yt_session.c::xannor_victory` | immutable asset checksum only | `candidate` | exact viewer/failure prefixes before every award/effect |
| `YTOPEN.ASC` / `YTOPEN.ANS` | `yt_session.c::opening_and_date`, `yt_output.c` | immutable asset checksums only | `candidate` | exact playback, missing-file behavior, remote bytes and local framebuffer |
| `DORINFO1.DEF` or argument | `yt_door.c`, `main_local.c` | limited LOCAL test | `candidate` | exact 12/13-line cursor/truncation/fault/SHELL boundaries |
| `ERRORS.DOR` | no exact native error router | none | `missing` | exact session-before-log append/PRINT/CLOSE and active-handler failure prefixes |
| `RMTINIT.TMP` | `main_rmt_init.c`, session Genesis path | limited standalone RMT test | `candidate` | exact producer/RUN handoff, first-line/delete ordering and failure domain |
| process, clock, entropy, serial/console, carrier and terminal state | platform/OpenDoors/session layers | injectable clock-provider order/exhaustion test plus entropy/clock smoke coverage | `candidate` | every documented external-result class and stateful adapter; remaining caller clock tapes |

## Promotion workflow

Work proceeds from shared prerequisites outward. For a bounded component:

1. identify its final analysis owner and all outgoing transfers;
2. name the exact C function(s), shared adapters, external effects and state;
3. add deterministic native tests for normal, alternate, interrupt, timeout,
   carrier, SysOp and physical-failure families that are reachable there;
4. compare remote bytes, local result, raw files/records, random draws and
   continuation state; and
5. update the root, transfer and presentation rows together. A component is
   not promoted while any reached child is replaced by an aggregate result.

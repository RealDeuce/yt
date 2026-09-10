# Terminal compatibility enhancements

Status: discussion document only. Nothing here changes the recovered Yankee
Trader 3.6G behavior or authorizes a wire-protocol difference in exact
compatibility mode.

## SyncTERM/CTerm ANSI music

### Desired enhancement

When the remote terminal is positively identified as CTerm-compatible,
prefer CTerm's `CSI |` ANSI-music introducer over the legacy `CSI M`
introducer.

Both forms carry the same BASIC `PLAY`-language string and end it with SO:

```text
legacy:  ESC [ M <PLAY string> 0E
CTerm:   ESC [ | <PLAY string> 0E
```

The CTerm form avoids overloading standard `CSI M` (`DL`, Delete Line).
CTerm recognizes `CSI |` in every one of its ANSI-music modes, including its
strict mode in which `CSI M` remains Delete Line. Yankee Trader's nine
recovered PLAY strings use only characters allowed by CTerm's music parser.

CTerm's documentation says an ANSI-music test must be followed by SI
(`0x0f`) to undo SO (`0x0e`) on a terminal that did not consume the music
terminator. The safer proposed CTerm transmission is therefore:

```text
ESC [ | <PLAY string> 0E 0F
```

The SI does not make an unsupported terminal play the music or suppress the
visible PLAY string, but it prevents SO from leaving a VT-compatible terminal
with G1 invoked into GL.

### Positive capability detection

CTerm implements the standard primary Device Attributes query:

```text
request:   CSI c
response:  CSI = 67;84;101;114;109;<revision> c
```

The five fixed decimal parameters spell `CTerm` in ASCII. The revision has
dots converted to semicolons, for example `1;156`.

This response is a stronger signal than a `TERM` value, BBS-reported terminal
name, caller-supplied string, or ANSI flag:

- the current CTerm documentation defines the response and directs callers
  to use its revision for feature detection;
- official source history added `CSI |` on 2005-11-15;
- official source history added the identifiable CTerm DA response on
  2005-11-20; and
- consequently every official CTerm build capable of returning this
  signature already contains `CSI |` support.

`APC SyncTERM:VER ST` can identify SyncTERM specifically, but it is narrower
than necessary. ANSI music is implemented by CTerm, so the CTerm DA response
is the appropriate primary capability signal.

No response proves only that support was not detected. It does not prove that
the terminal lacks ANSI music.

If all remote input passes through a raw, generic demultiplexer, detection
need not block startup or delay the player. Such an implementation can send
the DA query once, early in the session, leave CTerm support initially
unlatched, and recognize a complete valid CTerm DA response whenever it
eventually arrives. A late response would permanently latch `CSI |` support
for later music calls; it could not change music already emitted under the
selected not-yet-detected policy.

That input architecture is a precondition, not an assumed implementation
choice. Without it, an asynchronous DA response can be consumed by an
unrelated prompt, translated as a key sequence, or inserted into the player's
command text. In that architecture the door must not send the query merely
because the response format is known.

### OpenDoors integration constraints

OpenDoors 6.00 predates CTerm's extension. It contains no CTerm or SyncTERM
recognition, and `od_autodetect()` detects only ANSI and RIP. Its `user_ansi`
flag is therefore not evidence of `CSI |` support.

A continuously active detector would require the application to:

- send the DA query to the remote endpoint only, early in the session;
- never block session startup while waiting for the reply;
- recognize a response split across any number of input events;
- watch for a valid response throughout the session and latch support once;
- consume only the complete, validated CTerm response;
- preserve genuine player bytes that arrive before, during, or after it;
- prevent the response bytes from entering Yankee Trader's command queue;
- avoid consuming local SysOp input as terminal-response data;
- use a short inter-byte timeout only to disambiguate an incomplete response
  prefix from genuine player input, replaying every buffered byte in exact
  order after a mismatch or timeout;
- centralize session input through `od_get_input(..., GETIN_RAW)`, use
  `bFromRemote` to pass only remote bytes through the response recognizer,
  and perform any required player-key translation only after that recognizer;
- discard parser state cleanly on carrier loss or OpenDoors termination; and
- store the result only in per-session state, without changing any
  legacy-compatible data-file format.

The current Yankee Trader code centralizes all game-owned polling, blocking input,
and timed waits behind `yt_input.c`, using `od_get_input(..., GETIN_RAW)` or
`od_get_input_until(..., GETIN_RAW)`. It does not intercept input while an
OpenDoors-owned personality or chat interaction has control, so centralization
alone does not prove that a reply cannot arrive while OpenDoors owns the input
flow. Before
selecting asynchronous detection, the implementation must establish either:

- every possible input consumer can be routed through the demultiplexer;
- terminal responses are intentionally recognized only during a bounded
  early detection phase; or
- trustworthy terminal capability information is available out of band.

If none is true, automatic DA probing is not safe and must remain
unimplemented.

The query, reply, and music bytes must bypass OpenDoors' local display, just
as documented in `OPENDOORS-IO.md`. Neither `od_disp(..., TRUE)` nor
`od_disp_emu()` is suitable for them.

### Compatibility boundary still requiring a decision

Positive CTerm detection provides a safe basis for preferring `CSI |`, but
does not decide the behavior before a response arrives or when none ever
arrives. Candidate policies include:

- preserve the exact legacy `CSI M ... 0E` bytes and recovered endpoint
  gates;
- require explicit user opt-in before using the legacy form;
- suppress ANSI music and retain only the recovered plain/BEL behavior; or
- expose exact and terminal-safe policies as distinct compatibility modes.

That choice changes observable behavior and must not be inferred during
implementation. It requires an explicit project decision before the sound
dispatcher is completed.

### Required tests

- complete and byte-fragmented valid CTerm DA responses;
- old and current revision forms;
- immediate, heavily delayed, partial, malformed, and lookalike responses;
- mismatched or timed-out prefixes replayed as player input without loss,
  duplication, or reordering;
- simultaneous genuine player input, preserving its exact order;
- local SysOp input during detection;
- carrier loss and session-time expiration while a partial response prefix is
  buffered;
- entry into every OpenDoors-owned interactive facility before, during, and
  after a response, if asynchronous detection is selected;
- a late valid response changing only subsequent sound emissions;
- detected `CSI |` output with the exact recovered PLAY string, SO, and SI;
- confirmation that query, reply, and music never reach the Win32 local
  screen; and
- the explicitly selected no-detection fallback.

## Sources

- `/synchronet/src/sbbs/src/conio/cterm.adoc`, especially `CSI Pn M`,
  `CSI = Ps M`, primary Device Attributes, and the `"ANSI" Music` section.
- `/synchronet/src/sbbs/src/conio/cterm_cterm.c`, including the unconditional
  `CSI |` handler and SO-terminated music accumulator.
- OpenDoors selection and local-display analysis in `OPENDOORS-IO.md`.

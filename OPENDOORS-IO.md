# OpenDoors input and output function selection

## Scope and sources

This is the OpenDoors I/O selection guide for the Yankee Trader
reimplementation. It is based on a complete reading of
`third_party/opendoors/historic/OPENDOOR.TXT`, not only its API tables or
function-name index. The tutorial, control-structure reference, Win32 notes,
troubleshooting chapter, version history, glossary, and index contain
behavior that is absent from the individual function pages.

The pinned OpenDoors header and implementation were also checked where
the top of the manual announces later, undocumented additions. Those
additions are identified as such below. The installed public header is the
authority for their current names and prototypes; in particular, the note at
the top of the manual says `od_split_cmd_line_free()`, but the public symbol
is actually `od_free_split_cmd_line()`.

This document lists functions that do at least one of the following:

- consume local, remote, command-line, terminal-response, or screen-state
  input;
- produce local, remote, terminal-control, status-line, log, or child-process
  output;
- manipulate OpenDoors' model of the visible screen;
- select or materially control an interactive input/output facility; or
- pump I/O and the asynchronous behavior coupled to it.

Pure accessors and parsers with no I/O behavior, such as
`od_control_get()` and `od_color_config()`, are intentionally omitted.
`od_sleep()` is discussed only as a non-I/O contrast because mistaking it
for an I/O pump is consequential.

Manual page references use the printed page numbers in `OPENDOOR.TXT`.

## Rules that apply to nearly every choice

### An I/O call can initialize or terminate the process

Almost every public OpenDoors function implicitly calls `od_init()` if
OpenDoors is not initialized. Initialization can read configuration and drop
files, open communications, create the local interface, display startup
material, prompt in forced-local mode, and call `exit()` on a fatal startup
condition.

Before the first initializing call, install every setting and cleanup hook
that must survive an OpenDoors-triggered exit. The manual permits only
`od_add_personality()` and `od_parse_cmd_line()` before `od_init()`; the
installed command-line split/free helpers also do not initialize OpenDoors.

Most I/O calls also run `od_kernel()` directly or indirectly. A seemingly
simple display or input call may therefore:

- process a local SysOp function key;
- enter chat or a shell;
- alter the user's time;
- display a time or inactivity warning;
- notice carrier loss or timeout; and
- invoke `od_before_exit` and terminate the process.

No shared file update may be left in an interruptible state across such a
call.

### Local and remote are separate destinations

In remote mode, the common high-level display functions normally produce the
same visible output on the remote terminal and in the local output window.
In local mode there is no remote destination.

The two easily confused boolean parameters mean opposite things:

| Function | Boolean parameter | `TRUE` | `FALSE` |
|---|---|---|---|
| `od_disp()` | `bLocalEcho` | remote plus local | remote only |
| `od_disp_emu()` | `bRemoteEcho` | local plus remote | local only |

Thus, `od_disp(..., FALSE)` displays nothing in local mode, while
`od_disp_emu(..., FALSE)` is specifically a local-only display.

`od_silent_mode` suppresses the local interface and SysOp commands, except
that OpenDoors disables silent mode when operating locally.

On Win32, "local echo" does not write the bytes to a Windows console.
OpenDoors maintains its own 80-by-25 character-and-attribute buffer and
paints it with GDI. The raw local path used by `od_disp()` and
`od_disp_str()` recognizes only carriage return, line feed, backspace, tab,
and bell. Every other byte, including ESC (`0x1b`), is stored as a character
cell. The Win32 painter selects `OEM_FIXED_FONT` and passes those cells to
`TextOut()`. Consequently, `ESC [ 2 J` is locally rendered as the OEM glyph
for `0x1b` (normally the CP437 left arrow) followed by the literal characters
`[2J`; it is not interpreted as a screen clear.

The Unix-like backend is explicitly always locally silent. It updates an
allocated screen buffer but has no displayed local operator screen. This
does not make the Win32 distinction optional: Win32 is the platform on which
a remote player and visible local SysOp display are normally maintained
together.

### Input normally merges two keyboards

The normal input queue contains remote-user keys and non-function keys from
the local keyboard in arrival order. This deliberately lets the local SysOp
operate the door as though they were the player.

Use:

- `od_get_key()` when that merged behavior is desired;
- `od_get_input()` when the caller must know the origin or recognize
  extended keys; and
- DOS custom SysOp hotkeys when an action must be local-only and must not
  enter the player's input stream.

`od_control.od_last_input` is updated by `od_get_key()` to `0` for remote or
`1` for local input. It is not a substitute for the per-event
`bFromRemote` field returned by `od_get_input()`.

`DIS_LOCAL_INPUT` prevents local keys from becoming ordinary door input but
does not disable SysOp function keys. `DIS_SYSOP_KEYS` disables both.
`od_user_keyboard_on`, normally toggled by Alt-K, controls whether remote
input is accepted.

In the DOS backend, the `od_local_input` callback does **not** consume a key.
For a local non-SysOp-function key, OpenDoors calls the callback and then
places the same key in the common input queue. It is suitable for observing
local input, not for implementing an exclusive local command. The Win32
window path calls `ODKrnlHandleLocalKey()` directly and never invokes
`od_local_input`, so the callback is not a Win32 local-input hook.

### SysOp function-key handling is platform-specific

Under DOS, the kernel intercepts built-in status, chat, shell, hangup,
drop-to-BBS, keyboard-off, lockout, time-adjustment, SysOp-next, and
registered custom SysOp hotkeys before ordinary input. These do not reach
`od_get_key()` as player input.

The default status assignments include F1, F2, F3, F4, F5, F6, F9, and F10.
That table and its `od_hot_key[]`/`od_hot_function[]` dispatcher are inside
the DOS-only keyboard branch.

Win32 does not run that dispatcher. Its GUI accelerator table consumes
OpenDoors' own menu commands—Alt-C/H/K/L/N/X, Alt/Shift-Up/Down, and F1 for
Help. Other recognized navigation and F2-through-F10 keys are converted to
OpenDoors extended-key codes and placed directly in the common input queue.
Neither `key_status[]` nor `od_hot_key[]` is consulted on that path.

Consequently, the legacy Yankee Trader F4-through-F10 bindings cannot be
implemented portably as an OpenDoors personality/custom hotkey. This does
not require recreating them when the Win32 SysOp interface itself need not
match the DOS interface: OpenDoors' GUI already supplies chat, time
adjustment, Exit to BBS, hangup, lockout, keyboard-off, SysOp-next, and
inactivity controls. Yankee Trader only has to integrate the applicable
state changes and preserve their required player-facing consequences.

### Bytes, strings, formatting, and emulation are different

- `od_disp()` accepts an explicit byte count and can transmit embedded NULs.
- `od_disp_str()` accepts a NUL-terminated byte string and performs no
  `printf` formatting.
- `od_printf()` formats values and then interprets OpenDoors color
  delimiters.
- `od_disp_emu()` interprets terminal and optionally RA/QuickBBS control
  sequences for the local display and possibly the remote display.
- `od_send_file()` adds filename selection, terminal emulation, paging,
  pause, stop, hotkey, and substitution behavior.

These functions are not interchangeable when exact player-visible bytes or
exact input consumption matter.

`od_disp()` and `od_disp_str()` **must not be used with local echo when their
input may contain ANSI, AVATAR, or other terminal-emulator commands**. The
remote terminal may execute the commands while the Win32 local display shows
their component bytes as glyphs. This includes arbitrary user, BBS, SysOp,
configuration, or data-file text unless its permitted byte set has actually
been established.

Classify every output value at its boundary as one of:

- a trusted program literal known not to contain emulator commands;
- text whose complete byte range was explicitly validated or sanitized; or
- emulator-bearing or unsanitized data.

A field name or filename extension is not validation. In particular, player
names, messages, BBS/drop-file strings, registration strings, persisted
records shared with the original binaries, SysOp-editable files, and
nominally `.ASC` files must be treated according to their actual provenance
and allowed bytes.

For data intended to contain terminal commands, either express the recovered
formatting as OpenDoors color/cursor/screen calls, or use `od_disp_emu()` with
the global translation state set deliberately. For data intended to be
literal text, apply exactly the recovered original validation or
sanitization rule; do not invent one. Passing arbitrary text through
`od_disp_emu()` is not sanitization: it authorizes that text to manipulate
the local display, and an incomplete sequence leaves the emulator's
persistent parser state to consume bytes from later calls.

The 6.00 manual tells callers to use `"\n\r"` rather than a lone `"\n"` for
a terminal newline. OpenDoors does not normalize an arbitrary buffer passed
to `od_disp()`; the sequence supplied by the caller is the sequence sent.
Yankee Trader must therefore emit the line-ending order recovered from the
original program, not adopt the manual's example by convention.

### Graphics capability and screen clearing are independent

Color, cursor positioning, block operations, windows, and advanced editors
require ANSI, AVATAR, or RIP as documented for the individual function.
They either have no visible effect or fail with `ERR_NOGRAPHICS` when the
required mode is absent.

Screen clearing is a separate user setting. In the 6.00 manual,
`od_clr_scr()` honors it. The installed implementation also has the later
`od_always_clear` control: when true, it forces the clear. Yankee Trader's
current pre-initialization assignment of false is overwritten by
`od_init()` and therefore does not presently make the function honor the
user's setting; see the call-site audit below.

### Output may be transcoded

The installed version adds `od_control.od_cp437_to_utf8_out`, activated by
the undocumented `CP437UTF8` command-line option. When enabled, bytes
`0x80` through `0xff` sent through the communications layer are converted
from CP437 to multibyte UTF-8. Local display bytes are not the remote byte
stream after that conversion.

The preliminary note at the top of the manual calls this field
`od_cp437_unicode_out`; that name is stale. The installed public header uses
`od_cp437_to_utf8_out`.

This option must remain off whenever byte-for-byte CP437 output is required.

### Errors are sticky

Functions returning `FALSE`, `NULL`, or a function-specific error value set
`od_control.od_error`. A successful later call does not necessarily reset
it. Always test the function's return value before interpreting
`od_error`.

## Quick selection table

| Need | Select | Important reason |
|---|---|---|
| Exact counted bytes, including possible NULs, known to contain no emulator commands | `od_disp()` | No string termination, formatting, color parsing, or terminal emulation |
| Exact NUL-terminated text known to contain no emulator commands | `od_disp_str()` | Simpler than counted output, but cannot carry NUL |
| C formatted text with OpenDoors color markup | `od_printf()` | Formatting and color parsing are intentional |
| One visible byte | `od_putch()` | Echoes to both endpoints and advances the cursor |
| A repeated character | `od_repeat()` | May use AVATAR repeat optimization |
| NUL-free ANSI/AVATAR stream interpreted locally and sent remotely unchanged | `od_no_ra_codes = TRUE`, then `od_disp_emu(text, TRUE)` | Remote transmission uses the raw string while the local display emulates it |
| ANSI-music sequence | `od_disp(sequence, length, FALSE)` | OpenDoors' local emulator does not implement ANSI music; the sequence must be remote-only |
| RA/QuickBBS substitutions intentionally translated | `od_no_ra_codes = FALSE`, then `od_disp_emu()` | Emulator consumes/substitutes RA commands and can change remote output |
| Exact legacy display-file behavior without OpenDoors file features | manual read plus a purpose-built raw/emulated output path | `od_disp()` alone corrupts the Win32 local view when the file contains emulator commands |
| OpenDoors-managed ASC/ANS/AVT/RIP file | `od_send_file()` | Chooses a compatible file and supplies paging/control behavior |
| One ordinary key from player or SysOp | `od_get_key()` | Merged queue, optional indefinite wait |
| Timed input, origin, arrows, or function keys | `od_get_input()` | Event structure, timeout, translation |
| Test without consuming | `od_key_pending()` | Boolean queue test only |
| One key restricted to a set | `od_get_answer()` | Case-insensitive validation and returned-case normalization |
| Simple echoed line input | `od_input_str()` | ASCII-safe end editing; always owns echo and final newline |
| Formatted full-screen field | `od_edit_str()` | Rich editing and validation, but requires graphics |
| Multi-line editor | `od_multiline_edit()` | Word wrap, navigation, callbacks, optional reallocation |
| DOS local-only SysOp action | custom SysOp hotkey | DOS consumes built-in/custom hotkeys before player input |
| Win32 local extended key | `od_get_input()` with origin checking | Personalities and custom SysOp hotkeys are unsupported on Win32 |
| Observe DOS local typing without consuming it | `od_local_input` callback | DOS callback is notified, then the key is still queued; it is not called by Win32 |
| Pump DOS-era asynchronous door work | `od_kernel()` | Carrier, timeout, status, SysOp keys, and received input |

## Text and byte display

### `od_disp()`

```c
void od_disp(const char *buffer, INT size, BOOL local_echo);
```

- Sends exactly `size` bytes to the remote communications channel in remote
  mode. Embedded NUL bytes are permitted.
- With `local_echo == TRUE`, also displays the same counted buffer locally.
  With `FALSE`, it is remote-only.
- It does not parse `printf` fields, OpenDoors color delimiters, or terminal
  sequences.
- On Win32, the local-echo path writes raw cells to OpenDoors' screen buffer.
  ESC is an ordinary OEM glyph, not the start of ANSI. Only CR, LF,
  backspace, tab, and bell receive special handling.
- It is the least transforming public output API, subject to the installed
  communications layer's optional CP437-to-UTF-8 conversion.
- Choose it with local echo only when the counted data is established not to
  contain terminal-emulator commands. For a raw remote-only stream,
  `local_echo == FALSE` avoids corrupting the local view, but provides no
  local mirror.

### `od_disp_str()`

```c
void od_disp_str(const char *text);
```

- Displays a NUL-terminated string both remotely and locally.
- Stops at the first NUL and cannot be used for a counted binary buffer.
- Does not perform `printf` formatting or OpenDoors color-markup parsing.
- Does not provide a remote-only or local-only switch.
- Its Win32 local behavior has the same raw-control limitation as
  `od_disp()`. Choose it only for trusted or validated strings known not to
  contain emulator commands.

### `od_printf()`

```c
void od_printf(const char *format, ...);
```

- Performs normal C `printf`-style formatting and sends the result both
  remotely and locally.
- After formatting, parses color descriptions delimited by
  `od_color_delimiter`, backquote by default. A delimiter may therefore come
  from a `%s` argument and still be interpreted.
- `od_color_delimiter = 0` disables descriptive color parsing. The installed
  version also retains the older single-byte `od_color_char` mechanism.
- Color codes select ANSI, AVATAR, or RIP output as available and disappear
  in plain ASCII mode.
- The manual limits the fully expanded text, including color sequences, to
  511 characters.
- Do not pass player-controlled text as the format string. Do not choose this
  function for exact legacy bytes that may contain percent signs or color
  delimiters.

### `od_putch()`

```c
void od_putch(char value);
```

- Displays one byte remotely and locally and advances the cursor.
- Uses the current color in graphics modes.
- It does not automatically echo a key merely because the key came from an
  input function; the caller chooses whether to call `od_putch()`.
- Choose it when reproducing the original program's explicit per-key echo.

### `od_repeat()`

```c
void od_repeat(char value, BYTE count);
```

- Displays one byte `count` times both remotely and locally.
- Uses an efficient AVATAR repeat sequence when possible; otherwise sends
  repeated characters.
- `count` is a `BYTE`, so this is not the interface for runs longer than 255
  characters without chunking.
- Because the remote representation may be an optimized control sequence,
  do not choose it when the literal transmitted bytes themselves must match a
  captured legacy stream.

### `od_disp_emu()`

```c
void od_disp_emu(const char *text, BOOL remote_echo);
```

- Always feeds the NUL-terminated string through OpenDoors' local terminal
  emulator.
- With `remote_echo == TRUE`, also sends the corresponding stream remotely;
  with `FALSE`, it is local-only.
- Interprets ANSI and AVATAR control sequences locally. It also translates
  RA/QuickBBS substitution codes unless `od_no_ra_codes` is true.
- With `remote_echo == TRUE` and `od_no_ra_codes == TRUE`, the installed
  implementation first sends the NUL-terminated string remotely through
  `od_disp(..., FALSE)` unchanged, then emulates it locally without remote
  echo. This is the relevant mode for a NUL-free legacy ANSI/AVATAR stream
  whose remote bytes must remain unchanged.
- With `od_no_ra_codes == FALSE`, remote output is generated while the
  emulator processes the input. RA/QuickBBS commands may be consumed and
  replaced, so the remote byte stream is not necessarily the input string.
- ANSI and AVATAR parser state is static and persists across calls. This
  permits a valid sequence to cross call boundaries, but an incomplete or
  hostile sequence can also affect later, otherwise trusted output.
- It cannot process an embedded NUL because both `strlen()` and the emulator
  loop use NUL termination.
- `od_emu_simulate_modem` can throttle local emulation to approximate the
  remote connect speed; local mode assumes 9600 bps.
- Choose it when terminal controls are intended and the relevant global
  settings have been fixed before the call. Do not use it as a substitute
  for validating text that is supposed to be literal.

### ANSI music is remote-only

OpenDoors does not implement ANSI music in its local terminal emulator and
does not expose a portable local ANSI-music player. Its supplied
`ex_music.c` demonstrates the intended transport explicitly: send `ESC [`
and the PLAY-language bytes with `od_disp(..., FALSE)`, “to the remote system
only.” The manual likewise describes the example's helper as sending ANSI
music to the remote system.

Passing ANSI music through either local-capable path corrupts the Win32
operator display:

- `od_disp(..., TRUE)` treats every byte except CR, LF, backspace, tab, and
  BEL as a raw screen cell, so it renders the escape and PLAY command bytes;
- `od_disp_emu()` recognizes the `M` in `ESC [ M...` as ANSI `DL` (delete
  line), which its emulator marks unsupported and consumes, then renders the
  remaining PLAY-language bytes as ordinary text; and
- Yankee Trader's final `0x0e` is SO (Shift Out), not SI (Shift In, `0x0f`).
  It is not a terminator understood by the local emulator and reaches the
  ordinary character-cell path as a CP437 glyph.

Therefore every legacy ANSI-music emission must use an exact counted,
remote-only call:

```c
od_disp(sequence, length, FALSE);
```

If music is adjacent to ordinary ANSI screen output, split it from that
output. Send the display portion through the selected semantic or emulated
screen path and the complete music portion through the remote-only path. Do
not feed even the `ESC [ M` prefix to the local emulator, because that would
silently invoke the unsupported `DL` case.

Remote-only routing protects the OpenDoors operator display; it does not
make the bytes safe for an arbitrary remote ANSI/VT terminal. A terminal
without the ANSI-music extension can apply the standard meanings instead:

- `CSI M` is `DL` and can delete a line;
- the PLAY-language tail is displayed as ordinary text; and
- SO (`0x0e`) invokes G1 into GL and can leave subsequent output in the
  alternate character set until SI (`0x0f`) or another reset restores it.

This is persistent terminal-state corruption, not merely an unwanted music
note or one bad glyph. OpenDoors' `user_ansi` flag establishes ANSI display
support, not ANSI-music support. Its own example tests music capability
separately by sending a sample and asking the user whether it was heard.
Consequently `user_ansi` alone cannot establish that transmitting the bytes
is safe. This observation does not replace Yankee Trader's recovered sound
gates with an invented capability policy; it records the compatibility
hazard that any explicitly chosen policy must address.

The proposed positive detection of CTerm and preference for its non-conflicting
`CSI |` music introducer are specified separately in
`TERMINAL-COMPATIBILITY.md`. OpenDoors does not perform that detection.

This is an endpoint-routing rule, not permission to change Yankee Trader's
recovered sound gates or bytes. The legacy dispatcher controls separately
whether to emit remote ANSI music, remote BEL, and local BASIC `PLAY`.
OpenDoors' remote-only call covers only the first endpoint. Reproducing the
legacy local-speaker endpoint, where required, needs a separate
platform-specific or portability-layer implementation; it must not be
implemented by locally echoing the terminal sequence.

The selected Yankee Trader portability policy is now explicit: every reached
local BASIC `PLAY` remains an ordered logical presentation event, but the
OpenDoors physical adapter deliberately performs no host audio operation.
This is an authorized compatibility departure. It does not change the PLAY
gate, command bytes, parser-facing logical identity, event order, or any
remote ANSI-music/BEL output.

## Color, cursor, and clearing

### `od_set_attrib()`

```c
void od_set_attrib(INT attribute);
```

- Sets the current IBM-PC text attribute for subsequent output.
- The low nibble is foreground; the high nibble is background plus the
  blink bit.
- Emits an appropriate graphics-mode sequence and updates the local display.
  It has no visual color effect without ANSI, AVATAR, or RIP.
- `od_full_color` can force emission even when OpenDoors believes the
  requested attribute is already current.

### `od_set_color()`

```c
void od_set_color(INT foreground, INT background);
```

- Sets the same state as `od_set_attrib()`, but takes separate OpenDoors color
  constants.
- Foreground accepts dark and bright values. Background accepts normal or
  blinking values.
- Has no visual color effect in plain ASCII mode.
- Choose between this and `od_set_attrib()` according to the recovered
  representation of the color; they are alternative interfaces to the same
  output state.

### `od_set_cursor()`

```c
void od_set_cursor(INT row, INT column);
```

- Positions the cursor using one-based coordinates.
- The 6.00 manual limits the output area to rows 1 through 23 and columns 1
  through 80.
- Requires ANSI, AVATAR, or RIP. The installed implementation records
  `ERR_NOGRAPHICS` when unavailable.
- It updates both the remote terminal and the local screen model.

### `od_get_cursor()` — later installed API

```c
void od_get_cursor(INT *row, INT *column);
```

- Returns OpenDoors' **best estimate**, obtained from its local screen model,
  of the current remote cursor position.
- Either output pointer may be `NULL`, but not both; both `NULL` sets
  `ERR_PARAMETER`.
- It does not query the remote terminal and cannot correct divergence caused
  by output that bypassed OpenDoors or by terminal behavior OpenDoors did not
  model.

### `od_clr_line()`

```c
void od_clr_line(void);
```

- Clears from the current cursor cell through the end of the line, then
  returns the cursor to its original position.
- In ASCII mode it emits spaces and backspaces. In graphics modes it uses the
  applicable terminal sequence.
- The cleared cells use the current attribute in graphics modes.

### `od_clr_scr()`

```c
void od_clr_scr(void);
```

- Clears the remote screen and local output window, excluding the local
  status-line area.
- Screen-clearing permission is separate from ANSI/AVATAR/RIP capability.
- Per the manual, it does nothing when the user has screen clearing disabled.
  In the installed version, `od_always_clear` overrides that check.
- The manual's force-clear recipe is `od_disp_emu("\x0c", TRUE)`. That is a
  materially different choice because it bypasses the user's clearing
  preference.

## Screen blocks and windows

### `od_gettext()`

```c
BOOL od_gettext(INT left, INT top, INT right, INT bottom, void *block);
```

- Copies a rectangular region from OpenDoors' screen model into caller
  memory; it does not ask the remote terminal to return its screen.
- Coordinates are one-based, columns 1 through 80 and rows 1 through 23.
- Storage is two bytes per cell: character followed by IBM-PC attribute.
- Preserves current cursor position and color.
- Requires ANSI, AVATAR, or RIP and returns `FALSE` with `od_error` on
  failure.

### `od_puttext()`

```c
BOOL od_puttext(INT left, INT top, INT right, INT bottom, void *block);
```

- Paints a character/attribute block in the format produced by
  `od_gettext()`. A caller may construct that format directly.
- Destination dimensions must equal the saved block dimensions.
- By default it sends only cells that differ from OpenDoors' current screen
  model. `od_full_put = TRUE` forces every cell to be emitted.
- Preserves cursor position and color.
- Requires graphics mode. The manual text says ANSI or AVATAR in one place
  and ANSI/AVATAR/RIP in the broader block-function descriptions; callers
  should test the return value rather than assume success from a mode flag.

### `od_save_screen()`

```c
BOOL od_save_screen(void *buffer);
```

- Saves the complete text screen, current cursor, and current color.
- Requires a caller buffer of at least 4004 bytes.
- Works in ASCII, ANSI, AVATAR, and RIP modes, but saves only text state in
  RIP mode.
- Its private format is not compatible with the two-byte-cell
  `od_gettext()` format.

### `od_restore_screen()`

```c
BOOL od_restore_screen(void *buffer);
```

- Restores a buffer produced by `od_save_screen()`, including cursor and
  color.
- Works in all display modes; RIP bitmap graphics are not restored.
- Must not be passed an `od_gettext()` buffer.

### `od_scroll()`

```c
BOOL od_scroll(INT left, INT top, INT right, INT bottom,
    INT distance, WORD flags);
```

- Scrolls a rectangular screen area. Positive distance moves text upward;
  negative distance moves it downward.
- New lines normally use the current color.
- `SCROLL_NO_CLEAR` permits new lines to remain uncleared when clearing
  would be slower.
- Preserves the prior cursor and color. Requires graphics mode.
- AVATAR has a compact native sequence. ANSI may internally save, restore,
  and clear blocks; full-screen-width scrolling is faster.

### `od_draw_box()`

```c
BOOL od_draw_box(BYTE left, BYTE top, BYTE right, BYTE bottom);
```

- Draws a border at one-based coordinates using the current attribute.
- Uses `od_box_chars[]`; the default is an IBM extended-character single-line
  border.
- Requires graphics mode and can use AVATAR-specific acceleration.
- It does not save or clear what is inside or underneath the box.

### `od_window_create()`

```c
void *od_window_create(INT left, INT top, INT right, INT bottom,
    char *title, BYTE border_color, BYTE title_color,
    BYTE inside_color, INT reserved);
```

- Saves the underlying screen region, draws and clears a popup window, and
  optionally draws a title.
- Requires graphics mode. `reserved` must be zero.
- Returns an allocated opaque handle or `NULL` with `od_error`.
- The returned handle must eventually be passed to `od_window_remove()`.

### `od_window_remove()`

```c
BOOL od_window_remove(void *window);
```

- Restores the region saved by `od_window_create()` and frees the window
  allocation.
- Overlapping windows must be removed in reverse creation order.
- Returns `FALSE` and sets `od_error` on failure.

## File and menu display

### `od_send_file()`

```c
BOOL od_send_file(const char *filename);
```

- Displays an ASCII, ANSI, AVATAR, or RIP file remotely and locally.
- With an explicit extension, opens that file. Without an extension, chooses
  among `.RIP`, `.AVT`, `.ANS`, and `.ASC` according to terminal capability
  and available fallbacks.
- ANSI/AVATAR sequences are interpreted for the local screen regardless of
  the file's extension.
- In RIP mode, OpenDoors seeks a separate AVT/ANS/ASC local representation.
  If none exists, it shows a local message naming the RIP file. RIP display
  disables page pausing.
- RA/QuickBBS `^F` and `^K` controls may substitute user/system values.
  `^KX` disconnects the user. Set `od_no_ra_codes` to disable this entire
  translation.
- Page pausing is controlled by `od_page_pausing` and
  `user_screen_length` (default 23 when unavailable).
- If enabled, `P` pauses and any next key resumes; `S`, Ctrl-K, or Ctrl-C
  stops. `od_list_pause` and `od_list_stop` independently disable those
  controls.
- Those controls consume input and can clear queued/outbound data. This is
  not a passive file copy.
- `od_emu_simulate_modem` can alter local presentation timing.
- Returns `FALSE`, normally with `ERR_FILEOPEN` or `ERR_FILEREAD`, when it
  cannot complete the file operation.

### `od_send_file_section()` — later installed API

```c
BOOL od_send_file_section(char *filename, char *section_name);
```

- Behaves like `od_send_file()` but displays only a named section.
- A section begins on a line starting with `@#` followed by
  `section_name`. The header line is not displayed; output stops at the next
  `@#` header or end of file.
- `section_name` must not include the `@#` prefix.
- It retains `od_send_file()`'s extension selection, emulation, pausing,
  stop-key, and RIP-local-fallback behavior.
- Returns `FALSE` if either pointer is `NULL`, the file cannot be used, or
  the named section is not found.

### `od_list_files()`

```c
BOOL od_list_files(char *file_spec);
```

- Reads a `FILES.BBS` listing and displays filenames, sizes, descriptions,
  title lines, wildcard matches, and `[OFFLINE]` for missing files.
- Accepts a directory/file specification and permits paths and wildcards in
  listing entries.
- Uses `od_list_title_col`, `od_list_name_col`, `od_list_size_col`,
  `od_list_comment_col`, and `od_list_offline_col`.
- Uses the same page-pausing and pause/stop input controls as file display.
- Returns success/failure rather than a selected filename; it is a listing
  UI, not a file-transfer function.

### `od_hotkey_menu()`

```c
char od_hotkey_menu(char *filename, char *hotkeys, BOOL wait);
```

- Displays a file using `od_send_file()` behavior while accepting a valid
  menu key at any time.
- A valid key stops display immediately. Matching is case-insensitive, but
  the returned character has the same case as its entry in `hotkeys`.
- The manual says Enter can be enabled with `\n`. The installed
  `od_get_key()` discards LF, however, so an installed-version hotkey list
  should contain `\r` (or both `\r` and `\n`) when Enter must be accepted.
- With `wait == TRUE`, it waits after display until a valid key is entered.
  With `FALSE`, it returns NUL if no valid key was pressed during display.
- It inherits file selection, terminal emulation, substitutions, paging,
  pause/stop controls, and their input consumption.

### `od_popup_menu()`

```c
INT od_popup_menu(char *title, char *text, INT left, INT top,
    INT level, WORD flags);
```

- Creates a graphics-mode popup menu whose items are separated by `|`.
  A `^` before a character designates that item's case-insensitive hotkey.
- The user may choose with a hotkey or a highlighted bar controlled by
  translated arrow keys.
- `MENU_ALLOW_CANCEL` permits Escape and returns `POPUP_ESCAPE`.
- `MENU_PULLDOWN` permits left/right exit and returns `POPUP_LEFT` or
  `POPUP_RIGHT`.
- `MENU_KEEP` leaves a selected menu on screen. Re-enter it using the same
  level, or destroy it with `MENU_DESTROY` and that level.
- Active overlapping levels must be managed like stacked windows.
- Returns a positive one-based item number, a special negative/zero result,
  or `POPUP_ERROR` with `od_error`.
- Requires ANSI, AVATAR, or RIP and uses the six `od_menu_*_col` settings.

## Keyboard and editor input

### `od_clear_keybuffer()`

```c
void od_clear_keybuffer(void);
```

- Discards type-ahead from both remote and local sources.
- The 6.00 manual describes a 64-key common queue. The installed version
  instead uses `od_in_buf_size`, default 256 events.
- The installed implementation pumps the kernel, empties the common event
  queue, and in remote mode also clears the communications inbound buffer.
- Use only at a documented input boundary. It can discard intentional macro
  commands as well as accidental type-ahead.

### `od_key_pending()` — later installed API

```c
BOOL od_key_pending(void);
```

- Pumps the kernel as needed and returns whether the common input queue is
  nonempty.
- Does not consume the event and reports neither origin nor event type.
- It is clearer than using `od_get_key(FALSE)` when the caller only needs a
  readiness test.

### `od_get_key()`

```c
char od_get_key(BOOL wait);
```

- Returns the next character from the merged local/remote input queue.
- With `wait == TRUE`, blocks until a character is available and yields to
  other tasks while waiting. With `FALSE`, returns zero immediately when no
  character is queued.
- Zero is therefore a sentinel and not a safely distinguishable input
  character.
- Updates `od_last_input` to identify the source of the returned character.
- It is not the interface for translated arrows, Insert/Delete, function
  keys, or robust DoorWay sequences; use `od_get_input()` for those.
- In the installed 6.20-and-later implementation, line-feed characters are
  silently skipped. A CR/LF terminal line therefore normally yields CR, not
  two line-ending events.
- Built-in and custom local SysOp hotkeys are handled by the kernel rather
  than returned here.

### `od_get_input()`

```c
BOOL od_get_input(tODInputEvent *event, tODMilliSec wait_ms, WORD flags);
```

- Returns an event with:

  - `EventType`: `EVENT_CHARACTER` or `EVENT_EXTENDED_KEY`;
  - `bFromRemote`: true for remote, false for local; and
  - `chKeyPress`: the byte or an `OD_KEY_*` extended-key value.

- `wait_ms == 0` polls, `OD_NO_TIMEOUT` waits indefinitely, and any other
  value is a millisecond deadline. DOS rounds timing to roughly 55 ms.
- `GETIN_NORMAL` recognizes ANSI and DoorWay extended-key sequences.
- `GETIN_RAW` disables extended-sequence translation and returns component
  bytes.
- `GETIN_RAWCTRL` disables the control-key aliases while retaining other
  extended-key recognition.
- Recognized extended values include F1-F10, arrows, Insert, Delete, Home,
  End, Page Up, Page Down, and Shift-Tab.
- Control aliases in normal mode are Ctrl-E/Ctrl-X for up/down, Ctrl-S/Ctrl-D
  for left/right, Ctrl-V for Insert, and Ctrl-G for Delete.
- This is the correct primitive when remote and local input must have
  different meanings. It is used internally by the popup, field, and
  multiline editors.

### `od_get_answer()`

```c
char od_get_answer(const char *options);
```

- Waits until `od_get_key(TRUE)` returns one of the listed choices.
- Alphabetic matching is case-insensitive.
- Returns the choice in the same case used in `options`, not necessarily the
  case typed.
- Does not automatically echo the accepted key.
- Spaces and control characters may be members of `options`; the manual
  shows CR/LF and Enter choices. In the installed version LF is discarded by
  `od_get_key()`, so include CR when Enter must be accepted.

### `od_input_str()`

```c
void od_input_str(char *input, INT maximum,
    unsigned char minimum, unsigned char maximum_character);
```

- Starts with an empty result and accepts up to `maximum` characters in the
  inclusive byte range.
- Echoes each accepted byte with `od_putch()`.
- Supports only end-of-string correction: Backspace erases the final
  character. It is not an insert/mid-line editor.
- Waits for Enter, NUL-terminates the result, and emits its own newline.
- The destination must have room for `maximum + 1` bytes.
- Even a one-character field is returned as a C string.
- It has no cancel result, no timeout, no source distinction, no no-echo
  password mode, and no way to suppress its echo/newline. A custom
  `od_get_key()` loop is required when exact legacy editing behavior differs.

### `od_edit_str()`

```c
WORD od_edit_str(char *input, char *format, INT row, INT column,
    BYTE normal_color, BYTE highlight_color, char blank, WORD flags);
```

- Provides insert/overwrite editing, cursor movement, Home/End, Delete,
  Backspace, Ctrl-Y line erase, validation, literal insertion, field
  navigation, cancellation, and optional password masking.
- Requires ANSI, AVATAR, or RIP. It returns `EDIT_RETURN_ERROR` if graphics
  are unavailable or parameters are invalid.
- Returns `EDIT_RETURN_ACCEPT`, `EDIT_RETURN_CANCEL`,
  `EDIT_RETURN_PREVIOUS`, or `EDIT_RETURN_NEXT` for normal outcomes.
- Enter/Ctrl-Z accepts only valid input. Escape cancels only with
  `EDIT_FLAG_ALLOW_CANCEL`; cancellation restores the original string.
- Previous/next field controls are available with
  `EDIT_FLAG_FIELD_MODE`. The local up/down arrows normally remain reserved
  for SysOp time adjustment, so Tab and Shift-Tab are the reliable local
  alternatives unless key assignments are changed.
- Format characters are:

| Code | Accepted or generated content |
|---|---|
| `#` | digit |
| `%` | digit or space |
| `9` | digit, `.`, `-`, or `+` |
| `?` | any byte |
| `*` | printable ASCII 32 through 127 |
| `A` | letter or space |
| `C` | title-cased city characters: letter, space, comma, period |
| `D` | digit, `-`, or `/` |
| `F` | DOS filename character |
| `H` | hexadecimal digit |
| `L` | lower-case letter or space; converts upper case |
| `M` | title-cased name character |
| `T` | telephone digit, parentheses, hyphen, or space |
| `U` | upper-case letter or space; converts lower case |
| `W` | DOS filename character plus `*` and `?` |
| `X` | alphanumeric or space |
| `Y` | Y/N, normalized to upper case |
| `'...'` or `"..."` | automatically supplied literal text |

- Field placement is one-based. The input allocation must hold every
  produced character, including literals and the terminating NUL.
- `blank` fills unused field cells; in password mode it is the masking
  character.
- Available flags and their selection consequences are:

| Flag | Effect |
|---|---|
| `EDIT_FLAG_NO_REDRAW` | Caller has already drawn the field; suppress entry/exit redraw |
| `EDIT_FLAG_FIELD_MODE` | Enable previous/next field return paths |
| `EDIT_FLAG_EDIT_STRING` | Edit initialized existing text instead of starting blank |
| `EDIT_FLAG_STRICT_INPUT` | Prevent edits that temporarily violate mixed-position formats |
| `EDIT_FLAG_PASSWORD_MODE` | Display `blank` rather than entered characters |
| `EDIT_FLAG_ALLOW_CANCEL` | Escape restores the original value and cancels |
| `EDIT_FLAG_FILL_STRING` | Require every nonliteral format position |
| `EDIT_FLAG_AUTO_ENTER` | Accept automatically when full and valid |
| `EDIT_FLAG_AUTO_DELETE` | With edit mode, first non-edit key replaces existing text |
| `EDIT_FLAG_KEEP_BLANK` | Leave unused-field background visible after exit |
| `EDIT_FLAG_PERMALITERAL` | Display and retain literals from the outset |
| `EDIT_FLAG_LEAVE_BLANK` | Return empty when only leading literals would remain |
| `EDIT_FLAG_SHOW_SIZE` | Do not draw the usual extra cursor cell |

### `od_multiline_edit()`

```c
INT od_multiline_edit(char *buffer, UINT buffer_size,
    tODEditOptions *options);
```

- Provides a graphics-mode multi-line editor with cursor navigation,
  Home/End/Page Up/Page Down, insert/overwrite, Ctrl-Y line deletion, and
  optional word wrap.
- `buffer` must already be NUL-terminated. `buffer_size` is capacity, not
  current text length.
- A `NULL` options pointer uses defaults. Otherwise the caller must zero the
  entire structure before overriding members.
- The default area is rows 1 through 23 and columns 1 through 80.
- Text formats are:

  - `FORMAT_PARAGRAPH_BREAKS`: breaks only between paragraphs; wraps display;
  - `FORMAT_LINE_BREAKS`: stores each displayed line break and wraps input;
  - `FORMAT_NO_WORDWRAP`: line-break storage without new-input wrapping; and
  - `FORMAT_FTSC_MESSAGE`: CR paragraphs and ignored LF for FTSC messages.

- For the first three formats, existing contents determine whether new line
  breaks use CR, LF, or a pair. With no existing break, the default is LF.
- Escape or Ctrl-Z calls `pfMenuCallback` when provided. Its return either
  continues or exits; without a callback those keys exit the editor.
- `pfBufferRealloc` may grow a dynamically allocated buffer. Without it, the
  editor refuses text that would exceed the supplied capacity.
- After possible reallocation, use `pszFinalBuffer` and
  `unFinalBufferSize` from the options structure; the original pointer may no
  longer be current.
- Returns `OD_MULTIEDIT_SUCCESS` or `OD_MULTIEDIT_ERROR`, with `od_error`
  describing an error.

## Interactive facilities whose names do not look like simple I/O

### `od_autodetect()`

```c
void od_autodetect(INT flags);
```

- Exchanges terminal-detection traffic with the remote side and consumes the
  responses.
- Sets `user_ansi` and/or `user_rip` true when detected.
- Never clears a capability that was already true, because failure to respond
  does not prove absence.
- Cannot detect AVATAR.
- The manual requires `DETECT_NORMAL`; the flag is reserved for future use.
- Do not call it in a byte-exact session unless the recovered program is
  documented to perform that negotiation.

### `od_chat()`

```c
void od_chat(void);
```

- Enters OpenDoors' blocking, line-oriented SysOp chat facility and returns
  when chat ends.
- Merges local and remote typing, displays it to both endpoints, changes
  colors according to origin, performs word wrapping, and freezes
  OpenDoors' own user-time accounting.
- It does not know about an application-owned absolute deadline. A door such
  as Yankee Trader that maintains a separate deadline must synchronize it
  through the chat hooks or otherwise establish OpenDoors as the authoritative
  time source.
- Local Escape exits chat; a remote Escape does not.
- Clears `user_wantchat`, logs chat events when enabled, and runs
  `od_cbefore_chat`/`od_cafter_chat`.
- A before-chat callback can replace built-in chat by running its own UI and
  setting `od_chat_active` false before returning.
- The configurable before/after strings may also change player-visible
  output.

### `od_page()`

```c
void od_page(void);
```

- Runs a complete user-facing paging interaction rather than merely sounding
  a beep.
- Clears the screen, asks for a reason, and treats a blank reason as abort.
- Enforces `od_okaytopage` and configured paging hours.
- On a valid page, sets want-chat state, changes the local status line,
  increments `user_numpages`, logs the event if enabled, and emits the timed
  page alarm while continuing to service the kernel.
- Displays unavailable/no-response text and wait prompts according to the
  customizable strings.
- Choose it only when that whole transcript and state transition match the
  original door.

### `od_kernel()`

```c
void od_kernel(void);
```

- In the DOS design, receives remote bytes into the common queue, handles
  local SysOp keys, carrier loss, user time, inactivity, warnings, callbacks,
  and status-line updates.
- The manual requires an OpenDoors call at least about once per second during
  long processing and recommends explicit `od_kernel()` calls when no other
  OpenDoors API will run.
- The manual says it has no effect in the Win32 build because background
  threads perform those tasks. Later Unix-like ports may also use threaded
  internals; calling it at documented safe points remains the portable API
  contract.
- It can call application callbacks and can terminate the process. It must
  not be placed inside a data-file mutation that cannot safely be
  interrupted.
- `od_ker_exec`, when non-NULL, is called whenever the kernel executes.

### `od_set_statusline()`

```c
void od_set_statusline(INT setting);
```

- Selects or hides the current **local SysOp** status line. It does not send a
  player menu.
- This API is DOS/text-mode only in the installed source. On Win32 and
  Unix-like builds it sets `od_error = ERR_UNSUPPORTED` and makes no display
  change; Win32 uses its separate GUI status bar.
- `STATUS_NORMAL`, `STATUS_USER1` through `STATUS_USER4`,
  `STATUS_SYSTEM`, `STATUS_HELP`, and `STATUS_NONE` correspond to the
  personality's status views.
- The change is temporary: while `od_status_on` remains true, the SysOp can
  select another status line with local function keys.
- `STATUS_NONE` hides the line but leaves the subsystem enabled.
  `od_status_on = FALSE` disables updates and prevents function keys from
  restoring it.
- Changing output-window height as status visibility changes can alter local
  cursor and scrolling behavior even though it is not remote output.

### `od_add_personality()` and `od_set_personality()`

```c
BOOL od_add_personality(const char *name, BYTE output_top,
    BYTE output_bottom, OD_PERSONALITY_PROC *callback);
BOOL od_set_personality(const char *name);
```

- These control the local status-line layout and the SysOp key map, so they
  are part of local I/O selection even though they do not print player text.
- `od_add_personality()` must be called before initialization. It registers
  the local output-window bounds and callback.
- It only affects the DOS/text-mode personality system. The manual explicitly
  says `od_add_personality()` only has an effect under DOS, and the installed
  source defines `OD_TEXTMODE` only for `ODPLAT_DOS`.
- On Win32 and Unix-like builds, `od_add_personality()` and
  `od_set_personality()` return `FALSE` and set `od_error` to
  `ERR_UNSUPPORTED`.
- On those builds, `od_mps` and `od_default_personality` are not used during
  initialization, and no `PEROP_*` callback is installed.
- `od_set_personality()` requires the MPS component to be enabled before
  initialization and selects a registered or built-in case-insensitive name.
- Personality callbacks receive initialization/deinitialization,
  status-display/update, and custom-key operations.

## Communications state and termination

### `od_init()`

```c
void od_init(void);
```

- Reads the configuration and door-information inputs, initializes local and
  remote communications, establishes internal queues/screen state, and
  displays the local status interface.
- May display copyright/startup output unless `od_nocopyright` is true.
- Forced automatic local mode may prompt for a user name unless
  `DIS_NAME_PROMPT` is set.
- Can change the current working directory through the configuration-file
  `DoorDir` setting and restore it on exit.
- Must be called explicitly before reading initialized `od_control` fields.
- May call `exit()` on initialization failure; cleanup hooks must already be
  installed.

### `od_carrier()`

```c
BOOL od_carrier(void);
```

- Reads the current carrier-detect state.
- Returns false both for a lost carrier and for local mode; callers must use
  known mode state to distinguish them.
- The manual primarily intends it for programs that set
  `DIS_CARRIERDETECT` and take responsibility for carrier handling.
- With automatic carrier handling enabled, OpenDoors may terminate the
  process when loss is serviced rather than allow application-level recovery.

### `od_set_dtr()`

```c
void od_set_dtr(BOOL high);
```

- Raises or lowers the modem DTR signal. Lowering it normally disconnects the
  remote caller.
- It is not valid in local mode and can report `ERR_NOREMOTE`.
- A program that deliberately lowers DTR and intends to keep running must
  disable automatic carrier-loss exit first, wait for carrier loss, then
  raise DTR again.
- Not every modem is configured to hang up on DTR loss.

### `od_exit()`

```c
void od_exit(INT errorlevel, BOOL terminate_call);
```

- Runs `od_before_exit`, writes changed drop-file state unless disabled,
  closes OpenDoors communications, clears the screen according to exit
  settings, and terminates the process with `errorlevel`.
- With `terminate_call == TRUE`, lowers DTR to log the caller off before
  exiting. `FALSE` returns the still-connected caller to the BBS.
- OpenDoors may invoke the same shutdown path itself for carrier loss,
  timeout, inactivity, or local SysOp commands.
- `od_noexit = TRUE` performs OpenDoors shutdown without the final process
  exit; the application then needs its own reliable way to detect why
  shutdown occurred.
- Direct C `exit()` or returning from `main()` does not perform this complete
  door shutdown. Conversely, `od_exit()` normally does not return.

## Local screen and child-process I/O

### `od_spawn()`

```c
BOOL od_spawn(const char *command_line);
```

- Saves the OpenDoors screen, current drive, and directory; runs a child
  command; then restores them.
- May swap the door to EMS or disk in DOS according to swapping controls.
- The user's time normally continues to decrease. `od_spawn_freeze_time`
  freezes it.
- Returns only success/failure, not the child's exit status.
- A child transfer utility may communicate through the same modem, but
  `od_spawn()` itself does not provide a transfer protocol.

### `od_spawnvpe()`

```c
INT16 od_spawnvpe(INT16 mode, char *path,
    const char *const argv[], const char *const envp[]);
```

- Has the same OpenDoors screen/save/swap behavior as `od_spawn()`.
- Adds path searching, explicit argument/environment vectors, and the child
  exit status.
- The manual expects `P_WAIT`; returns `-1` on failure.

### `od_log_write()`

```c
BOOL od_log_write(const char *message);
```

- Writes a SysOp log-file entry, not player or local-screen output.
- Lazily opens the log if needed and adds time and line formatting
  automatically.
- The message should contain no terminal control characters or line-ending
  controls. The manual recommends no more than 67 characters.
- Requires the logfile component and is suppressed by
  `od_logfile_disable`.

## Command-line input

### `od_parse_cmd_line()`

```c
#ifdef ODPLAT_WIN32
void od_parse_cmd_line(LPSTR command_line);
#else
void od_parse_cmd_line(INT argc, char *argv[]);
#endif
```

- Consumes process command-line input and populates OpenDoors startup state.
- Must be called before `od_init()` or any implicitly initializing function,
  apart from an optional preceding `od_add_personality()`.
- DOS/Unix-like builds take the normal `argc`/`argv`; Win32 takes the raw
  `WinMain` command-line string. Substituting one form for the other is not
  portable.
- Handles communications, node, drop/config path, local, silent, user, help,
  socket/handle, and other installed options. Unknown options can be routed
  through `od_cmd_line_flag_handler` or `od_cmd_line_handler`.
- The installed `CP437UTF8` option materially changes remote output bytes.
- Win32 programs should also pass `nCmdShow` through `od_cmd_show` as
  described by the manual; that is separate from parsing the text.

### `od_split_cmd_line()` and `od_free_split_cmd_line()` — later installed APIs

```c
char **od_split_cmd_line(const char *command_line, INT *argc_out);
void od_free_split_cmd_line(char **argv);
```

- Splits a raw command line into an allocated `argv`-style array without
  initializing OpenDoors.
- The installed splitter separates on whitespace; it is not documented as a
  Windows `CommandLineToArgvW`-compatible quote/backslash parser.
- Returns `NULL` with `ERR_PARAMETER` or `ERR_MEMORY` on failure.
- The result must be released only with `od_free_split_cmd_line()`.
- The top-of-manual name `od_split_cmd_line_free()` is stale and does not
  match the installed public header.

## Deliberate non-selection: `od_sleep()`

```c
void od_sleep(tODMilliSec milliseconds);
```

`od_sleep()` yields or delays; it is not an input function and, in the
installed implementation, does not itself call `od_kernel()` while sleeping.
It does implicitly initialize OpenDoors if needed.

Use it to avoid a busy loop, but pair it with `od_get_key(FALSE)`,
`od_get_input()`, `od_key_pending()`, or an explicit `od_kernel()` at the
required cadence. A loop containing only `od_sleep()` is not a portable I/O
pump.

## Yankee Trader call-site audit

The current implementation uses only a deliberately small subset:

| Current use | Assessment from the documented semantics |
|---|---|
| `od_parse_cmd_line()` before `od_init()` | Correct ordering and correct platform-specific signature |
| cleanup registered with both `atexit()` and `od_before_exit` before initialization | Required because parsing/initialization and later kernel activity can call `exit()` |
| `od_disp(buffer, length, TRUE)` in the general `yt_out*` path | Correct only for values established not to contain terminal-emulator commands. It sends intended commands correctly to the player but renders them as raw glyphs on Win32 |
| `yt_out_file("YTOPEN.ANS")` followed by `od_disp(..., TRUE)` | Incorrect for the Win32 local display. The shipped file begins with ANSI commands, including `ESC [ 40 m`, `ESC [ 2 J`, and cursor positioning, all of which are rendered locally as raw bytes |
| manually read text/display files followed by `od_disp()` | Requires a per-file provenance decision. Manual reading avoids `od_send_file()` selection, substitution, paging, stop-key, and hotkey behavior, but does not provide local terminal emulation |
| `session->user_sound` toggle without a sound dispatcher | Incomplete. The current session code changes and reports the flag but emits none of the recovered ANSI-music, BEL, or local-speaker endpoints from the legacy 59-caller dispatcher |
| `od_putch()` for accepted key echo | Correct when the recovered program echoes that individual key |
| custom `od_get_key()` line editors | Necessary where the original owns echo, queued-command suppression, accepted byte range, Ctrl-R/Ctrl-X, or newline behavior that `od_input_str()` cannot configure |
| `od_get_key(FALSE)` plus `od_sleep(10)` polling | Services input on every iteration and avoids a tight loop; `od_sleep()` alone would not be enough |
| `od_clr_scr()` | Correct OpenDoors clear primitive, but its current force/preference setting is unresolved as described below |
| `od_set_color()` | Correct only for recovered color state; it intentionally becomes a no-op in plain ASCII mode |
| `od_carrier()` guarded by known local-mode state | Necessary because `od_carrier()` itself returns false in local mode |
| `od_exit(errorlevel, FALSE)` | Correct return-to-BBS shutdown rather than a player hangup |

Additional limitations matter for pending work:

1. The current pre-initialization assignment
   `od_control.od_always_clear = FALSE` is overwritten by the installed
   `od_init()`, which unconditionally defaults that field to `TRUE` in its
   second initialization stage. Consequently, current `od_clr_scr()` calls
   force a clear. If the recovered Yankee Trader behavior is to honor the
   caller's screen-clearing preference, the assignment must occur after
   `od_init()`; if it is to force clears, the ineffective assignment should
   be removed. This guide does not choose between those behaviors.
2. `od_get_key()` is intentionally correct for normal prompts where either
   the player or local SysOp may type on the player's behalf. It is not
   sufficient for identifying local-only SysOp commands because it exposes
   origin only through shared `od_last_input` state and returns extended keys
   as multiple bytes.
3. The personality/custom-hotkey approach is DOS-only and therefore cannot
   reproduce Yankee Trader's F4-through-F10 bindings on Win32. Under the
   selected requirement that the SysOp interface need not match the legacy
   DOS interface, the relevant mapping is:

   | Legacy control | Win32/OpenDoors replacement | Yankee Trader integration |
   |---|---|---|
   | F4 local sound | No corresponding command required | In normal remote mode it changes only local presentation |
   | F5 `END` | Exit to BBS; Hangup remains a separate explicit command | Existing `od_before_exit`/`atexit()` cleanup must remain safe |
   | F8 replace remaining time | Toolbar time editor and add/subtract-time menu commands | Currently incomplete: they change `od_control.user_timelimit`, while Yankee Trader enforces its separate `session_deadline` |
   | F9 local snoop | Visible Win32 local display | In normal remote mode it changes only local presentation |
   | F10 SysOp chat | Chat Mode menu, toolbar button, or Alt-C | OpenDoors supplies the chat transport and player interaction; before/after hooks are available for Yankee Trader state synchronization |

   Win32 also sends unaccelerated F2-through-F10 through the common input
   queue, but Yankee Trader does not need to consume those keys merely to
   recreate obsolete DOS bindings.
4. `read_keyboard_line()` currently admits only bytes `0x20` through `0x7e`,
   and the radio composer applies the same range, so text entered through
   those paths cannot contain ESC or the C0 bytes used for AVATAR and
   RA/QuickBBS commands.
   That fact does not sanitize existing compatible records, externally
   edited files, registration data, or BBS/drop-file identity strings.
5. The otherwise-unused generic `yt_input_line()` accepts `0x20` through
   `0xff` and echoes with `od_putch()`. In a CP437 stream, `0x80` through
   `0xff` are glyphs, so this range also excludes the ANSI/AVATAR/RA
   command-introducing C0 bytes. Its acceptance of the extended CP437 glyph
   range must still match the recovered input rules before the function is
   used.
6. `yt_outf()` includes numerous substitutions from player records, messages,
   port/planet/team names, registration text, and BBS identity data. A
   formatting wrapper does not establish their byte range. Each source must
   be traced to its input or file decoder before retaining raw local echo or
   selecting emulation.

No current call should be replaced wholesale by `od_printf()`,
`od_input_str()`, `od_disp_emu()`, or `od_send_file()` merely because it is
higher-level. Each can add player-visible formatting, echo, newline,
input-consumption, substitution, translation, or file-control behavior.
Output selection must instead follow the recovered behavior and the
established provenance of each value.

#include "yt_input.h"

#include "OpenDoor.h"
#include "qb.h"
#include "yt_door.h"
#include "yt_output.h"
#include "yt_platform.h"

#include <ctype.h>
#include <string.h>

int
yt_input_key(void)
{
	int key = (unsigned char)od_get_key(TRUE);
	struct yt_door *door = yt_door_current();

	if (!od_carrier() && !od_control.od_force_local
	    && (door == NULL || !door->identity.local))
		return EOF;
	return key;
}

int
yt_input_command(void)
{
	int key = yt_input_key();

	if (key >= 'a' && key <= 'z')
		key -= 'a' - 'A';
	return key;
}

bool
yt_input_line(char *text, size_t size)
{
	size_t used = 0;

	if (size == 0)
		return false;
	text[0] = '\0';
	for (;;) {
		int key = yt_input_key();

		if (key == EOF)
			return false;
		if (key == '\r' || key == '\n') {
			yt_out_plain("\r\n");
			text[used] = '\0';
			return true;
		}
		if ((key == 8 || key == 127) && used > 0) {
			--used;
			yt_out_plain("\b \b");
			continue;
		}
		if (key >= 32 && key <= 255 && used + 1U < size) {
			text[used++] = (char)key;
			od_putch((char)key);
		}
	}
}

bool
yt_input_number(const char *prompt, double *value)
{
	char line[80];
	struct qb_val_result parsed;

	yt_out_plain(prompt);
	if (!yt_input_line(line, sizeof(line)))
		return false;
	parsed = qb_val(line);
	*value = parsed.valid ? parsed.value : 0.0;
	return true;
}

bool
yt_input_yes_no(const char *prompt)
{
	int key;

	yt_out_plain(prompt);
	do {
		key = yt_input_command();
	} while (key != EOF && key != 'Y' && key != 'N');
	if (key == EOF)
		return false;
	od_putch((char)key);
	yt_out_plain("\r\n");
	return key == 'Y';
}

static bool
poll_splitter(struct yt_input_splitter *splitter)
{
	tODInputEvent event;

	while (yt_input_splitter_can_push(splitter, false)
	    && yt_input_splitter_can_push(splitter, true)
	    && od_get_input(&event, 0, GETIN_RAW)) {
		struct yt_input_value value = {{0, 0}, 0, 0, false};

		if (event.EventType == EVENT_EXTENDED_KEY) {
			value.bytes[0] = 0;
			value.bytes[1] = (uint8_t)event.chKeyPress;
			value.length = 2;
			value.sequence = 0;
		}
		else {
			value.bytes[0] = (uint8_t)event.chKeyPress;
			value.bytes[1] = 0;
		value.length = 1;
			value.sequence = 0;
		}
		value.remote = event.bFromRemote != FALSE;
		if (!yt_input_splitter_push(splitter,
		    value.remote, &value))
			return false;
	}
	return true;
}

bool
yt_input_poll_legacy(struct yt_input_splitter *splitter, float mode,
    enum yt_input_phase phase, struct yt_input_value *selected)
{
	if (!poll_splitter(splitter))
		return false;
	*selected = yt_input_splitter_select(splitter, mode, phase);
	return true;
}

bool
yt_input_poll_merged(struct yt_input_splitter *splitter,
    struct yt_input_value *selected)
{
	if (!poll_splitter(splitter))
		return false;
	*selected = yt_input_splitter_select_merged(splitter);
	return true;
}

bool
yt_input_poll_source(struct yt_input_splitter *splitter, bool remote,
    struct yt_input_value *selected)
{
	if (!poll_splitter(splitter))
		return false;
	*selected = yt_input_splitter_select_source(splitter, remote);
	return true;
}

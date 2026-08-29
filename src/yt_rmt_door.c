#include "yt_rmt_door.h"

#include "OpenDoor.h"
#include "ODScrn.h"

#include <stdlib.h>
#include <string.h>

static struct yt_rmt_door *current_door;
static bool cleanup_registered;

static void
rmt_door_cleanup(void)
{
	if (current_door != NULL)
		yt_platform_rmt_serial_close(&current_door->serial);
}

static bool
rmt_door_local_sink(void *context, const uint8_t *data, size_t length,
    size_t *accepted)
{
	struct yt_rmt_door *door = context;

	*accepted = 0U;
	if (door == NULL || !door->initialized
	    || (data == NULL && length != 0U))
		return false;
	while (length != 0U) {
		INT amount = length > 32767U ? 32767 : (INT)length;

		ODScrnDisplayBuffer((const char *)data, amount);
		data += (size_t)amount;
		length -= (size_t)amount;
		*accepted += (size_t)amount;
	}
	return true;
}

static bool
rmt_door_serial_sink(void *context, const uint8_t *data, size_t length,
    size_t *accepted)
{
	struct yt_rmt_door *door = context;

	*accepted = 0U;
	if (door == NULL || !door->initialized
	    || (data == NULL && length != 0U))
		return false;
	while (length != 0U) {
		INT amount = length > 32767U ? 32767 : (INT)length;

		od_disp((const char *)data, amount, FALSE);
		data += (size_t)amount;
		length -= (size_t)amount;
		*accepted += (size_t)amount;
	}
	return true;
}

bool
yt_rmt_door_prepare(struct yt_rmt_door *door, int port,
    const struct yt_startup_framing *framing, struct yt_error *error)
{
	if (door == NULL)
		return false;
	memset(door, 0, sizeof(*door));
	return yt_platform_rmt_serial_prepare(port, framing, &door->serial,
	    error);
}

bool
yt_rmt_door_start(struct yt_rmt_door *door,
    const struct yt_rmt_serial_state *state, struct yt_error *error)
{
	if (door == NULL || state == NULL || !door->serial.prepared
	    || door->serial.restored || door->initialized
	    || state->outcome != YT_RMT_SERIAL_REMOTE
	    || state->requested_port < 1 || state->requested_port > 4
	    || state->requested_port - 1 > INT16_MAX
	    || state->detected_baud != (float)door->serial.observed_baud) {
		if (error != NULL) {
			error->status = YT_INVALID;
			error->system_error = 0;
			snprintf(error->operation, sizeof(error->operation),
			    "start RMT OpenDoors adapter");
			error->path[0] = '\0';
		}
		return false;
	}
	if (current_door != NULL && current_door != door) {
		if (error != NULL) {
			error->status = YT_INVALID;
			error->system_error = 0;
			snprintf(error->operation, sizeof(error->operation),
			    "reuse RMT OpenDoors adapter");
			error->path[0] = '\0';
		}
		return false;
	}
	current_door = door;
	if (!cleanup_registered) {
		if (atexit(rmt_door_cleanup) != 0) {
			if (error != NULL) {
				error->status = YT_INVALID;
				error->system_error = 0;
				snprintf(error->operation, sizeof(error->operation),
				    "register RMT OpenDoors cleanup");
				error->path[0] = '\0';
			}
			current_door = NULL;
			return false;
		}
		cleanup_registered = true;
	}
	od_control.od_nocopyright = TRUE;
	od_control.od_always_clear = FALSE;
	od_control.od_clear_on_exit = FALSE;
	od_control.od_cp437_to_utf8_out = FALSE;
	od_control.od_no_ra_codes = TRUE;
	od_control.od_default_personality = pdef_od_onerow;
	od_control.od_disable |= DIS_INFOFILE | DIS_NAME_PROMPT | DIS_TIMEOUT;
	od_control.baud = door->serial.observed_baud;
	od_control.od_connect_speed = door->serial.observed_baud;
	od_control.port = (INT16)(state->requested_port - 1);
	od_control.od_open_handle = (DWORD_PTR)door->serial.native_handle;
	(void)snprintf(od_control.od_prog_name, sizeof(od_control.od_prog_name),
	    "Yankee Trader RMT-INIT");
	(void)snprintf(od_control.od_prog_version,
	    sizeof(od_control.od_prog_version), "3.6g");
	od_init();
	door->initialized = true;
	if (!yt_platform_rmt_serial_restore(&door->serial, error)) {
		od_control.od_noexit = TRUE;
		od_exit(EXIT_FAILURE, FALSE);
		door->initialized = false;
		yt_platform_rmt_serial_close(&door->serial);
		current_door = NULL;
		return false;
	}
	return true;
}

void
yt_rmt_door_sink(struct yt_rmt_door *door, struct yt_rmt_output_sink *sink)
{
	if (sink == NULL)
		return;
	sink->context = door;
	sink->local = rmt_door_local_sink;
	sink->serial = rmt_door_serial_sink;
}

bool
yt_rmt_door_local_write(struct yt_rmt_door *door, const uint8_t *data,
    size_t length)
{
	size_t accepted;

	return rmt_door_local_sink(door, data, length, &accepted)
	    && accepted == length;
}

void
yt_rmt_door_finish(struct yt_rmt_door *door, int errorlevel)
{
	if (door == NULL)
		return;
	if (door->initialized) {
		od_control.od_noexit = TRUE;
		od_exit(errorlevel, FALSE);
		door->initialized = false;
	}
	yt_platform_rmt_serial_close(&door->serial);
	if (current_door == door)
		current_door = NULL;
}

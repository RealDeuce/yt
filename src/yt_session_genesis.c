#include "yt_session_internal.h"

#include "yt_file.h"
#include "yt_platform.h"
#include "yt_text.h"

#include <stdio.h>
#include <string.h>

static bool
genesis_handoff_open_output(struct yt_text_output *output,
    struct yt_error *error)
{
	bool opened;

	opened = yt_text_output_open(output, "RMTINIT.TMP", error);
	if (!opened) {
		if (output->last_output_open_basic_error != 0U)
			(void)yt_error_attach_basic_fault_number(error,
			    YT_BASIC_FAULT_GENESIS_OPEN_OUTPUT,
			    output->last_output_open_basic_error);
	}
	return opened;
}

static bool
genesis_handoff_print_command(struct yt_text_output *output,
    const uint8_t *line, size_t line_length, struct yt_error *error)
{
	bool printed;

	printed = yt_text_output_write(output, line, line_length, error);
	if (!printed) {
		if (output->last_write_basic_error != 0U)
			(void)yt_error_attach_basic_fault_number(error,
			    YT_BASIC_FAULT_GENESIS_PRINT_VALUE,
			    output->last_write_basic_error);
	}
	return printed;
}

static bool
genesis_handoff_close_all(struct yt_session *session,
    struct yt_text_output *output, struct yt_error *error)
{
	/*
	 * The database file-1 control predates the new sequential file-5
	 * control.  CLOSE with no file number therefore walks file 5 first,
	 * appending its DOS EOF, and then closes file 1 before RUN.
	 */
	if (!yt_text_output_close_all(output, error)) {
		if (output->last_close_basic_error != 0U)
			(void)yt_error_attach_basic_fault_number(error,
			    YT_BASIC_FAULT_GENESIS_CLOSE_ALL,
			    output->last_close_basic_error);
		return false;
	}
	if (!session->door->game_open)
		return true;
	if (!yt_database_close_all_single(&session->door->game.database,
	    error)) {
		uint16_t basic_error =
		    session->door->game.database.last_close_basic_error;

		if (basic_error != 0U)
			(void)yt_error_attach_basic_fault_number(error,
			    YT_BASIC_FAULT_GENESIS_CLOSE_ALL, basic_error);
		if (session->door->game.database.file == NULL)
			session->door->game_open = false;
		return false;
	}
	session->door->game_open = false;
	return true;
}

static bool
genesis_handoff_run_program(struct yt_session *session,
    struct yt_error *error)
{
	char sibling[1024];
	char *arguments[2];

	if (!yt_platform_sibling_program(sibling, sizeof(sibling),
	    session->executable_path, "rmt-init", error))
		return false;
	arguments[0] = sibling;
	arguments[1] = NULL;
	if (fflush(NULL) != 0) {
		if (error != NULL) {
			error->status = YT_IO_ERROR;
			snprintf(error->operation, sizeof(error->operation),
			    "flush before Genesis");
		}
		return false;
	}
	yt_door_shutdown_for_replace();
	if (!yt_platform_spawn(sibling, arguments, YT_SPAWN_REPLACE, NULL,
	    error))
		return false;
	return true; /* Unreachable after a successful RUN replacement. */
}

static bool
genesis_handoff(struct yt_session *session, struct yt_error *error)
{
	struct yt_text_output output;
	uint8_t line[sizeof(session->door->rmt_handoff_path) + 2U];
	size_t line_length;
	bool result;

	line_length = strlen(session->door->rmt_handoff_path) + 2U;
	memcpy(line, session->door->rmt_handoff_path, line_length - 2U);
	line[line_length - 2U] = '\r';
	line[line_length - 1U] = '\n';
	yt_text_output_init(&output);
	result = false;
	if (!genesis_handoff_open_output(&output, error))
		goto done;
	if (!genesis_handoff_print_command(&output, line, line_length, error))
		goto done;
	if (!genesis_handoff_close_all(session, &output, error))
		goto done;
	result = genesis_handoff_run_program(session, error);

done:
	yt_text_output_destroy(&output);
	return result;
}

bool
yt_session_command_genesis(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t prophecy_first[] =
	    "It has been written that one day a Trader Baron will rise up";
	static const uint8_t prophecy_second[] =
	    "and wipe the universe clean of the evil that infests it.";
	static const uint8_t disabled[] = "*FUNCTION DISABLED*";
	static const uint8_t declined[] =
	    "Alas, today is not the day that the prophesy will be fullfilled.";
	static const uint8_t success_first[] =
	    "...and so it was written, that one day a trader baron would emerge who";
	static const uint8_t success_second[] =
	    "would wipe away the all of the evil in the universe.....";
	uint8_t cached_trader[sizeof(session->player.name) - 1U];
	size_t cached_trader_length = strlen(session->player.name);
	uint8_t prompt[512];
	uint8_t first[256];
	uint8_t second[256];
	size_t prompt_length;
	size_t first_length;
	size_t second_length;
	enum yt_yes_no_answer answer;
	float required_ports = session->door->game.config.genesis_ports;

	if (cached_trader_length > sizeof(cached_trader))
		return session_range_error(error, "Genesis cached trader length");
	memcpy(cached_trader, session->player.name, cached_trader_length);
	if (!session_reload_player(session, error))
		return false;
	if (!session_present_paged_line(session, prophecy_first,
	    sizeof(prophecy_first) - 1U, "Genesis prophecy first row", error))
		return false;
	if (!session_present_paged_fragment(session, prophecy_second,
	    sizeof(prophecy_second) - 1U))
		return false;
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "Genesis prompt leading blank", error))
		return false;
	if (!yt_genesis_confirmation_prompt(cached_trader,
	    cached_trader_length, prompt, sizeof(prompt), &prompt_length))
		return false;
	if (!session_confirm(session, prompt, prompt_length, &answer, error))
		return false;
	if (required_ports > 300.0f) {
		if (!session_present_alert(session, disabled, sizeof(disabled) - 1U,
		    "Genesis disabled row", error))
			return false;
		answer = YT_YES_NO_NO;
	}
	if (answer != YT_YES_NO_YES)
		return session_present_paged_line(session, declined,
		    sizeof(declined) - 1U, "Genesis declined row", error);
	if ((float)session->player.ports_owned < required_ports) {
		if (!yt_genesis_insufficient_rows(required_ports,
		    session->player.ports_owned, first, sizeof(first),
		    &first_length,
		    second, sizeof(second), &second_length))
			return session_range_error(error,
			    "Genesis insufficient row composition");
		if (!session_present_paged_line(session, first, first_length,
		    "Genesis insufficient first row", error))
			return false;
		return session_present_paged_fragment(session, second,
		    second_length);
	}
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "Genesis success leading blank", error))
		return false;
	session->presentation.bold = true;
	if (!session_present_paged_fragment(session, success_first,
	    sizeof(success_first) - 1U))
		return false;
	session->presentation.bold = true;
	if (!session_present_paged_fragment(session, success_second,
	    sizeof(success_second) - 1U))
		return false;
	return genesis_handoff(session, error);
}

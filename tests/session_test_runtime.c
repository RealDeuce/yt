#include "session_test_runtime.h"

#include "OpenDoor.h"

void
session_test_runtime_start(void)
{
	od_control.od_force_local = TRUE;
	od_control.od_nocopyright = TRUE;
	od_init();
	od_control.od_always_clear = FALSE;
	od_control.od_status_on = FALSE;
}

void
session_test_runtime_stop(void)
{
	od_control.od_noexit = TRUE;
	od_exit(0, FALSE);
}

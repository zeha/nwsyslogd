/*============================================================================
=  NetWare syslogd Daemon and Interfaces
=
=  Copyright (C) Unpublished Work of Novell, Inc. All Rights Reserved.
=
=  This work is an unpublished work and contains confidential, proprietary and
=  trade  secret information of Novell, Inc. Access to this work is restricted
=  to (i) Novell, Inc. employees who have a need to know how to perform  tasks
=  within  the scope of their assignments and (ii) entities other than Novell,
=  Inc. who have entered into appropriate license agreements. No part of  this
=  work may be used, practiced, performed, copied, distributed, revised, modi-
=  fied, translated, abridged, condensed, expanded, collected, compiled, link-
=  ed,  recast,  transformed  or  adapted without the prior written consent of
=  Novell, Inc. Any use or exploitation of  this  work  without  authorization
=  could subject the perpetrator to criminal and civil liability.
=
=  setlogmask.c by Russ Bateman, October 2002
==============================================================================
*/
#include <errno.h>
#include <private.h>


int SetSysLogMask
(
	int	maskpri
)
{
	appdata_t	*app;

/*
** The setlogmask() function sets the log priority mask for the current
** process to maskpri and returns the previous mask. If the maskpri argument
** is 0, the current log mask is not modified. Calls by the current process
** to syslog() with a priority not set in maskpri shall be rejected. The
** default log mask allows all priorities to be logged. A call to openlog()
** is not required prior to calling setlogmask().
*/
	if (!(app = GetOrSetAppData()))
		return ENOMEM;

	app->logmask = maskpri;

	return 0;
}

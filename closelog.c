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
=  closelog.c by Russ Bateman, October 2002
==============================================================================
*/
#include <errno.h>
#include <library.h>
#include <private.h>


int CloseSysLog( void )
{
	appdata_t	*app;

/*
** The closelog() function closes any open file descriptors allocated by
** previous calls to openlog() or syslog(). Since these are open by us and
** not by the callers, this doesn't amount to much.
*/
	if (!(app = get_app_data(gLibId)))
		return ENOENT;

consoleprintf("syslog: closesyslog called.\n");
	return DisposeAppData(app);
}

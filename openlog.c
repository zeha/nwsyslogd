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
=  openlog.c by Russ Bateman, October 2002
==============================================================================
*/
#include <errno.h>
#include <string.h>
#include <library.h>

#include <private.h>


int OpenSysLog
(
	const char	*ident,
	int			logopt,
	int			facility
)
{
	size_t		length;
	appdata_t	*app;

/*
** The openlog() function sets process attributes that affect subsequent calls
** to syslog(). The ident argument is a string that is prepended to every
** message. The logopt argument indicates logging options.
**
** The openlog() function may allocate a file descriptor.
*/
	if (!(app = GetOrSetAppData()))
		return ENOMEM;

	app->options    = logopt;
	app->facilities = facility;

	if (length = strlen(ident))
	{
		if (app->identifier && length <= app->length)
		{
			app->length = length;
			strcpy(app->identifier, ident);
		}
		else			// not enough room there--start over...
		{
			rtag_t	rtag;

			if (!(rtag = getallocresourcetag()))
				rtag = gLibAllocRTag;

			Free(app->identifier);

			if (!(app->identifier = AllocSleepOK(length+1, rtag, NULL)))
			{
				app->length = 0;
				return ENOMEM;
			}

			app->length = length;
		}
	}
	else if (app->identifier)
	{
		Free(app->identifier);
		app->identifier = (char *) NULL;
		app->length     = 0;
	}

	consoleprintf("setting a logmask.\n");

	app->logmask = 0xFF000000;

	return 0;
}

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
=  library.c by Russ Bateman, October 2002
==============================================================================
*/
#include <errno.h>
#include <library.h>
#include <netware.h>
#include <private.h>


appdata_t *GetOrSetAppData( void )
{
	rtag_t		rtag;
	appdata_t	*app = NULL;

/*
** If the caller already has instance data, just return it. If not, then
** it's a new caller so allocate some, but attempt to do it on their tag.
** That failing, we'll use our own tag 'cause it will get cleaned up anyway
** when they (or we) unload.
*/
	if (app = get_app_data(gLibId))
		return app;

	if (!(rtag = getallocresourcetag()))
		rtag = gLibAllocRTag;

	if (app = AllocSleepOK(sizeof(appdata_t), rtag, NULL))
	{
		// by default, no logging is going to happen...
		app->logmask    =
		app->options    =
		app->facilities = 0;
		app->identifier = (char *) NULL;
		app->length     = 0;
	}

	if (set_app_data(gLibId, app))
	{
		Free(app);
		return (appdata_t *) NULL;
	}
	
	return app;
}

int DisposeAppData
(
	void	*data_area
)
{
	appdata_t	*app = data_area;

	if (app)
	{
		if (app->identifier)
		{
			Free(app->identifier);
			app->identifier = NULL;
		}

		Free(app);
	}

	return 0;
}

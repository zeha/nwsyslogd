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
=  messages.c by Russell Bateman, November 2001
==============================================================================
*/
#include <netware.h>
#include <private.h>

#define kGetStrings
#include <messages.h>

#define ENGLISH	4

msgstruct_t		SysLogMessages;
static char		*sMessageTable[LAST_MESSAGE+1];
static int		UseHardcodedMessages( msgstruct_t *libraryMessages );

static int UseHardcodedMessages
(
	msgstruct_t	*libraryMessages
)
{
	if (!libraryMessages)
		return -1;

	libraryMessages->count      = LAST_MESSAGE;
	libraryMessages->languageID = ENGLISH;
	libraryMessages->table      = sMessageTable;

	sMessageTable[ 0] = "Message number out of range/unknown message";

	sMessageTable[ 1] = START_MSG;
	sMessageTable[ 2] = VERSION_LIB;
	sMessageTable[ 3] = COPYRIGHT_LIB;
	sMessageTable[ 4] = BSD_COPYRIGHT;

	sMessageTable[ 5] = INIT_ERR_03;
	sMessageTable[ 6] = INIT_ERR_04;
	sMessageTable[ 7] = INIT_ERR_05;
	sMessageTable[ 8] = INIT_ERR_06;
	sMessageTable[ 9] = INIT_ERR_07;

	sMessageTable[LAST_MESSAGE] = STOP_MSG;

	return 0;
}

int LoadMessages
(
	void	*handle
)
{
	int	err = -1;

	handle = handle;

	// attempt to get our messages...
	err = ReturnMessageInformation(handle, &SysLogMessages.table,
						&SysLogMessages.count, &SysLogMessages.languageID, NULL);

	// ...but if not, then use our back-up messages
	if (err || !SysLogMessages.count)
		err = UseHardcodedMessages(&SysLogMessages);

	if (err)
		SysLogMessages.signature = 0x2D2D2D2D;

	// failure is so unlikely...
	return err;
}

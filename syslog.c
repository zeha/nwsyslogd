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
=  syslog.c by Russ Bateman, October 2002
==============================================================================
*/
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "private.h"


int LogSysLog
(
	int			priority,
	const char	*message,
	va_list		args
)
{
	appdata_t	*app;
	time_t	myNow;
	const char*		timestamp;
	char*			expandedMessage;

/*
** The syslog() function sends a message to syslogd.nlm, which logs it in a
** file on the path, "c:\nwserver\sys$log" or on another path as specified by
** the configuration file (see config.c). Someday, perhaps, it will be able
** to forward the message over the network to a different host per the way
** this works on UNIX systems. The logged message includes a message header
** and a message body. The message header contains at least a timestamp and
** a tag string.
**
** The message body is generated from the message and following arguments in
** the same manner as if these were arguments to printf(), except that the
** additional conversion specification %m is recognized to convert no
** arguments, causing the output of the error message string associated with
** the value of 'errno' on entry to syslog(), and may be mixed with argument
** specifications of the form "%n$". If a complete conversion specification
** with the m conversion specifier character is not just "%m ", the behavior
** is not handled. A trailing newline may be added if needed.
**
** Values of the priority argument are formed by OR'ing together a severity-
** level value and an optional facility value. If no facility value is
** specified, the current default facility value is used.
**
** The syslog() functions may allocate a file descriptor. It is not necessary
** to call openlog() prior to calling syslog().
*/
	if (!(app = GetOrSetAppData()))
		return ENOMEM;

	if (!app->logmask)
		return ENOENT;	// XXX choose better retval
	
consoleprintf("logmask check2: %x & %x = %x\n",app->logmask,priority,app->logmask & priority);

	if (!(app->logmask & priority))
	{
		consoleprintf(" -> aborting\n");
		return ENOENT;	// XXX choose better retval
	}

	if ((expandedMessage = (char *) malloc(MAXSVLINE)) == NULL)
		return ENOMEM;

consoleprintf("trying to log smthng.\n");

	// get a timestamp text
	time(&myNow);
	timestamp = ctime(&myNow) + 4;
	
	vsnprintf(expandedMessage,MAXSVLINE,message,args);

	// log the message (passing NULL: use LocalHostName)
	LogAMessage(	
					priority,
					app->facilities,
					timestamp,
					NULL,
					(app->identifier == NULL) ? "UNKNOWN" : app->identifier,
					expandedMessage,
					(app->options & LOG_CONS) ? LOG_CONS : 0
					);
					
	free(expandedMessage);
	
	return 0;
}

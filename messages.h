#ifndef __messages_h__
#define __messages_h__
/*============================================================================
=  LibC to CLib Shim for NLMs
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
=  messages.h by Russell Bateman, November 2001
==============================================================================
*/
#include <private.h>

#ifdef MSG
# undef MSG
#endif
/*
** This message file is set up so that this NLM binary might be enabled even
** if a message file cannot be found. Please update definition 'LAST_MESSAGE'
** at the end of this file as equal to the second argument of the last message,
** i.e.: the 'n' in MSG(s, n).
*/
#ifdef kGetStrings
# define MSG(s, id)		s	// how to get the strings directly
#else
# define MSG(s, id)		SysLogMessages.table[id]
#endif

#define START_MSG			MSG("syslogd.nlm messages start here."					, 1)
#define VERSION_LIB		MSG("VeRsIoN=1.0"												, 2)
#define COPYRIGHT_LIB	MSG("CoPyRiGhT=Copyright (c) 2004,\
 Novell, Inc. All rights reserved."														, 3)
#define BSD_COPYRIGHT	MSG("Portions copyright (c) 1980, 1982, 1983,\
 1986, 1990 and 1993 by the Regents\nof the University of California\
 (Free BSD).\n"																				, 4)

// main.c:
// Message syslogd-1.0-I01 is hard coded...
// Message syslogd-1.0-I02 is hard coded...
#define INIT_ERR_03		MSG("syslogd-1.0-I05: unused message.\n"				, 5)
#define INIT_ERR_04		MSG("syslogd-1.0-I05: unused message.\n"				, 6)
#define INIT_ERR_05		MSG("syslogd-1.0-I05: unused message.\n"				, 7)
#define INIT_ERR_06		MSG("syslogd-1.0-I06: Unable to allocate resource\
 tag.\n"																							, 8)
#define INIT_ERR_07		MSG("syslogd-1.0-I07: Unable to open or parse\
 configuration file.\n"																		, 9)


#define STOP_MSG			MSG("syslogd.nlm messages end here."					, 10)
#define LAST_MESSAGE		10

#endif

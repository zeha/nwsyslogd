/*============================================================================
=  Unicode NLM Replacement Library
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
=  dllmain.c by Russell Bateman, June 2004
==============================================================================
*/
#include <event.h>
#include <stdio.h>
#include <screen.h>
#include <string.h>
#include <library.h>
#include <netware.h>
#include <pthread.h>
#include <windows.h>
#include <messages.h>
#include <semaphore.h>

int					 gLibId        = -1;
const char			*gCfgFileName  = P_cfgfile;
void					*gModuleHandle = (void *) NULL;
FILE					*gSysLogFile   = (FILE *) NULL;
scr_t					gLibScreen     = NULL;
rtag_t				gLibAllocRTag  = NULL,
						sDownEventRTag = NULL;
pthread_mutex_t	gFileMutex;
event_handle_t		sDownEventHandle = 0;


static int DownWarning( void *printf, void *parm, void *userParm );


int DllMain								// returns TRUE (things okay), FALSE (failure)
(
	void				*hinstDLL,		// equivalent to return from register_library()
	unsigned long	fdwReason,		// our attach/detach, etc. message
	void				*lvpReserved	// library per-VM data, place to put error...
)											// ...return or NLM handle
{
	switch (fdwReason)
	{
		default :						// bogus message: don't process it!
			return FALSE;

		case DLL_PROCESS_ATTACH :	// (don't have per-NLM data)
		case DLL_PROCESS_DETACH :	// (ibid)
		case DLL_THREAD_ATTACH :	// (don't have per-thread data)
		case DLL_THREAD_DETACH :	// (ibid)
		case DLL_ACTUAL_DLLMAIN :	// (say that we're a library)
			return TRUE;

		case DLL_NLM_STARTUP :		// this is our start-up initialization
			gLibId        = (int) hinstDLL;
			gModuleHandle = lvpReserved;
			gLibScreen    = getconsolehandle();

			if (LoadMessages(gModuleHandle))
			{
				OutputToScreen(gLibScreen,
					"syslog-1.0-I02: Unable to load messages for syslogd.nlm...\n");
				return FALSE;
			}

			OutputToScreen(gLibScreen, BSD_COPYRIGHT);

			if (!(sDownEventRTag = AllocateResourceTag(gModuleHandle,
								"NetWare Shutdown Event Notification", EventSignature)))
			{
				OutputToScreen(0, INIT_ERR_06);
				return FALSE;
			}

			sDownEventHandle = RegisterForEventNotification(sDownEventRTag,
											EVENT_PRE_DOWN_SERVER, EVENT_PRIORITY_DEVICE,
											DownWarning, (Report_t) NULL, (void *) NULL);

			if (!(gLibAllocRTag = AllocateResourceTag(gModuleHandle,
								"LibC Interface Working Memory", AllocSignature)))
			{
				OutputToScreen(0, INIT_ERR_06);
				return FALSE;
			}

			pthread_mutex_init(&gFileMutex, (const pthread_mutexattr_t *) NULL);
			register_destructor((int) hinstDLL, DisposeAppData);
			return TRUE;

		case DLL_NLM_SHUTDOWN :		// this is our shut-down deinitialization
			UnRegisterEventNotification(sDownEventHandle);
			pthread_mutex_destroy(&gFileMutex);
			CloseAllLogFiles();
			FreeUndisposedMemory();
			return TRUE;
	}

	return FALSE;
}

int DownWarning
(
	void	*printf,
	void	*parm,
	void	*userParm
)
{
#pragma unused(printf,parm,userParm)
	CloseAllLogFiles();
}

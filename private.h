#ifndef __private_h__
#define __private_h__
/*============================================================================
=  NetWare syslogd Daemon and Interfaces
=
=  Copyright (C) Unpublished Work of Novell, Inc. All Rights Reserved.
=
=  This work is subject to U.S. and international copyright laws and treaties.
=  Use and redistribution of this work is subject  to  the  license  agreement
=  accompanying  the  software  development kit (SDK) that contains this work.
=  However, no part of this work may be revised and/or  modified  without  the
=  prior  written consent of Novell, Inc. Any use or exploitation of this work
=  without authorization could subject the perpetrator to criminal  and  civil
=  liability. 
=
=  Interfaces to private syslogd.nlm.
=
=  private.h
==============================================================================
*/
#include <stdarg.h>
#include <stddef.h>
#include <syslog.h>
#include <netware.h>
#include <pthread.h>
#include <sys/param.h>

#include <messages.h>

#define kMsgMgrSig	'MMgr'

#ifdef DEBUG
# define STATIC
#else
# define STATIC
#endif


typedef struct
{
	unsigned long	signature;	// 'MMgr'
	size_t			count;		// count of messages supplied in buffer
	int				languageID;	// identifies language of supplied messages
	char				**table;		// pointer to array of pointers into the buffer
} msgstruct_t;

typedef struct
{
	int		logmask;				// against which syslog() called
	char		*identifier;		// to be prepended to each message
	size_t	length;				// length of preceeding
	int		options;				// display options for each message
	int		facilities;			// which facility is default/origin
} appdata_t;

// our global variables...
extern int					gLibId;
extern void					*gModuleHandle;
extern const char			*gCfgFileName;
extern rtag_t				gLibAllocRTag;
extern pthread_mutex_t	gFileMutex;
extern msgstruct_t		SysLogMessages;

// important prototypes...
appdata_t	*GetOrSetAppData    ( void );
int			DisposeAppData      ( void *data_area );
void			CloseAllLogFiles    ( void );
void			FreeUndisposedMemory( void );
void			ParseSysLogConf     ( int CalledFromSignal );
int			LogAMessage         ( int priority, int facility,
												const char *timestamp, const char *from,
												const char* prog, const char *message,
												int flags );

// other prototypes...
int	LoadMessages ( void *handle );

// exported prototypes...
int	CloseSysLog  ( void );
int	OpenSysLog   ( const char *ident, int logopt, int facility );
int	SetSysLogMask( int maskpri );
int	LogSysLog    ( int priority, const char *message, va_list args );

/*
** Constants and types where I'm not sure if they are good as they are now.
** Most of it is just copied over from <insert-favourite-freebsd-header-here.h>
** Christian Hofstaedtler, 8th May 2004
*/
#include <paths.h>


#define	LOG_NFACILITIES	24	/* current number of facilities */
#define	LOG_FACMASK	0x03f8	/* mask to extract facility part */
				/* facility of pri */
#define	LOG_FAC(p)	(((p) & LOG_FACMASK) >> 3)

struct sockaddr_storage {
        unsigned char           ss_len;         /* address length */
        unsigned char           ss_family;      /* address family */
        unsigned char   fill[126];
};

#define NBPW    sizeof(int)     /* number of bytes per word (integer) */

#define 	NFDBITS   (sizeof(fd_mask) * NBBY)

typedef long fd_mask;

#define	LOG_PRIMASK	0x07	/* mask to extract priority part (internal) */
				/* extract priority */
#define	LOG_PRI(p)	((p) & LOG_PRIMASK)
#define	LOG_MAKEPRI(fac, pri)	((fac) | (pri))


#define	INTERNAL_NOPRI	0x0	/* the "no priority" priority */
				/* mark "facility" */
#define	INTERNAL_MARK	LOG_MAKEPRI((LOG_NFACILITIES<<3), 0)

#define	LINE_MAX		 2048	/* max bytes in an input line */


#define	MAXLINE		1024		/* maximum line length */
#define	MAXSVLINE	120		/* maximum saved line length */



#endif

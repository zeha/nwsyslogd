/*
 * Copyright (c) 1983, 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

/*
 *  syslogd -- log system messages
 *
 * This program implements a system log.
 *
 * To kill syslogd, send signal TERM.  A HUP signal will
 * cause it to reread its configuration file.
 *
 * Defined Constants:
 *
 * MAXLINE -- the maximimum line length that can be handled.
 * DEFUPRI -- the default priority for user messages
 *
 * Author: Eric Allman
 * extensive changes by Ralph Campbell
 * more extensive changes by Eric Allman (again)
 * Extension to log by program name as well as facility and priority
 *   by Peter da Silva.
 * -u and -v by Harlan Stenn.
 * Priority comparison code by Harlan Stenn.
 */

#define DEFUPRI		(LOG_USER|LOG_NOTICE)
#define TIMERINTVL	30		/* interval for checking flush, mark */

#define SYSLOG_NAMES		// import syslog names from syslog.h
#define EXPERIMENTAL		// import other stuff from syslog.h

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
//#include <sys/uio.h>
#include <sys/time.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <synch.h>

#include <library.h>
#include <netware.h>

/* syslog only */
#include "sysexits.h"
#include "addrinfo.h"

#include "syslog.h"
#include "private.h"
#include "messages.h"


#ifdef NI_WITHSCOPEID
static const int withscopeid = NI_WITHSCOPEID;
#else
static const int withscopeid;
#endif

// this ain't gonna work...
const char	gConsolePath[]		= "/dev/console";



#define	dprintf		if (Debug) printf

#define		ADDDATE		1
#define		IGN_CONS	2
#define		MARK		4
#define		SYNC_FILE	8

//#define MAXUNAMES	20	/* maximum number of user names */

/*
 * This structure represents the files that will have log
 * copies printed.
 */

struct filed {
	struct	filed *f_next;		/* next in linked list */
	short	f_type;			/* entry type, see below */
	short	f_file;			/* file descriptor */
	time_t	f_time;			/* time this was last written */
	char	*f_host;		/* host from which to recd. */
	u_char	f_pmask[LOG_NFACILITIES+1];	/* priority mask */
	u_char	f_pcmp[LOG_NFACILITIES+1];	/* compare priority */
#define PRI_LT	0x1
#define PRI_EQ	0x2
#define PRI_GT	0x4
	char	*f_program;		/* program this applies to */
	union {
		struct {
			char	f_hname[MAXHOSTNAMELEN];
			struct addrinfo *f_addr;

		} f_forw;		/* forwarding address */
		char	f_fname[MAXPATHLEN];
	} f_un;
	char	f_prevline[MAXSVLINE];		/* last message logged */
	char	f_lasttime[16];			/* time of last occurrence */
	char	f_prevhost[MAXHOSTNAMELEN];	/* host from which recd. */
	int	f_prevpri;			/* pri of f_prevline */
	int	f_prevlen;			/* length of f_prevline */
	int	f_prevcount;			/* repetition cnt of prevline */
	u_int	f_repeatcount;			/* number of "repeated" msgs */
};

/*
 * Struct to hold records of network addresses that are allowed to log
 * to us.
 */
struct allowedpeer {
	int isnumeric;
	u_short port;
	union {
		struct {
			struct sockaddr_storage addr;
			struct sockaddr_storage mask;
		} numeric;
		char *name;
	} u;
#define a_addr u.numeric.addr
#define a_mask u.numeric.mask
#define a_name u.name
};


/*
 * Intervals at which we flush out "message repeated" messages,
 * in seconds after previous message is logged.  After each flush,
 * we move to the next interval until we reach the largest.
 */
int	repeatinterval[] = { 30, 120, 600 };	/* # of secs before flush */
#define	MAXREPEAT ((sizeof(repeatinterval) / sizeof(repeatinterval[0])) - 1)
#define	REPEATTIME(f)	((f)->f_time + repeatinterval[(f)->f_repeatcount])
#define	BACKOFF(f)	{ if (++(f)->f_repeatcount > MAXREPEAT) \
				 (f)->f_repeatcount = MAXREPEAT; \
			}

/* values for f_type */
#define F_UNUSED	0		/* unused entry */
#define F_FILE		1		/* regular file */
#define F_CONSOLE	2		/* console terminal */
#define F_FORW		3		/* remote machine */

const char *TypeNames[4] = {
	"UNUSED",	"FILE",		"CONSOLE", "FORW"
};

static struct filed *Files;	/* Log files that we write to */
static struct filed gConsoleFileD;	/* Console */

static int	Debug;		/* debug flag */
static int	resolve = 1;	/* resolve hostname */
static char	LocalHostName[MAXHOSTNAMELEN];	/* our hostname */
static char	*LocalDomain;	/* our local domain name */
static char *BindHostname = NULL;
static int	*SocketInternet;		/* Internet datagram socket */
static int	Initialized;	/* set when we have initialized ourselves == config file has been read */
int LogSysLogRunning = 0;

static int	MarkInterval = 20 * 60;	/* interval between marks in seconds */
static int	MarkSeq;	/* mark sequence number */

static int	SecureMode;	/* when true, receive only unix domain socks */
#ifdef INET6
static int	family = PF_UNSPEC; /* protocol family (IPv4, IPv6 or both) */
#else
static int	family = PF_INET; /* protocol family (IPv4 only) */
#endif
static int	send_to_all;	/* send message to all IPv4/IPv6 addresses */
static int	no_compress;	/* don't compress messages (1=pipes XXX, 2=all) */


struct allowedpeer *AllowedPeers; /* List of allowed peers */
static int	NumAllowed;	/* Number of entries in AllowedPeers */

static int	UniquePriority;	/* Only log specified priority? */
static int	LogFacPri;	/* Put facility and priority in log message: */
				/* 0=no, 1=numeric, 2=names */
static int	KeepKernFac;	/* Keep remotely logged kernel facility */



volatile sig_atomic_t MarkSet, WantDie;



static int	allowaddr(char *);
static void	cfline(const char *, struct filed *,
		    const char *, const char *);
static const char *cvthname(struct sockaddr *);
static int	decode(const char *, CODE *);
static void	die(int);
static void	domark(int);
static void	fprintlog(struct filed *, int, const char *);
static int	*socksetup(int, const char *);
static void ParseSysLogConf(int CalledFromSignal);
static void	logerror(const char *);
static void	LogDoMark(void);
static int	skip_message(const char *, const char *, int);
static void	printline(const char *, char *);
static void	usage(void);
static int	ValidatePeerPermissions(struct sockaddr *, const char *);
static void	unmapped(struct sockaddr *);
static void sighup_handler( int signo );
static void sigterm_handler( int signo );
static int logmessage(int pri, const char *msg, const char *from, int flags);

static void sighup_handler( int signo )	// reparse changed configuration file
{
	ParseSysLogConf( signo );
}

static void sigterm_handler( int signo )	// quit!
{
	WantDie = signo;
}


static void LogParseCommandLine(int argc, char* argv[])
{
	int ch;
	while ((ch = getopt(argc, argv, "46Aa:b:cdf:kl:m:nop:P:suv")) != -1)
		switch (ch) {
		case '4':
			family = PF_INET;
			break;
		case '6':
#ifdef INET6
			family = PF_INET6;
#endif
			break;
		case 'A':
			send_to_all++;
			break;
		case 'a':		/* allow specific network addresses only */
			if (allowaddr(optarg) == -1)
				usage();
			break;
		case 'b':
			BindHostname = optarg;
			break;
		case 'c':
			no_compress++;
			break;
		case 'd':		/* debug */
			Debug++;
			break;
		case 'f':		/* configuration file */
			gCfgFileName = optarg;
			break;
		case 'k':		/* keep remote kern fac */
			KeepKernFac = 1;
			break;
		case 'm':		/* mark interval */
			MarkInterval = atoi(optarg) * 60;
			break;
		case 'n':
			resolve = 0;
			break;
		case 's':		/* no network mode */
			SecureMode++;
			break;
		case 'u':		/* only log specified priority */
		    UniquePriority++;
			break;
		case 'v':		/* log facility and priority */
		  	LogFacPri++;
			break;
		case '?':
		default:
			usage();
		}
	if ((argc -= optind) != 0)
		usage();
}

void CloseAllLogFiles( void )			// called at shut-down
{
	struct filed *f, *next, **nextp;
	
	/*
	 *  Close all open log files.
	 */
	Initialized = 0;
	for (f = Files; f != NULL; f = next) {
		/* flush any pending output */
		if (f->f_prevcount)
			fprintlog(f, 0, (char *)NULL);

		switch (f->f_type) {
		case F_FILE:
		case F_FORW:
		case F_CONSOLE:
			(void)close(f->f_file);
			break;
		}
		next = f->f_next;
		if (f->f_program) free(f->f_program);
		if (f->f_host) free(f->f_host);
		free((char *)f);
	}
	Files = NULL;
	nextp = &Files;

	return;
}

fd_set *s_fdsr_base = NULL;

int main(int argc, char *argv[])
{
	int i, fdsrmax = 0, l;
	fd_set *fdsr = NULL;
	struct sockaddr_storage frominet;
	char line[MAXLINE + 1];
	const char *hname;
	struct timeval tv, *tvp;
	size_t len;

	LogParseCommandLine(argc,argv);

	gConsoleFileD.f_type = F_CONSOLE;
	strlcpy(gConsoleFileD.f_un.f_fname, gConsolePath + sizeof _PATH_DEV - 1,
														sizeof(gConsoleFileD.f_un.f_fname));
	if (SecureMode <= 1)
		SocketInternet = socksetup(family, BindHostname);

	if (SocketInternet) {
		// close socket(s) for reading if specified.
		if (SecureMode) {
			for (i = 0; i < *SocketInternet; i++) {
				if (shutdown(SocketInternet[i+1], SHUT_RD) < 0) {
					logerror("shutdown");
					if (!Debug)
						die(0);
				}
			}
		}
	}

	ParseSysLogConf(0);

	signal(SIGTERM, sigterm_handler);
	signal(SIGHUP, sighup_handler);

	// we "run" -> library functions may be called now.
	LogSysLogRunning = 1;

	tvp = &tv;
	tv.tv_sec = tv.tv_usec = 0;

	if (SocketInternet && !SecureMode) {
		for (i = 0; i < *SocketInternet; i++) {
		    if (SocketInternet[i+1] != -1 && SocketInternet[i+1] > fdsrmax)
			fdsrmax = SocketInternet[i+1];
		}
	}

	fdsr = s_fdsr_base = (fd_set *)calloc(howmany(fdsrmax+1, NFDBITS), sizeof(fd_mask));
	if (fdsr == NULL)
		errx(1, "calloc fd_set");

	for (;;) {
		if (MarkSet)
			LogDoMark();
		if (WantDie)
			die(WantDie);

		bzero(fdsr, howmany(fdsrmax+1, NFDBITS) * sizeof(fd_mask));

		if (SocketInternet && !SecureMode) {
			for (i = 0; i < *SocketInternet; i++) {
				if (SocketInternet[i+1] != -1)
					FD_SET(SocketInternet[i+1], fdsr);
			}
		}
		i = select(fdsrmax+1, fdsr, NULL, NULL, tvp);
		switch (i) {
		case 0:
			if (tvp) {
				tvp = NULL;
			}
			continue;
		case -1:
			if (errno != EINTR)
				logerror("select");
			continue;
		}
		if (SocketInternet && !SecureMode) {
			for (i = 0; i < *SocketInternet; i++) {
				if (FD_ISSET(SocketInternet[i+1], fdsr)) {
					len = sizeof(frominet);
					l = recvfrom(SocketInternet[i+1], line, MAXLINE,
					     0, (struct sockaddr *)&frominet,
					     &len);
					if (l > 0) {
						line[l] = '\0';
						hname = cvthname((struct sockaddr *)&frominet);
						unmapped((struct sockaddr *)&frominet);
						if (ValidatePeerPermissions((struct sockaddr *)&frominet, hname))
							printline(hname, line);
					} else if (l < 0 && errno != EINTR)
						logerror("recvfrom inet");
				}
			}
		}
	}

	FreeUndisposedMemory();
}

static void unmapped(struct sockaddr *sa)
{
#pragma unused(sa)
/*	struct sockaddr_in6 *sin6;
	struct sockaddr_in sin4;


#ifdef NETWARE
	// no v6 in here (yet)
	return;
#else
	if (sa->sa_family != AF_INET6)
		return;
	if (sa->sa_len != sizeof(struct sockaddr_in6) ||
	    sizeof(sin4) > sa->sa_len)
		return;
	sin6 = (struct sockaddr_in6 *)sa;
	if (!IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr))
		return;

	memset(&sin4, 0, sizeof(sin4));
	sin4.sin_family = AF_INET;
	sin4.sin_len = sizeof(struct sockaddr_in);
	memcpy(&sin4.sin_addr, &sin6->sin6_addr.s6_addr[12],
	       sizeof(sin4.sin_addr));
	sin4.sin_port = sin6->sin6_port;

	memcpy(sa, &sin4, sin4.sin_len);
#endif
	*/
}

static void
usage(void)
{

	fprintf(stderr, "%s\n%s\n%s\n%s\n",
		"usage: syslogd [-Acdknosuv] [-a allowed_peer]",		// 46
		"               [-b bind address] [-f config_file]",
		"               [-m mark_interval]");
	exit(1);
}

/*
 * Take a raw input line, decode the message, and print the message
 * on the appropriate log files.
 */
static void
printline(const char *hname, char *msg)
{
	int c, pri;
	char *p, *q, line[MAXLINE + 1];

	/* test for special codes */
	pri = DEFUPRI;
	p = msg;
	if (*p == '<') {
		pri = 0;
		while (isdigit(*++p))
			pri = 10 * pri + (*p - '0');
		if (*p == '>')
			++p;
	}
	if (pri &~ (LOG_FACMASK|LOG_PRIMASK))
		pri = DEFUPRI;

	/* don't allow users to log kernel messages */
	if (LOG_FAC(pri) == LOG_KERN && !KeepKernFac)
		pri = LOG_MAKEPRI(LOG_USER, LOG_PRI(pri));

	q = line;

	while ((c = (unsigned char)*p++) != '\0' &&
	    q < &line[sizeof(line) - 4]) {
		if ((c & 0x80) && c < 0xA0) {
			c &= 0x7F;
			*q++ = 'M';
			*q++ = '-';
		}
		if (isascii(c) && iscntrl(c)) {
			if (c == '\n') {
				*q++ = ' ';
			} else if (c == '\t') {
				*q++ = '\t';
			} else {
				*q++ = '^';
				*q++ = c ^ 0100;
			}
		} else {
			*q++ = c;
		}
	}
	*q = '\0';

	logmessage(pri, line, hname, 0);
}

static time_t	now;

/*
 * Match a program or host name against a specification.
 * Return a non-0 value if the message must be ignored
 * based on the specification.
 */
static int
skip_message(const char *name, const char *spec, int checkcase)
{
#pragma unused(checkcase)
	const char *s;
	char prev, next;
	int exclude = 0;
	/* Behaviour on explicit match */

	if (spec == NULL)
		return 0;
	switch (*spec) {
	case '-':
		exclude = 1;
		/*FALLTHROUGH*/
	case '+':
		spec++;
		break;
	default:
		break;
	}
#ifndef __NOVELL_LIBC__
	if (checkcase)
		s = strstr (spec, name);
	else
		s = strcasestr (spec, name);
#else
	s = strstr (spec, name);
#endif

	if (s != NULL) {
		prev = (s == spec ? ',' : *(s - 1));
		next = *(s + strlen (name));

		if (prev == ',' && (next == '\0' || next == ','))
			/* Explicit match: skip iff the spec is an
			   exclusive one. */
			return exclude;
	}

	/* No explicit match for this name: skip the message iff
	   the spec is an inclusive one. */
	return !exclude;
}

/*
 * Log a message to the appropriate log files, users, etc. based on
 * the priority.
 */
static int logmessage(int pri, const char *msg, const char *from, int flags)
{
	int i, fac, msglen, prilev;
 	char prog[NAME_MAX+1];
//	char buf[MAXLINE+1];
	const char *timestamp;

	dprintf("logmessage: pri %o, flags %x, from %s, msg %s\n",
	    pri, flags, from, msg);

	/*
	 * Check to see if msg looks non-standard.
	 */
	msglen = strlen(msg);
	if (msglen < 16 || msg[3] != ' ' || msg[6] != ' ' ||
	    msg[9] != ':' || msg[12] != ':' || msg[15] != ' ')
		flags |= ADDDATE;

	// get a now for ADDDATE
	time(&now);
	if (flags & ADDDATE) {
		timestamp = ctime(&now) + 4;
	} else {
		timestamp = msg;
		msg += 16;
		msglen -= 16;
	}

	/* skip leading blanks */
	while (isspace(*msg)) {
		msg++;
		msglen--;
	}

	/* extract facility and priority level */
	if (flags & MARK)
		fac = LOG_NFACILITIES;
	else
		fac = LOG_FAC(pri);
	prilev = LOG_PRI(pri);

	/* extract program name */
	for (i = 0; i < NAME_MAX; i++) {
		if (!isprint(msg[i]) || msg[i] == ':' || msg[i] == '[')
			break;
		prog[i] = msg[i];
	}
	prog[i] = 0;
	
	return LogAMessage(prilev, fac, timestamp, from, prog, msg, 0);
}

// called from: the network and from LogSysLog
int LogAMessage(int priority, int facility, const char *timestamp, const char *from, const char* prog, const char *message, int flags)
{
	struct filed *f;
	int msglen = strlen(message);
	int ErrorEncountered = 0;
	const char* myFrom;
	
	// update now timestamp for use below
	time(&now);
	
	if (from == NULL)
		myFrom = LocalHostName;
		else
		myFrom = from;

	// acquire mutex
	if (mutex_lock(&gFileMutex) != 0)
		ErrorEncountered = 1;

	/* log the message to the particular outputs */
	if (!Initialized)
		ErrorEncountered = 1;

consoleprintf("logamessage: %s %s %s\n",myFrom,prog,message);
		
	if (ErrorEncountered)
	{
		if (flags & LOG_CONS)
		{
			f = &gConsoleFileD;
			f->f_file = open(gConsolePath, O_WRONLY, 0);

			if (f->f_file >= 0) {
				(void)strlcpy(f->f_lasttime, timestamp,
					sizeof(f->f_lasttime));
				fprintlog(f, flags, message);
				(void)close(f->f_file);
			}
			if (mutex_unlock(&gFileMutex) != 0)
			{
				// mutex error
				consoleprintf("syslogd: mutex_unlock error\n");
			}
		}
		return ENOENT;
	}
	
	for (f = Files; f; f = f->f_next) {
		/* skip messages that are incorrect priority */
		if (!(((f->f_pcmp[facility] & PRI_EQ) && (f->f_pmask[facility] == priority))
		     ||((f->f_pcmp[facility] & PRI_LT) && (f->f_pmask[facility] < priority))
		     ||((f->f_pcmp[facility] & PRI_GT) && (f->f_pmask[facility] > priority))
		     )
		    || f->f_pmask[facility] == INTERNAL_NOPRI)
			continue;

		/* skip messages with the incorrect hostname */
		if (skip_message(myFrom, f->f_host, 0))
			continue;

		/* skip messages with the incorrect program name */
		if (skip_message(prog, f->f_program, 1))
			continue;

		/* skip message to console if it has already been printed */
		if (f->f_type == F_CONSOLE && (flags & IGN_CONS))
			continue;

		/* don't output marks to recently written files */
		if ((flags & MARK) && (now - f->f_time) < MarkInterval / 2)
			continue;

		/*
		 * suppress duplicate lines to this file
		 */
		if (no_compress == 0 &&
		    (flags & MARK) == 0 && msglen == f->f_prevlen &&
		    !strcmp(message, f->f_prevline) &&
		    !strcasecmp(myFrom, f->f_prevhost)) 
		    {
			(void)strlcpy(f->f_lasttime, timestamp,
				sizeof(f->f_lasttime));
			f->f_prevcount++;
			dprintf("msg repeated %d times, %ld sec of %d\n",
			    f->f_prevcount, (long)(now - f->f_time),
			    repeatinterval[f->f_repeatcount]);
			/*
			 * If domark would have logged this by now,
			 * flush it now (so we don't hold isolated messages),
			 * but back off so we'll flush less often
			 * in the future.
			 */
			if (now > REPEATTIME(f)) {
				fprintlog(f, flags, (char *)NULL);
				BACKOFF(f);
			}
		} else {
			/* new line, save it */
			if (f->f_prevcount)
				fprintlog(f, 0, (char *)NULL);
			f->f_repeatcount = 0;
			f->f_prevpri = priority;
			(void)strlcpy(f->f_lasttime, timestamp,
				sizeof(f->f_lasttime));
			(void)strlcpy(f->f_prevhost, myFrom,
			    sizeof(f->f_prevhost));
			if (msglen < MAXSVLINE) {
				f->f_prevlen = msglen;
				(void)strlcpy(f->f_prevline, message, sizeof(f->f_prevline));
				fprintlog(f, flags, (char *)NULL);
			} else {
				f->f_prevline[0] = 0;
				f->f_prevlen = 0;
				fprintlog(f, flags, message);
			}
		}
	}
	if (mutex_unlock(&gFileMutex) != 0)
	{
		// mutex error
		consoleprintf("syslogd: mutex_unlock error\n");
	}
	return 0;
}

static void
fprintlog(struct filed *f, int flags, const char *msg)
{
	struct iovec iov[7];
	struct iovec *v;
	struct addrinfo *r;
	int i, l, lsent = 0;
	char line[MAXLINE + 1], repbuf[80], *wmsg = NULL;
//	const char *msgret;

	v = iov;

	v->iov_base = f->f_lasttime;
	v->iov_len = 15;
	v++;
	v->iov_base = " ";
	v->iov_len = 1;
	v++;

	if (LogFacPri) {
	  	static char fp_buf[30];	/* Hollow laugh */
		int fac = f->f_prevpri & LOG_FACMASK;
		int pri = LOG_PRI(f->f_prevpri);
		const char *f_s = NULL;
		char f_n[5];	/* Hollow laugh */
		const char *p_s = NULL;
		char p_n[5];	/* Hollow laugh */

		if (LogFacPri > 1) {
		  CODE *c;

		  for (c = facilitynames; c->c_name; c++) {
		    if (c->c_val == fac) {
		      f_s = c->c_name;
		      break;
		    }
		  }
		  for (c = prioritynames; c->c_name; c++) {
		    if (c->c_val == pri) {
		      p_s = c->c_name;
		      break;
		    }
		  }
		}
		if (!f_s) {
		  snprintf(f_n, sizeof f_n, "%d", LOG_FAC(fac));
		  f_s = f_n;
		}
		if (!p_s) {
		  snprintf(p_n, sizeof p_n, "%d", pri);
		  p_s = p_n;
		}
		snprintf(fp_buf, sizeof fp_buf, "<%s.%s> ", f_s, p_s);
		v->iov_base = fp_buf;
		v->iov_len = strlen(fp_buf);
	} else {
	        v->iov_base="";
		v->iov_len = 0;
	}
	v++;

	v->iov_base = f->f_prevhost;
	v->iov_len = strlen(v->iov_base);
	v++;
	v->iov_base = " ";
	v->iov_len = 1;
	v++;

	if (msg) {
		wmsg = strdup(msg); /* XXX iov_base needs a `const' sibling. */
		if (wmsg == NULL) {
			logerror("strdup");
			exit(1);
		}
		v->iov_base = wmsg;
		v->iov_len = strlen(msg);
	} else if (f->f_prevcount > 1) {
		v->iov_base = repbuf;
		v->iov_len = snprintf(repbuf, sizeof repbuf,
		    "last message repeated %d times", f->f_prevcount);
	} else {
		v->iov_base = f->f_prevline;
		v->iov_len = f->f_prevlen;
	}
	v++;

	dprintf("Logging to %s", TypeNames[f->f_type]);
	f->f_time = now;

	switch (f->f_type) {
	case F_UNUSED:
		dprintf("\n");
		break;

	case F_FORW:
		dprintf(" %s\n", f->f_un.f_forw.f_hname);
		/* check for local vs remote messages */
		if (strcasecmp(f->f_prevhost, LocalHostName))
			l = snprintf(line, sizeof line - 1,
			    "<%d>%.15s Forwarded from %s: %s",
			    f->f_prevpri, iov[0].iov_base, f->f_prevhost,
			    iov[5].iov_base);
		else
			l = snprintf(line, sizeof line - 1, "<%d>%.15s %s",
			     f->f_prevpri, iov[0].iov_base, iov[5].iov_base);
		if (l < 0)
			l = 0;
		else if (l > MAXLINE)
			l = MAXLINE;

		if (SocketInternet) {
			for (r = f->f_un.f_forw.f_addr; r; r = r->ai_next) {
				for (i = 0; i < *SocketInternet; i++) {
#if 0 
					/*
					 * should we check AF first, or just
					 * trial and error? FWD
					 */
					if (r->ai_family ==
					    address_family_of(finet[i+1])) 
#endif
					lsent = sendto(SocketInternet[i+1], line, l, 0,
					    r->ai_addr, r->ai_addrlen);
					if (lsent == l) 
						break;
				}
				if (lsent == l && !send_to_all) 
					break;
			}
			dprintf("lsent/l: %d/%d\n", lsent, l);
			if (lsent != l) {
				int e = errno;
				logerror("sendto");
				errno = e;
				switch (errno) {
				case EHOSTUNREACH:
				case EHOSTDOWN:
					break;
				/* case EBADF: */
				/* case EACCES: */
				/* case ENOTSOCK: */
				/* case EFAULT: */
				/* case EMSGSIZE: */
				/* case EAGAIN: */
				/* case ENOBUFS: */
				/* case ECONNREFUSED: */
				default:
					dprintf("removing entry\n");
					(void)close(f->f_file);
					f->f_type = F_UNUSED;
					break;
				}
			}
		}
		break;

	case F_FILE:
		dprintf(" %s\n", f->f_un.f_fname);
		v->iov_base = "\n";
		v->iov_len = 1;
		if (writev(f->f_file, iov, 7) < 0) {
			int e = errno;
			(void)close(f->f_file);
			f->f_type = F_UNUSED;
			errno = e;
			logerror(f->f_un.f_fname);
		} else if (flags & SYNC_FILE)
			(void)fsync(f->f_file);
		break;

	case F_CONSOLE:
		if (flags & IGN_CONS) {
			dprintf(" (ignored)\n");
			break;
		}
		break;
	}
	f->f_prevcount = 0;
	if (msg)
		free(wmsg);
}

/*
 *  ParseSysLogConf -- Initialize syslogd from configuration table
 */
static void ParseSysLogConf(int CalledFromSignal)
{
	int i;
	FILE *cf;
	struct filed *f, *next, **nextp;
	char *p;
	char cline[LINE_MAX];
 	char prog[NAME_MAX+1];
	char host[MAXHOSTNAMELEN];
	char oldLocalHostName[MAXHOSTNAMELEN];
	char hostMsg[2*MAXHOSTNAMELEN+40];

	dprintf("init\n");

	/*
	 * Load hostname (may have changed).
	 */
	if (CalledFromSignal != 0)
		(void)strlcpy(oldLocalHostName, LocalHostName,
		    sizeof(oldLocalHostName));
	if (gethostname(LocalHostName, sizeof(LocalHostName)))
		err(EX_OSERR, "gethostname() failed");
	if ((p = strchr(LocalHostName, '.')) != NULL) {
		*p++ = '\0';
		LocalDomain = p;
	} else {
		LocalDomain = "";
	}

	/*
	 *  Close all open log files.
	 */
	Initialized = 0;
	for (f = Files; f != NULL; f = next) {
		/* flush any pending output */
		if (f->f_prevcount)
			fprintlog(f, 0, (char *)NULL);

		switch (f->f_type) {
		case F_FILE:
		case F_FORW:
		case F_CONSOLE:
			(void)close(f->f_file);
			break;
		}
		next = f->f_next;
		if (f->f_program) free(f->f_program);
		if (f->f_host) free(f->f_host);
		free((char *)f);
	}
	Files = NULL;
	nextp = &Files;

	/* open the configuration file */
	if ((cf = fopen(gCfgFileName, "r")) == NULL) {
		dprintf("cannot open %s\n", gCfgFileName);
		*nextp = (struct filed *)calloc(1, sizeof(*f));
		if (*nextp == NULL) {
			logerror("calloc");
			exit(1);
		}
		cfline("*.ERR\t/dev/console", *nextp, "*", "*");
		(*nextp)->f_next = (struct filed *)calloc(1, sizeof(*f));
		if ((*nextp)->f_next == NULL) {
			logerror("calloc");
			exit(1);
		}
		cfline("*.PANIC\t*", (*nextp)->f_next, "*", "*");
		Initialized = 1;
		return;
	}

	/*
	 *  Foreach line in the conf table, open that file.
	 */
	f = NULL;
	(void)strlcpy(host, "*", sizeof(host));
	(void)strlcpy(prog, "*", sizeof(prog));
	while (fgets(cline, sizeof(cline), cf) != NULL) {
		/*
		 * check for end-of-section, comments, strip off trailing
		 * spaces and newline character. #!prog is treated specially:
		 * following lines apply only to that program.
		 */
		for (p = cline; isspace(*p); ++p)
			continue;
		if (*p == 0)
			continue;
		if (*p == '#') {
			p++;
			if (*p != '!' && *p != '+' && *p != '-')
				continue;
		}
		if (*p == '+' || *p == '-') {
			host[0] = *p++;
			while (isspace(*p))
				p++;
			if ((!*p) || (*p == '*')) {
				(void)strlcpy(host, "*", sizeof(host));
				continue;
			}
			if (*p == '@')
				p = LocalHostName;
			for (i = 1; i < MAXHOSTNAMELEN - 1; i++) {
				if (!isalnum(*p) && *p != '.' && *p != '-'
                                    && *p != ',')
					break;
				host[i] = *p++;
			}
			host[i] = '\0';
			continue;
		}
		if (*p == '!') {
			p++;
			while (isspace(*p)) p++;
			if ((!*p) || (*p == '*')) {
				(void)strlcpy(prog, "*", sizeof(prog));
				continue;
			}
			for (i = 0; i < NAME_MAX; i++) {
				if (!isprint(p[i]))
					break;
				prog[i] = p[i];
			}
			prog[i] = 0;
			continue;
		}
		for (p = strchr(cline, '\0'); isspace(*--p);)
			continue;
		*++p = '\0';
		f = (struct filed *)calloc(1, sizeof(*f));
		if (f == NULL) {
			logerror("calloc");
			exit(1);
		}
		*nextp = f;
		nextp = &f->f_next;
		cfline(cline, f, prog, host);
	}

	/* close the configuration file */
	(void)fclose(cf);

	Initialized = 1;

	if (Debug) {
		for (f = Files; f; f = f->f_next) {
			for (i = 0; i <= LOG_NFACILITIES; i++)
				if (f->f_pmask[i] == INTERNAL_NOPRI)
					printf("X ");
				else
					printf("%d ", f->f_pmask[i]);
			printf("%s: ", TypeNames[f->f_type]);
			switch (f->f_type) {
			case F_FILE:
				printf("%s", f->f_un.f_fname);
				break;

			case F_CONSOLE:
				printf("%s%s", _PATH_DEV, f->f_un.f_fname);
				break;

			case F_FORW:
				printf("%s", f->f_un.f_forw.f_hname);
				break;
			}
			if (f->f_program)
				printf(" (%s)", f->f_program);
			printf("\n");
		}
	}

	logmessage(LOG_SYSLOG|LOG_INFO, "syslogd: restart", LocalHostName, ADDDATE);
	dprintf("syslogd: restarted\n");
	/*
	 * Log a change in hostname, but only on a restart.
	 */
	if (CalledFromSignal != 0 && strcmp(oldLocalHostName, LocalHostName) != 0) {
		(void)snprintf(hostMsg, sizeof(hostMsg),
		    "syslogd: hostname changed, \"%s\" to \"%s\"",
		    oldLocalHostName, LocalHostName);
		logmessage(LOG_SYSLOG|LOG_INFO, hostMsg, LocalHostName, ADDDATE);
		dprintf("%s\n", hostMsg);
	}
}

/*
 * Crack a configuration file line
 */
static void
cfline(const char *line, struct filed *f, const char *prog, const char *host)
{
	struct addrinfo hints, *res;
	int error, i, pri;
	const char *p, *q;
	char *bp;
	char buf[MAXLINE], ebuf[100];

	dprintf("cfline(\"%s\", f, \"%s\", \"%s\")\n", line, prog, host);

	errno = 0;	/* keep strerror() stuff out of logerror messages */

	/* clear out file entry */
	memset(f, 0, sizeof(*f));
	for (i = 0; i <= LOG_NFACILITIES; i++)
		f->f_pmask[i] = INTERNAL_NOPRI;

	/* save hostname if any */
	if (host && *host == '*')
		host = NULL;
	if (host) {
		int hl;

		f->f_host = strdup(host);
		if (f->f_host == NULL) {
			logerror("strdup");
			exit(1);
		}
		hl = strlen(f->f_host);
		if (hl > 0 && f->f_host[hl-1] == '.')
			f->f_host[--hl] = '\0';
//		trimdomain(f->f_host, hl);
	}

	/* save program name if any */
	if (prog && *prog == '*')
		prog = NULL;
	if (prog) {
		f->f_program = strdup(prog);
		if (f->f_program == NULL) {
			logerror("strdup");
			exit(1);
		}
	}

	/* scan through the list of selectors */
	for (p = line; *p && *p != '\t' && *p != ' ';) {
		int pri_done;
		int pri_cmp;
		int pri_invert;

		/* find the end of this facility name list */
		for (q = p; *q && *q != '\t' && *q != ' ' && *q++ != '.'; )
			continue;

		/* get the priority comparison */
		pri_cmp = 0;
		pri_done = 0;
		pri_invert = 0;
		if (*q == '!') {
			pri_invert = 1;
			q++;
		}
		while (!pri_done) {
			switch (*q) {
			case '<':
				pri_cmp |= PRI_LT;
				q++;
				break;
			case '=':
				pri_cmp |= PRI_EQ;
				q++;
				break;
			case '>':
				pri_cmp |= PRI_GT;
				q++;
				break;
			default:
				pri_done++;
				break;
			}
		}

		/* collect priority name */
		for (bp = buf; *q && !strchr("\t,; ", *q); )
			*bp++ = *q++;
		*bp = '\0';

		/* skip cruft */
		while (strchr(",;", *q))
			q++;

		/* decode priority name */
		if (*buf == '*') {
			pri = LOG_PRIMASK + 1;
			pri_cmp = PRI_LT | PRI_EQ | PRI_GT;
		} else {
			pri = decode(buf, prioritynames);
			if (pri < 0) {
				(void)snprintf(ebuf, sizeof ebuf,
				    "unknown priority name \"%s\"", buf);
				logerror(ebuf);
				return;
			}
		}
		if (!pri_cmp)
			pri_cmp = (UniquePriority)
				  ? (PRI_EQ)
				  : (PRI_EQ | PRI_GT)
				  ;
		if (pri_invert)
			pri_cmp ^= PRI_LT | PRI_EQ | PRI_GT;

		/* scan facilities */
		while (*p && !strchr("\t.; ", *p)) {
			for (bp = buf; *p && !strchr("\t,;. ", *p); )
				*bp++ = *p++;
			*bp = '\0';

			if (*buf == '*') {
				for (i = 0; i < LOG_NFACILITIES; i++) {
					f->f_pmask[i] = pri;
					f->f_pcmp[i] = pri_cmp;
				}
			} else {
				i = decode(buf, facilitynames);
				if (i < 0) {
					(void)snprintf(ebuf, sizeof ebuf,
					    "unknown facility name \"%s\"",
					    buf);
					logerror(ebuf);
					return;
				}
				f->f_pmask[i >> 3] = pri;
				f->f_pcmp[i >> 3] = pri_cmp;
			}
			while (*p == ',' || *p == ' ')
				p++;
		}

		p = q;
	}

	/* skip to action part */
	while (*p == '\t' || *p == ' ')
		p++;

	switch (*p) {
	case '@':
		(void)strlcpy(f->f_un.f_forw.f_hname, ++p,
			sizeof(f->f_un.f_forw.f_hname));
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = family;
		hints.ai_socktype = SOCK_DGRAM;
		error = getaddrinfo(f->f_un.f_forw.f_hname, "syslog", &hints,
				    &res);
		if (error) {
			logerror(gai_strerror(error));
			break;
		}
		f->f_un.f_forw.f_addr = res;
		f->f_type = F_FORW;
		break;

	default:
//	case '/':
		if ((f->f_file = open(p, O_WRONLY|O_APPEND|O_CREAT, "rwx")) < 0) {
			f->f_type = F_UNUSED;
			logerror(p);
			break;
		}
		if (isatty(f->f_file)) {
			if (strcmp(p, gCfgFileName) == 0)
				f->f_type = F_CONSOLE;
			(void)strlcpy(f->f_un.f_fname, p + sizeof(_PATH_DEV) - 1,
			    sizeof(f->f_un.f_fname));
		} else {
			(void)strlcpy(f->f_un.f_fname, p, sizeof(f->f_un.f_fname));
			f->f_type = F_FILE;
		}
		break;
	}
}

/*
 * Return a printable representation of a host address.
 */
static const char *
cvthname(struct sockaddr *f)
{
	static char hname[NI_MAXHOST], ip[NI_MAXHOST];
/*
	int error, hl;
	sigset_t omask, nmask;
*/

/* hostname stuff doesnt work

	error = getnameinfo((struct sockaddr *)f,
#if HAVE_SOCKADDR_LEN
			    ((struct sockaddr *)f)->sa_len,
#else
				0,
#endif
			    ip, sizeof ip, NULL, 0,
			    NI_NUMERICHOST | withscopeid);
	dprintf("cvthname(%s)\n", ip);

	if (error) {
		dprintf("Malformed from address %s\n", gai_strerror(error));
		return ("???");
	}
	if (!resolve)
		return (ip);

	sigemptyset(&nmask);
	sigaddset(&nmask, SIGHUP);
	sigprocmask(SIG_BLOCK, &nmask, &omask);
	error = getnameinfo((struct sockaddr *)f,
#if HAVE_SOCKADDR_LEN
			    ((struct sockaddr *)f)->sa_len,
#else
				0,
#endif
			    hname, sizeof hname, NULL, 0,
			    NI_NAMEREQD | withscopeid);
	sigprocmask(SIG_SETMASK, &omask, NULL);
	if (error) {
		dprintf("Host name for your address (%s) unknown\n", ip);
		return (ip);
	}
	hl = strlen(hname);
	if (hl > 0 && hname[hl-1] == '.')
		hname[--hl] = '\0';
	trimdomain(hname, hl);

*/
	struct sockaddr_in f_;
	memcpy(&f_,f,sizeof(struct sockaddr_in));
	sprintf(hname,"%d.%d.%d.%d",f_.sin_addr.S_un.S_un_b.s_b1,
								f_.sin_addr.S_un.S_un_b.s_b2,
								f_.sin_addr.S_un.S_un_b.s_b3,
								f_.sin_addr.S_un.S_un_b.s_b4);

	return (hname);
}

static void domark(int signo)
{
#pragma unused(signo)

	MarkSet = 1;
}

/*
 * Print syslogd errors some place.
 */
static void
logerror(const char *type)
{
	char buf[512];
	static int recursed = 0;

	/* If there's an error while trying to log an error, give up. */
	if (recursed)
		return;
	recursed++;
	if (errno)
		(void)snprintf(buf,
		    sizeof buf, "syslogd: %s: %s", type, strerror(errno));
	else
		(void)snprintf(buf, sizeof buf, "syslogd: %s", type);
	errno = 0;
	dprintf("%s\n", buf);
	logmessage(LOG_SYSLOG|LOG_ERR, buf, LocalHostName, ADDDATE);
	recursed--;
}

static void
die(int signo)
{
	struct filed *f;
	int was_initialized;
	char buf[100];

	was_initialized = Initialized;
	Initialized = 0;	/* Don't log SIGCHLDs. */
	
	for (f = Files; f != NULL; f = f->f_next)
	{
		// flush any pending output
		if (f->f_prevcount)
			fprintlog(f, 0, (char *)NULL);
	}
	Initialized = was_initialized;
	if (signo)
	{
		dprintf("syslogd: exiting on signal %d\n", signo);
		(void)snprintf(buf, sizeof(buf), "exiting on signal %d", signo);
		errno = 0;
		logerror(buf);
	}

	if (SocketInternet)
		free(SocketInternet);

	exit(1);
}


/*
 *  Decode a symbolic name to a numeric value
 */
static int
decode(const char *name, CODE *codetab)
{
	CODE *c;
	char *p, buf[40];

	if (isdigit(*name))
		return (atoi(name));

	for (p = buf; *name && p < &buf[sizeof(buf) - 1]; p++, name++) {
		if (isupper(*name))
			*p = tolower(*name);
		else
			*p = *name;
	}
	*p = '\0';
	for (c = codetab; c->c_name; c++)
		if (!strcmp(buf, c->c_name))
			return (c->c_val);

	return (-1);
}

static void
LogDoMark(void)
{
	struct filed *f;

	now = time((time_t *)NULL);
	MarkSeq += TIMERINTVL;
	if (MarkSeq >= MarkInterval) {
		logmessage(LOG_INFO, "-- MARK --",
		    LocalHostName, ADDDATE|MARK);
		MarkSeq = 0;
	}

	for (f = Files; f; f = f->f_next) {
		if (f->f_prevcount && now >= REPEATTIME(f)) {
			dprintf("flush %s: repeated %d times, %d sec.\n",
			    TypeNames[f->f_type], f->f_prevcount,
			    repeatinterval[f->f_repeatcount]);
			fprintlog(f, 0, (char *)NULL);
			BACKOFF(f);
		}
	}
}

/*
 * Add `s' to the list of allowable peer addresses to accept messages
 * from.
 *
 * `s' is a string in the form:
 *
 *    [*]domainname[:{servicename|portnumber|*}]
 *
 * or
 *
 *    netaddr/maskbits[:{servicename|portnumber|*}]
 *
 * Returns -1 on error, 0 if the argument was valid.
 */
static int
allowaddr(char *s)
{
	char *cp1, *cp2;
	struct allowedpeer ap;
	struct servent *se;
	int masklen = -1;
	struct addrinfo hints, *res;
	struct in_addr *addrp, *maskp;
//	u_int32_t *addr6p, *mask6p;
//	char ip[NI_MAXHOST];

#ifdef INET6
	if (*s != '[' || (cp1 = strchr(s + 1, ']')) == NULL)
#endif
		cp1 = s;
	if ((cp1 = strrchr(cp1, ':'))) {
		/* service/port provided */
		*cp1++ = '\0';
		if (strlen(cp1) == 1 && *cp1 == '*')
			/* any port allowed */
			ap.port = 0;
		else if ((se = getservbyname(cp1, "udp"))) {
			ap.port = ntohs(se->s_port);
		} else {
			ap.port = strtol(cp1, &cp2, 0);
			if (*cp2 != '\0')
				return (-1); /* port not numeric */
		}
	} else {
		if ((se = getservbyname("syslog", "udp")))
			ap.port = ntohs(se->s_port);
		else
			/* sanity, should not happen */
			ap.port = 514;
	}

	if ((cp1 = strchr(s, '/')) != NULL &&
	    strspn(cp1 + 1, "0123456789") == strlen(cp1 + 1)) {
		*cp1 = '\0';
		if ((masklen = atoi(cp1 + 1)) < 0)
			return (-1);
	}
#ifdef INET6
	if (*s == '[') {
		cp2 = s + strlen(s) - 1;
		if (*cp2 == ']') {
			++s;
			*cp2 = '\0';
		} else {
			cp2 = NULL;
		}
	} else {
		cp2 = NULL;
	}
#endif
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;

	if (getaddrinfo(s, NULL, &hints, &res) == 0) {
		ap.isnumeric = 1;
		memcpy(&ap.a_addr, res->ai_addr, res->ai_addrlen);
		memset(&ap.a_mask, 0, sizeof(ap.a_mask));
		ap.a_mask.ss_family = res->ai_family;
		if (res->ai_family == AF_INET) {
			ap.a_mask.ss_len = sizeof(struct sockaddr_in);
			maskp = &((struct sockaddr_in *)&ap.a_mask)->sin_addr;
			addrp = &((struct sockaddr_in *)&ap.a_addr)->sin_addr;
			if (masklen < 0) {
				/* use default netmask */
				if (IN_CLASSA(ntohl(addrp->s_addr)))
					maskp->s_addr = htonl(IN_CLASSA_NET);
				else if (IN_CLASSB(ntohl(addrp->s_addr)))
					maskp->s_addr = htonl(IN_CLASSB_NET);
				else
					maskp->s_addr = htonl(IN_CLASSC_NET);
			} else if (masklen <= 32) {
				/* convert masklen to netmask */
				if (masklen == 0)
					maskp->s_addr = 0;
				else
					maskp->s_addr = htonl(~((1 << (32 - masklen)) - 1));
			} else {
				freeaddrinfo(res);
				return (-1);
			}
			/* Lose any host bits in the network number. */
			addrp->s_addr &= maskp->s_addr;
		}
#ifdef INET6
		else if (res->ai_family == AF_INET6 && masklen <= 128) {
			ap.a_mask.ss_len = sizeof(struct sockaddr_in6);
			if (masklen < 0)
				masklen = 128;
			mask6p = (u_int32_t *)&((struct sockaddr_in6 *)&ap.a_mask)->sin6_addr;
			/* convert masklen to netmask */
			while (masklen > 0) {
				if (masklen < 32) {
					*mask6p = htonl(~(0xffffffff >> masklen));
					break;
				}
				*mask6p++ = 0xffffffff;
				masklen -= 32;
			}
			/* Lose any host bits in the network number. */
			mask6p = (u_int32_t *)&((struct sockaddr_in6 *)&ap.a_mask)->sin6_addr;
			addr6p = (u_int32_t *)&((struct sockaddr_in6 *)&ap.a_addr)->sin6_addr;
			for (i = 0; i < 4; i++)
				addr6p[i] &= mask6p[i];
		}
#endif
		else {
			freeaddrinfo(res);
			return (-1);
		}
		freeaddrinfo(res);
	} else {
		/* arg `s' is domain name */
		ap.isnumeric = 0;
		ap.a_name = s;
		if (cp1)
			*cp1 = '/';
#ifdef INET6
		if (cp2) {
			*cp2 = ']';
			--s;
		}
#endif
	}

	if ((AllowedPeers = realloc(AllowedPeers,
				    ++NumAllowed * sizeof(struct allowedpeer)))
	    == NULL) {
		logerror("realloc");
		exit(1);
	}
	memcpy(&AllowedPeers[NumAllowed - 1], &ap, sizeof(struct allowedpeer));
	return (0);
}

/*
 * Validate that the remote peer has permission to log to us.
 */
static int
ValidatePeerPermissions(struct sockaddr *sa, const char *hname)
{
	int						i;
//	int	j, reject;
	size_t					l1, l2;
	char						*cp, name[NI_MAXHOST], ip[NI_MAXHOST], port[NI_MAXSERV];
	struct allowedpeer	*ap;
	struct sockaddr_in	*sin4, *a4p = NULL, *m4p = NULL;
//	struct sockaddr_in6	*sin6;
	struct sockaddr_in6	*a6p = NULL, *m6p = NULL;
	struct addrinfo		hints, *res;
	u_short					sport;

	if (NumAllowed == 0)
		/* traditional behaviour, allow everything */
		return (1);

	(void)strlcpy(name, hname, sizeof(name));
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;
	if (getaddrinfo(name, NULL, &hints, &res) == 0)
		freeaddrinfo(res);
	else if (strchr(name, '.') == NULL) {
		strlcat(name, ".", sizeof name);
		strlcat(name, LocalDomain, sizeof name);
	}
	if (getnameinfo(sa, 
#if HAVE_SOCKADDR_LEN
				sa->sa_len, 
#else
				0,
#endif
			ip, sizeof ip, port, sizeof port,
			NI_NUMERICHOST | withscopeid | NI_NUMERICSERV) != 0)
		return (0);	/* for safety, should not occur */
	dprintf("validate: dgram from IP %s, port %s, name %s;\n",
		ip, port, name);
	sport = atoi(port);

	/* now, walk down the list */
	for (i = 0, ap = AllowedPeers; i < NumAllowed; i++, ap++) {
		if (ap->port != 0 && ap->port != sport) {
			dprintf("rejected in rule %d due to port mismatch.\n", i);
			continue;
		}

		if (ap->isnumeric) {
			if (ap->a_addr.ss_family != sa->sa_family) {
				dprintf("rejected in rule %d due to address family mismatch.\n", i);
				continue;
			}
			if (ap->a_addr.ss_family == AF_INET) {
				sin4 = (struct sockaddr_in *)sa;
				a4p = (struct sockaddr_in *)&ap->a_addr;
				m4p = (struct sockaddr_in *)&ap->a_mask;
				if ((sin4->sin_addr.s_addr & m4p->sin_addr.s_addr)
				    != a4p->sin_addr.s_addr) {
					dprintf("rejected in rule %d due to IP mismatch.\n", i);
					continue;
				}
			}
#ifdef INET6
			else if (ap->a_addr.ss_family == AF_INET6) {
				sin6 = (struct sockaddr_in6 *)sa;
				a6p = (struct sockaddr_in6 *)&ap->a_addr;
				m6p = (struct sockaddr_in6 *)&ap->a_mask;
#ifdef NI_WITHSCOPEID
				if (a6p->sin6_scope_id != 0 &&
				    sin6->sin6_scope_id != a6p->sin6_scope_id) {
					dprintf("rejected in rule %d due to scope mismatch.\n", i);
					continue;
				}
#endif
				reject = 0;
				for (j = 0; j < 16; j += 4) {
					if ((*(u_int32_t *)&sin6->sin6_addr.s6_addr[j] & *(u_int32_t *)&m6p->sin6_addr.s6_addr[j])
					    != *(u_int32_t *)&a6p->sin6_addr.s6_addr[j]) {
						++reject;
						break;
					}
				}
				if (reject) {
					dprintf("rejected in rule %d due to IP mismatch.\n", i);
					continue;
				}
			}
#endif
			else
				continue;
		} else {
			cp = ap->a_name;
			l1 = strlen(name);
			if (*cp == '*') {
				/* allow wildmatch */
				cp++;
				l2 = strlen(cp);
				if (l2 > l1 || memcmp(cp, &name[l1 - l2], l2) != 0) {
					dprintf("rejected in rule %d due to name mismatch.\n", i);
					continue;
				}
			} else {
				/* exact match */
				l2 = strlen(cp);
				if (l2 != l1 || memcmp(cp, name, l1) != 0) {
					dprintf("rejected in rule %d due to name mismatch.\n", i);
					continue;
				}
			}
		}
		dprintf("accepted in rule %d.\n", i);
		return (1);	/* hooray! */
	}
	return (0);
}

int	*s_socks_base;

static int *socksetup(int af, const char *bindhostname)
{
	struct addrinfo hints, *res, *r;
	int error, maxs, *s, *socks;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = af;
	hints.ai_socktype = SOCK_DGRAM;
	error = getaddrinfo(bindhostname, "syslog", &hints, &res);
	if (error) {
		logerror(gai_strerror(error));
		errno = 0;
		die(0);
	}

	/* Count max number of sockets we may open */
	for (maxs = 0, r = res; r; r = r->ai_next, maxs++)
		;

	socks = s_socks_base = malloc((maxs+1) * sizeof(int));

	if (socks == NULL) {
		logerror("couldn't allocate memory for sockets");
		die(0);
	}

	*socks = 0;   /* num of sockets counter at start of array */
	s = socks + 1;
	for (r = res; r; r = r->ai_next) {
		*s = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
		if (*s < 0) {
			logerror("socket");
			continue;
		}
#ifdef INET6
		if (r->ai_family == AF_INET6) {
			int on = 1;
			if (setsockopt(*s, IPPROTO_IPV6, IPV6_V6ONLY,
				       (char *)&on, sizeof (on)) < 0) {
				logerror("setsockopt");
				close(*s);
				continue;
			}
		}
#endif
		if (bind(*s, r->ai_addr, r->ai_addrlen) < 0) {
			close(*s);
			logerror("bind");
			continue;
		}

		(*socks)++;
		s++;
	}

	if (*socks == 0)
	{
		free(socks);
		s_socks_base = NULL;
		if (Debug)
			return (NULL);
		else
			die(0);
	}
	if (res)
		freeaddrinfo(res);

	return (socks);
}

void FreeUndisposedMemory( void )
{
	if (s_socks_base)
		free(s_socks_base);

	if (s_fdsr_base)
		free(s_fdsr_base);
}

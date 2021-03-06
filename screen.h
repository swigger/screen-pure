/* Copyright (c) 2008, 2009
 *      Juergen Weigert (jnweiger@immd4.informatik.uni-erlangen.de)
 *      Michael Schroeder (mlschroe@immd4.informatik.uni-erlangen.de)
 *      Micah Cowan (micah@cowan.name)
 *      Sadrul Habib Chowdhury (sadrul@users.sourceforge.net)
 * Copyright (c) 1993-2002, 2003, 2005, 2006, 2007
 *      Juergen Weigert (jnweiger@immd4.informatik.uni-erlangen.de)
 *      Michael Schroeder (mlschroe@immd4.informatik.uni-erlangen.de)
 * Copyright (c) 1987 Oliver Laumann
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see the file COPYING); if not, see
 * https://www.gnu.org/licenses/, or contact Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 ****************************************************************
 * $Id$ GNU
 */

#pragma once
#include "os.h"


#include "osdef.h"

#include "ansi.h"
#include "sched.h"
#include "acls.h"
#include "comm.h"
#include "layer.h"
#include "term.h"


# define STATIC static

# define debugf(a)       do {} while (0)
# define debug(x)        debugf(x)
# define debug1(x,a)     debugf(x)
# define debug2(x,a,b)   debugf(x)
# define debug3(x,a,b,c) debugf(x)

# define NOASSERT

# define ASSERT(lousy_cpp) do {} while (0)

/* here comes my own Free: jw. */
#define Free(a) {if ((a) == 0) abort(); else free((void *)(a)); (a)=0;}

#define Ctrl(c) ((c)&037)

#define MAXSTR		768
#define MAXARGS 	64
#define MSGWAIT 	5
#define MSGMINWAIT 	1
#define SILENCEWAIT	30

/*
 * if a nasty user really wants to try a history of 3000 lines on all 10
 * windows, he will allocate 8 MegaBytes of memory, which is quite enough.
 */
#define MAXHISTHEIGHT		3000
#define DEFAULTHISTHEIGHT	100
# define DEFAULT_BUFFERFILE	"/tmp/screen-exchange"

/*
 * Define PATH_MAX to 4096 if it's not defined, like on GNU/Hurd
 */



struct mode
{
  struct termios tio;
};


/* #include "logfile.h" */	/* (requires stat.h) struct logfile */
#include "image.h"
#include "canvas.h"
#include "display.h"
#include "window.h"

/*
 * Parameters for the Detach() routine
 */
#define D_DETACH	0
#define D_STOP		1
#define D_REMOTE	2
#define D_POWER 	3
#define D_REMOTE_POWER	4
#define D_LOCK		5
#define D_HANGUP	6

/*
 * Here are the messages the attacher sends to the backend
 */
#define MSG_CREATE	0
#define MSG_ERROR	1
#define MSG_ATTACH	2
#define MSG_CONT	3
#define MSG_DETACH	4
#define MSG_POW_DETACH	5
#define MSG_WINCH	6
#define MSG_HANGUP	7
#define MSG_COMMAND	8
#define MSG_QUERY       9

/*
 * versions of struct msg:
 * 0:	screen version 3.6.6 (version count introduced)
 * 1:	screen version 4.1.0devel	(revisions e3fc19a upto 8147d08)
 * 					 A few revisions after 8147d08 incorrectly
 * 					 carried version 1, but should have carried 2.
 * 2:	screen version 4.1.0devel	(revisions 8b46d8a upto YYYYYYY)
 * 3:	screen version 4.2.0		(was incorrectly originally. Patched here)
 * 4:	screen version 4.2.1		(bumped once again due to changed terminal and login length)
 * 5: 	screen version 4.4.0		(fix screenterm size)
 */
#define MSG_VERSION	2

#define MSG_REVISION	(('m'<<24) | ('s'<<16) | ('g'<<8) | MSG_VERSION)
struct msg
{
  int protocol_revision;	/* reduce harm done by incompatible messages */
  int type;
  char m_tty[MAXPATHLEN];	/* ttyname */
  union
    {
      struct
	{
	  int lflag;
	  int aflag;
	  int flowflag;
	  int hheight;		/* size of scrollback buffer */
	  int nargs;
	  char line[MAXPATHLEN];
	  char dir[MAXPATHLEN];
	  char screenterm[20];	/* is screen really "screen" ? */
	}
      create;
      struct
	{
	  char auser[MAXLOGINLEN + 1];	/* username */
	  int apid;		/* pid of frontend */
	  int adaptflag;	/* adapt window size? */
	  int lines, columns;	/* display size */
	  char preselect[20];
	  int esc;		/* his new escape character unless -1 */
	  int meta_esc;		/* his new meta esc character unless -1 */
	  char envterm[MAXTERMLEN + 1];	/* terminal type */
	  int encoding;		/* encoding of display */
	  int detachfirst;      /* whether to detach remote sessions first */
	}
      attach;
      struct 
	{
	  char duser[MAXLOGINLEN + 1];	/* username */
	  int dpid;		/* pid of frontend */
	}
      detach;
      struct 
	{
	  char auser[MAXLOGINLEN + 1];	/* username */
	  int nargs;
	  char cmd[MAXPATHLEN];	/* command */
	  int apid;		/* pid of frontend */
	  char preselect[20];
	  char writeback[MAXPATHLEN];  /* The socket to write the result.
					  Only used for MSG_QUERY */
	}
      command;
      char message[MAXPATHLEN * 2];
    } m;
};

/*
 * And the signals the attacher receives from the backend
 */
#define SIG_BYE		SIGHUP
#define SIG_POWER_BYE	SIGUSR1
#define SIG_LOCK	SIGUSR2
#define SIG_STOP	SIGTSTP
#define SIG_NODEBUG	SIGIO		/* triggerd by command 'debug off' */


#define BELL		(Ctrl('g'))
#define VBELLWAIT	1 /* No. of seconds a vbell will be displayed */

#define BELL_ON		0 /* No bell has occurred in the window */
#define BELL_FOUND	1 /* A bell has occurred, but user not yet notified */
#define BELL_DONE	2 /* A bell has occured, user has been notified */

#define BELL_VISUAL	3 /* A bell has occured in fore win, notify him visually */

#define MON_OFF 	0 /* Monitoring is off in the window */
#define MON_ON		1 /* No activity has occurred in the window */
#define MON_FOUND	2 /* Activity has occured, but user not yet notified */
#define MON_DONE	3 /* Activity has occured, user has been notified */

#define DUMP_TERMCAP	0 /* WriteFile() options */
#define DUMP_HARDCOPY	1
#define DUMP_EXCHANGE	2
#define DUMP_SCROLLBACK 3

#define SILENCE_OFF	0 /* Not checking for silence */
#define SILENCE_ON	1 /* Window being monitored for silence */
#define SILENCE_FOUND   2 /* Window is silent */
#define SILENCE_DONE    3 /* Window is silent and user is notified */

extern char strnomem[];

/*
 * line modes used by Input()
 */
#define INP_COOKED	0
#define INP_NOECHO	1
#define INP_RAW		2
#define INP_EVERY	4


struct acl
{
  struct acl *next;
  char *name;
};

/* register list */
#define MAX_PLOP_DEFS 256

struct baud_values
{
  int idx;	/* the index in the bsd-is padding lookup table */
  int bps;	/* bits per seconds */
  int sym;	/* symbol defined in ttydev.h */
};

/*
 * windowlist orders
 */
#define WLIST_NUM 0
#define WLIST_MRU 1
#define WLIST_NESTED 2

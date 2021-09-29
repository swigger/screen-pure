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
 */

/*
 * NOTICE: tty.c is automatically generated from tty.sh
 * Do not change anything here. If you then change tty.sh.
 */

#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
# include <sys/file.h>
# include <sys/ioctl.h> /* collosions with termios.h */
#include <limits.h>


#include "config.h"

#include "screen.h"
#include "extern.h"


extern struct display *display, *displays;
extern int iflag;
extern struct win *console_window;
static void consredir_readev_fn __P((struct event *, char *));

int separate_sids = 1;

static void DoSendBreak __P((int, int, int));
static sigret_t SigAlrmDummy __P(SIGPROTOARG);


/* Frank Schulz (fschulz@pyramid.com):
 * I have no idea why VSTART is not defined and my fix is probably not
 * the cleanest, but it works.
 */




static sigret_t
SigAlrmDummy SIGDEFARG
{
  debug("SigAlrmDummy()\n");
  SIGRETURN;
}

/*
 *  Carefully open a charcter device. Not used to open display ttys.
 *  The second parameter is parsed for a few stty style options.
 */

int OpenTTY(char *line, char *opt)
{
  int f;
  struct mode Mode;
  sigret_t (*sigalrm)__P(SIGPROTOARG);

  sigalrm = signal(SIGALRM, SigAlrmDummy);
  alarm(2);

  /* this open only succeeds, if real uid is allowed */
  if ((f = secopen(line, O_RDWR | O_NONBLOCK | O_NOCTTY, 0)) == -1) {
    if (errno == EINTR)
      Msg(0, "Cannot open line '%s' for R/W: open() blocked, aborted.", line);
    else
      Msg(errno, "Cannot open line '%s' for R/W", line);

    alarm(0);
    signal(SIGALRM, sigalrm);
    return -1;
  }

  if (!isatty(f)) {
    Msg(0, "'%s' is not a tty", line);
    alarm(0);
    signal(SIGALRM, sigalrm);
    close(f);
    return -1;
  }

  /*
   * We come here exclusively. This is to stop all kermit and cu type things
   * accessing the same tty line.
   * Perhaps we should better create a lock in some /usr/spool/locks directory?
   */
 errno = 0;
 if (ioctl(f, TIOCEXCL, (char *) 0) < 0)
   Msg(errno, "%s: ioctl TIOCEXCL failed", line);
 debug3("%d %d %d\n", getuid(), geteuid(), getpid());
 debug2("%s TIOCEXCL errno %d\n", line, errno);
  /*
   * We create a sane tty mode. We do not copy things from the display tty
   */
    InitTTY(&Mode, W_TYPE_PLAIN);
  
  SttyMode(&Mode, opt);
  SetTTY(f, &Mode);

  {
    int mcs = 0;
    ioctl(f, TIOCMGET, &mcs);
    mcs |= TIOCM_RTS;
    ioctl(f, TIOCMSET, &mcs);
  }

  brktty(f);
  alarm(0);
  signal(SIGALRM, sigalrm);
  debug2("'%s' CONNECT fd=%d.\n", line, f);
  return f;
}


/*
 *  Tty mode handling
 */

void InitTTY(struct mode *m, int ttyflag)
{
  bzero((char *)m, sizeof(*m));
  /* struct termios tio 
   * defaults, as seen on SunOS 4.1.3
   */
  debug1("InitTTY: POSIX: termios defaults based on SunOS 4.1.3, but better (%d)\n", ttyflag);
	m->tio.c_iflag |= BRKINT;
	m->tio.c_iflag |= IGNPAR;
/* IF{ISTRIP}	m->tio.c_iflag |= ISTRIP;  may be needed, let's try. jw. */
	m->tio.c_iflag |= IXON;
/* IF{IMAXBEL}	m->tio.c_iflag |= IMAXBEL; sorry, this one is ridiculus. jw */

  if (!ttyflag)	/* may not even be good for ptys.. */
    {
	m->tio.c_iflag |= ICRNL;
	m->tio.c_oflag |= ONLCR; 
	m->tio.c_oflag |= TAB3; 
/* IF{PARENB}	m->tio.c_cflag |= PARENB;	nah! jw. */
	m->tio.c_oflag |= OPOST;
    }


/*
 * Or-ing the speed into c_cflags is dangerous.
 * It breaks on bsdi, where c_ispeed and c_ospeed are extra longs.
 *
 * IF{B9600}    m->tio.c_cflag |= B9600;
 * IF{IBSHIFT) && defined(B9600}        m->tio.c_cflag |= B9600 << IBSHIFT;
 *
 * We hope that we have the posix calls to do it right:
 * If these are not available you might try the above.
 */
       cfsetospeed(&m->tio, B9600);
       cfsetispeed(&m->tio, B9600);

 	m->tio.c_cflag |= CS8;
	m->tio.c_cflag |= CREAD;
	m->tio.c_cflag |= CLOCAL;

	m->tio.c_lflag |= ECHOCTL;
	m->tio.c_lflag |= ECHOKE;

  if (!ttyflag)
    {
	m->tio.c_lflag |= ISIG;
	m->tio.c_lflag |= ICANON;
	m->tio.c_lflag |= ECHO;
    }
	m->tio.c_lflag |= ECHOE;
	m->tio.c_lflag |= ECHOK;
	m->tio.c_lflag |= IEXTEN;

	m->tio.c_cc[VINTR]    = Ctrl('C');
	m->tio.c_cc[VQUIT]    = Ctrl('\\');
	m->tio.c_cc[VERASE]   = 0x7f; /* DEL */
	m->tio.c_cc[VKILL]    = Ctrl('U');
	m->tio.c_cc[VEOF]     = Ctrl('D');
	m->tio.c_cc[VEOL]     = VDISABLE;
	m->tio.c_cc[VEOL2]    = VDISABLE;
	m->tio.c_cc[VSTART]   = Ctrl('Q');
	m->tio.c_cc[VSTOP]    = Ctrl('S');
	m->tio.c_cc[VSUSP]    = Ctrl('Z');
	m->tio.c_cc[VREPRINT] = Ctrl('R');
	m->tio.c_cc[VDISCARD] = Ctrl('O');
	m->tio.c_cc[VWERASE]  = Ctrl('W');
	m->tio.c_cc[VLNEXT]   = Ctrl('V');

  if (ttyflag)
    {
      m->tio.c_cc[VMIN] = TTYVMIN;
      m->tio.c_cc[VTIME] = TTYVTIME;
    }



}

void SetTTY(int fd, struct mode *mp)
{
  errno = 0;
  tcsetattr(fd, TCSADRAIN, &mp->tio);
  if (errno)
    Msg(errno, "SetTTY (fd %d): ioctl failed", fd);
}

void GetTTY(int fd, struct mode *mp)
{
  errno = 0;
  tcgetattr(fd, &mp->tio);
  if (errno)
    Msg(errno, "GetTTY (fd %d): ioctl failed", fd);
}

/*
 * needs interrupt = iflag and flow = d->d_flow
 */
void SetMode(struct mode *op, struct mode *np, int flow, int interrupt)
{
  *np = *op;

  ASSERT(display);
  np->tio.c_iflag &= ~ICRNL;
  np->tio.c_iflag &= ~ISTRIP;
  np->tio.c_oflag &= ~ONLCR;
  np->tio.c_lflag &= ~(ICANON | ECHO);
  /*
   * From Andrew Myers (andru@tonic.lcs.mit.edu)
   * to avoid ^V^V-Problem on OSF1
   */
  np->tio.c_lflag &= ~IEXTEN;

  /*
   * Unfortunately, the master process never will get SIGINT if the real
   * terminal is different from the one on which it was originaly started
   * (process group membership has not been restored or the new tty could not
   * be made controlling again). In my solution, it is the attacher who
   * receives SIGINT (because it is always correctly associated with the real
   * tty) and forwards it to the master [kill(MasterPid, SIGINT)]. 
   * Marc Boucher (marc@CAM.ORG)
   */
  if (interrupt)
    np->tio.c_lflag |= ISIG;
  else
    np->tio.c_lflag &= ~ISIG;
  /* 
   * careful, careful catche monkey..
   * never set VMIN and VTIME to zero, if you want blocking io.
   *
   * We may want to do a VMIN > 0, VTIME > 0 read on the ptys too, to 
   * reduce interrupt frequency.  But then we would not know how to 
   * handle read returning 0. jw.
   */
  np->tio.c_cc[VMIN] = 1;
  np->tio.c_cc[VTIME] = 0;
  if (!interrupt || !flow)
    np->tio.c_cc[VINTR] = VDISABLE;
  np->tio.c_cc[VQUIT] = VDISABLE;
  if (flow == 0)
    {
	np->tio.c_cc[VSTART] = VDISABLE;
	np->tio.c_cc[VSTOP] = VDISABLE;
      np->tio.c_iflag &= ~IXON;
    }
	np->tio.c_cc[VDISCARD] = VDISABLE;
	np->tio.c_cc[VLNEXT] = VDISABLE;
	np->tio.c_cc[VSUSP] = VDISABLE;
 /* Set VERASE to DEL, rather than VDISABLE, to avoid libvte
    "autodetect" issues. */
	np->tio.c_cc[VERASE] = 0x7f;
	np->tio.c_cc[VKILL] = VDISABLE;
	np->tio.c_cc[VREPRINT] = VDISABLE;
	np->tio.c_cc[VWERASE] = VDISABLE;
}

/* operates on display */
void SetFlow(int on)
{
  ASSERT(display);
  if (D_flow == on)
    return;
  if (on)
    {
      D_NewMode.tio.c_cc[VINTR] = iflag ? D_OldMode.tio.c_cc[VINTR] : VDISABLE;
	D_NewMode.tio.c_cc[VSTART] = D_OldMode.tio.c_cc[VSTART];
	D_NewMode.tio.c_cc[VSTOP] = D_OldMode.tio.c_cc[VSTOP];
      D_NewMode.tio.c_iflag |= D_OldMode.tio.c_iflag & IXON;
    }
  else
    {
      D_NewMode.tio.c_cc[VINTR] = VDISABLE;
	D_NewMode.tio.c_cc[VSTART] = VDISABLE;
	D_NewMode.tio.c_cc[VSTOP] = VDISABLE;
      D_NewMode.tio.c_iflag &= ~IXON;
    }
  if (!on)
    tcflow(D_userfd, TCOON);
  if (tcsetattr(D_userfd, TCSANOW, &D_NewMode.tio))
    debug1("SetFlow: ioctl errno %d\n", errno);
  D_flow = on;
}

/* parse commands from opt and modify m */
int SttyMode(struct mode *m, char *opt)
{
  static const char sep[] = " \t:;,";

  if (!opt)
    return 0;

  while (*opt) {
    while (index(sep, *opt)) opt++;
      if (*opt >= '0' && *opt <= '9') {
        if (SetBaud(m, atoi(opt), atoi(opt)))
	      return -1;
      }

      else if (!strncmp("cs7", opt, 3)) {
	  m->tio.c_cflag &= ~CSIZE;
	  m->tio.c_cflag |= CS7;
      }

      else if (!strncmp("cs8", opt, 3)) {
	  m->tio.c_cflag &= ~CSIZE;
	  m->tio.c_cflag |= CS8;
      }

      else if (!strncmp("istrip", opt, 6)) {
	  m->tio.c_iflag |= ISTRIP;
      }

      else if (!strncmp("-istrip", opt, 7)) {
	  m->tio.c_iflag &= ~ISTRIP;
      }

      else if (!strncmp("ixon", opt, 4)) {
	  m->tio.c_iflag |= IXON;
      }

      else if (!strncmp("-ixon", opt, 5)) {
	  m->tio.c_iflag &= ~IXON;
      }

      else if (!strncmp("ixoff", opt, 5)) {
	  m->tio.c_iflag |= IXOFF;
      }

      else if (!strncmp("-ixoff", opt, 6)) {
	  m->tio.c_iflag &= ~IXOFF;
      }

      else if (!strncmp("crtscts", opt, 7)) {
	  m->tio.c_cflag |= CRTSCTS;
      }

      else if (!strncmp("-crtscts", opt, 8)) {
	  m->tio.c_cflag &= ~CRTSCTS;
      }

      else
        return -1;

      while (*opt && !index(sep, *opt)) opt++;
    }
  return 0;
}

/*
 *  Job control handling
 *
 *  Somehow the ultrix session handling is broken, so use
 *  the bsdish variant.
 */

/*ARGSUSED*/
void brktty(int fd)
{
  if (separate_sids)
    setsid();		/* will break terminal affiliation */
  /* GNU added for Hurd systems 2001-10-10 */
}

int fgtty(int fd)
{
  int mypid;
  mypid = getpid();

  /*
   * Under BSD we have to set the controlling terminal again explicitly.
   */

  if (separate_sids)
    if (tcsetpgrp(fd, mypid)) {
      debug1("fgtty: tcsetpgrp: %d\n", errno);
      return -1;
    }

  return 0;
}

/* 
 * The alm boards on our sparc center 1000 have a lousy driver.
 * We cannot generate long breaks unless we use the most ugly form
 * of ioctls. jw.
 */
int breaktype = 2;


/*
 * type:
 *  0:	TIOCSBRK / TIOCCBRK
 *  1:	TCSBRK
 *  2:	tcsendbreak()
 * n: approximate duration in 1/4 seconds.
 */
static void DoSendBreak(int fd, int n, int type)
{
  switch (type) {
    case 2:	/* tcsendbreak() =============================== */
      /* 
       * here we hope, that multiple calls to tcsendbreak() can
       * be concatenated to form a long break, as we do not know 
       * what exact interpretation the second parameter has:
       *
       * - sunos 4: duration in quarter seconds
       * - sunos 5: 0 a short break, nonzero a tcdrain()
       * - hpux, irix: ignored
       * - mot88: duration in milliseconds
       * - aix: duration in milliseconds, but 0 is 25 milliseconds.
       */
      debug2("%d * tcsendbreak(fd=%d, 0)\n", n, fd);
	  {
	    int i;

	    if (!n)
	      n++;
	    for (i = 0; i < n; i++)
	      if (tcsendbreak(fd, 0) < 0) {
		    Msg(errno, "cannot send BREAK (tcsendbreak SVR4)");
		    return;
	      }
	  }
      break;

    case 1:	/* TCSBRK ======================================= */
      if (!n)
        n++;
      /*
       * Here too, we assume that short breaks can be concatenated to 
       * perform long breaks. But for SOLARIS, this is not true, of course.
       */
      debug2("%d * TCSBRK fd=%d\n", n, fd);
	  {
	    int i;

	    for (i = 0; i < n; i++)
	      if (ioctl(fd, TCSBRK, (char *)0) < 0) {
		    Msg(errno, "Cannot send BREAK (TCSBRK)");
		    return;
	      }
	  }
      break;

    case 0:	/* TIOCSBRK / TIOCCBRK ========================== */
      /*
       * This is very rude. Screen actively celebrates the break.
       * But it may be the only save way to issue long breaks.
       */
      debug("TIOCSBRK TIOCCBRK\n");
      if (ioctl(fd, TIOCSBRK, (char *)0) < 0) {
	    Msg(errno, "Can't send BREAK (TIOCSBRK)");
	    return;
	  }
      sleep1000(n ? n * 250 : 250);
      if (ioctl(fd, TIOCCBRK, (char *)0) < 0) {
	    Msg(errno, "BREAK stuck!!! -- HELP! (TIOCCBRK)");
	    return;
	  }
      break;

    default:	/* unknown ========================== */
      Msg(0, "Internal SendBreak error: method %d unknown", type);
    }
}

/* 
 * Send a break for n * 0.25 seconds. Tty must be PLAIN.
 * The longest possible break allowed here is 15 seconds.
 */

void SendBreak(struct win *wp, int n, int closeopen)
{
  sigret_t (*sigalrm)__P(SIGPROTOARG);

  if (wp->w_type != W_TYPE_PLAIN)
    return;

  debug3("break(%d, %d) fd %d\n", n, closeopen, wp->w_ptyfd);

  (void) tcflush(wp->w_ptyfd, TCIOFLUSH);

  if (closeopen) {
    close(wp->w_ptyfd);
    sleep1000(n ? n * 250 : 250);
    if ((wp->w_ptyfd = OpenTTY(wp->w_tty, wp->w_cmdargs[1])) < 1) {
	  Msg(0, "Ouch, cannot reopen line %s, please try harder", wp->w_tty);
	  return;
	}
    (void) fcntl(wp->w_ptyfd, F_SETFL, FNBLOCK);
  }
  else {
    sigalrm = signal(SIGALRM, SigAlrmDummy);
    alarm(15);

    DoSendBreak(wp->w_ptyfd, n, breaktype);

    alarm(0);
    signal(SIGALRM, sigalrm);
  }
  debug("            broken.\n");
}

/*
 *  Console grabbing
 */


static struct event consredir_ev;
static int consredirfd[2] = {-1, -1};

static void consredir_readev_fn(struct event *ev, char *data)
{
  char *p, *n, buf[256];
  int l;

  if (!console_window || (l = read(consredirfd[0], buf, sizeof(buf))) <= 0) {
    close(consredirfd[0]);
    close(consredirfd[1]);
    consredirfd[0] = consredirfd[1] = -1;
    evdeq(ev);
    return;
  }

  for (p = n = buf; l > 0; n++, l--)
    if (*n == '\n') {
      if (n > p)
        WriteString(console_window, p, n - p);
      WriteString(console_window, "\r\n", 2);
      p = n + 1;
    }

  if (n > p)
    WriteString(console_window, p, n - p);
}


/*ARGSUSED*/
int TtyGrabConsole(int fd, int on, char *rc_name)
{
  struct display *d;
  struct mode new1, new2;
  char *slave;

  if (on > 0) {
    if (displays == 0) {
      Msg(0, "I need a display");
      return -1;
    }

    for (d = displays; d; d = d->d_next)
	  if (strcmp(d->d_usertty, "/dev/console") == 0)
	    break;

    if (d) {
	  Msg(0, "too dangerous - screen is running on /dev/console");
	  return -1;
	}
  }

  if (consredirfd[0] >= 0) {
    evdeq(&consredir_ev);
    close(consredirfd[0]);
    close(consredirfd[1]);
    consredirfd[0] = consredirfd[1] = -1;
  }
  if (on <= 0)
    return 0;

  /* special linux workaround for a too restrictive kernel */
  if ((consredirfd[0] = OpenPTY(&slave)) < 0) {
    Msg(errno, "%s: could not open detach pty master", rc_name);
    return -1;
  }
  if ((consredirfd[1] = open(slave, O_RDWR | O_NOCTTY)) < 0) {
    Msg(errno, "%s: could not open detach pty slave", rc_name);
    close(consredirfd[0]);
    return -1;
  }
  InitTTY(&new1, 0);
  SetMode(&new1, &new2, 0, 0);
  SetTTY(consredirfd[1], &new2);

  if (UserContext() == 1)
    UserReturn(ioctl(consredirfd[1], TIOCCONS, (char *)&on));
  if (UserStatus()) {
    Msg(errno, "%s: ioctl TIOCCONS failed", rc_name);
    close(consredirfd[0]);
    close(consredirfd[1]);
    return -1;
  }

  consredir_ev.fd = consredirfd[0];
  consredir_ev.type = EV_READ;
  consredir_ev.handler = consredir_readev_fn;
  evenq(&consredir_ev);
  return 0;

}

/*
 * Read modem control lines of a physical tty and write them to buf
 * in a readable format.
 * Will not write more than 256 characters to buf.
 * Returns buf;
 */
char * TtyGetModemStatus(int fd, char *buf)
{
  char *p = buf;
  unsigned int softcar;
  unsigned int mflags;

  struct mode mtio;	/* screen.h */

  int rtscts;
  int clocal;

  GetTTY(fd, &mtio);
  clocal = 0;
  if (mtio.tio.c_cflag & CLOCAL) {
    clocal = 1;
    *p++ = '{';
  }

  if (!(mtio.tio.c_cflag & CRTSCTS))
    rtscts = 0;
  else
    rtscts = 1;

  if (ioctl(fd, TIOCGSOFTCAR, (char *)&softcar) < 0)
    softcar = 0;

  if (ioctl(fd, TIOCMGET, (char *)&mflags) < 0)
    {
      sprintf(p, "NO-TTY? %s", softcar ? "(CD)" : "CD");
      p += strlen(p);
    }
  else
    {
      char *s;

      s = "!RTS ";
      if (mflags & TIOCM_RTS)
        s++;
      while (*s) *p++ = *s++;

      s = "!CTS "; 
      if (!rtscts) {
        *p++ = '(';
        s = "!CTS) "; 
      }
      if (mflags & TIOCM_CTS)
        s++;
      while (*s) *p++ = *s++;

      s = "!DTR ";
      if (mflags & TIOCM_DTR)
        s++;
      while (*s) *p++ = *s++;

      s = "!DSR ";
      if (mflags & TIOCM_DSR)
        s++;
      while (*s) *p++ = *s++;

      s = "!CD "; 
      if (softcar) {
        *p++ = '(';
        s = "!CD) ";
      }

      if (mflags & TIOCM_CD)
        s++;
      while (*s) *p++ = *s++;


      if (mflags & TIOCM_RI)
        for (s = "RI "; *s; *p++ = *s++);


      if (p > buf && p[-1] == ' ')
        p--;
      *p = '\0';
  }
  if (clocal)
    *p++ = '}';
  *p = '\0';
  return buf;
}

/*
 * Old bsd-ish machines may not have any of the baudrate B... symbols.
 * We hope to detect them here, so that the btable[] below always has
 * many entries.
 */

/*
 * On hpux, idx and sym will be different. 
 * Rumor has it that, we need idx in D_dospeed to make tputs
 * padding correct. 
 * Frequently used entries come first.
 */
static struct baud_values btable[] =
{
	{	33,	4000000,	B4000000},
	{	32,	3500000,	B3500000},
	{	31,	3000000,	B3000000},
	{	30,	2500000,	B2500000},
	{	29,	2000000,	B2000000},
	{	28,	1500000,	B1500000},
	{	27,	1152000,	B1152000},
	{	26,	1000000,	B1000000},
	{	25,	921600,		B921600	},
	{	24,	576000,		B576000	},
	{	23,	500000,		B500000	},
	{	22,	460800,		B460800	},
	{	21,	230400,		B230400	},
	{	20,	115200,		B115200	},
	{	19,	57600,		B57600	},
	{	18,	38400,		EXTB	},
	{	18,	38400,		B38400	},
	{	17,	19200,		EXTA	},
	{	17,	19200,		B19200	},
	{	16,	9600,		B9600	},
	{	14,	4800,		B4800	},
	{	12,	2400,		B2400	},
	{	11,	1800,		B1800	},
	{	10,	1200,		B1200	},
 	{	8,	600,		B600	},
 	{	7,	300, 		B300	},
 	{	6,	200, 		B200	},
 	{	5,	150,		B150	},
 	{	4,	134,		B134	},
 	{	3,	110,		B110	},
  	{	2,	75,		B75	},
  	{	1,	50,		B50	},
   	{	0,	0,		B0	},
		{	-1,	-1,		-1	}
};

/*
 * baud may either be a bits-per-second value or a symbolic
 * value as returned by cfget?speed() 
 */
struct baud_values *lookup_baud(int baud)
{
  struct baud_values *p;

  for (p = btable; p->idx >= 0; p++)
    if (baud == p->bps || baud == p->sym)
      return p;
  return NULL;
}

/*
 * change the baud rate in a mode structure.
 * ibaud and obaud are given in bit/second, or at your option as
 * termio B... symbols as defined in e.g. suns sys/ttydev.h
 * -1 means do not change.
 */
int SetBaud(struct mode *m, int ibaud, int obaud)
{
  struct baud_values *ip, *op;

  if ((!(ip = lookup_baud(ibaud)) && ibaud != -1) || (!(op = lookup_baud(obaud)) && obaud != -1))
    return -1;

  if (ip)
    cfsetispeed(&m->tio, ip->sym);
  if (op)
    cfsetospeed(&m->tio, op->sym);
  return 0;
}

/*
 * Define PATH_MAX to 4096 if it's not defined, like on GNU/Hurd
 */


int CheckTtyname (char *tty)
{
  struct stat st;
  char realbuf[PATH_MAX];
  const char *real;
  int rc;

  real = realpath(tty, realbuf);
  if (!real)
    return -1;
  realbuf[sizeof(realbuf)-1]='\0';

  if (lstat(real, &st) || !S_ISCHR(st.st_mode) || (st.st_nlink > 1 && strncmp(real, "/dev", 4)))
    rc = -1;
  else
    rc = 0;

  return rc;
}

/*
 *  Write out the mode struct in a readable form
 */


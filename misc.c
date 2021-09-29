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

#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>	/* mkdir() declaration */
#include <signal.h>
#include <fcntl.h>

#include "config.h"
#include "screen.h"
#include "extern.h"


extern struct layer *flayer;

extern int eff_uid, real_uid;
extern int eff_gid, real_gid;
extern struct mline mline_old;
extern struct mchar mchar_blank;
extern unsigned char *null, *blank;


char *
SaveStr(str)
register const char *str;
{
  register char *cp;

  if ((cp = malloc(strlen(str) + 1)) == NULL)
    Panic(0, "%s", strnomem);
  else
    strcpy(cp, str);
  return cp;
}

char *
SaveStrn(str, n)
register const char *str;
int n;
{
  register char *cp;

  if ((cp = malloc(n + 1)) == NULL)
    Panic(0, "%s", strnomem);
  else
    {
      bcopy((char *)str, cp, n);
      cp[n] = 0;
    }
  return cp;
}

/* cheap strstr replacement */
char *
InStr(str, pat)
char *str;
const char *pat;
{
  int npat = strlen(pat);
  for (;*str; str++)
    if (!strncmp(str, pat, npat))
      return str;
  return 0;
}


void
centerline(str, y)
char *str;
int y;
{
  int l, n;

  ASSERT(flayer);
  n = strlen(str);
  if (n > flayer->l_width - 1)
    n = flayer->l_width - 1;
  l = (flayer->l_width - 1 - n) / 2;
  LPutStr(flayer, str, n, &mchar_blank, l, y);
}

void
leftline(str, y, rend)
char *str;
int y;
struct mchar *rend;
{
  int l, n;
  struct mchar mchar_dol;

  mchar_dol = mchar_blank;
  mchar_dol.image = '$';

  ASSERT(flayer);
  l = n = strlen(str);
  if (n > flayer->l_width - 1)
    n = flayer->l_width - 1;
  LPutStr(flayer, str, n, rend ? rend : &mchar_blank, 0, y);
  if (n != l)
    LPutChar(flayer, &mchar_dol, n, y);
}


char *
Filename(s)
char *s;
{
  register char *p = s;

  if (p)
    while (*p)
      if (*p++ == '/')
        s = p;
  return s;
}

char *
stripdev(nam)
char *nam;
{
  if (nam == NULL)
    return NULL;
  if (strncmp(nam, "/dev/", 5) == 0)
    return nam + 5;
  return nam;
}


/*
 *    Signal handling
 */

sigret_t (*xsignal(sig, func))
 __P(SIGPROTOARG)
int sig;
sigret_t (*func) __P(SIGPROTOARG);
{
  struct sigaction osa, sa;
  sa.sa_handler = func;
  (void)sigemptyset(&sa.sa_mask);
  sa.sa_flags = (sig == SIGCHLD ? SA_RESTART : 0);
  if (sigaction(sig, &sa, &osa))
    return (sigret_t (*)__P(SIGPROTOARG))-1;
  return osa.sa_handler;
}



/*
 *    uid/gid handling
 */


void
xseteuid(euid)
int euid;
{
  int oeuid;

  oeuid = geteuid();
  if (oeuid == euid)
    return;
  if ((int)getuid() != euid)
    oeuid = getuid();
  if (setreuid(oeuid, euid))
    Panic(errno, "setreuid");
}

void
xsetegid(egid)
int egid;
{
  int oegid;

  oegid = getegid();
  if (oegid == egid)
    return;
  if ((int)getgid() != egid)
    oegid = getgid();
  if (setregid(oegid, egid))
    Panic(errno, "setregid");
}





void
bclear(p, n)
char *p;
int n;
{
  bcopy((char *)blank, p, n);
}


void
Kill(pid, sig)
int pid, sig;
{
  if (pid < 2)
    return;
  (void) kill(pid, sig);
}


void
closeallfiles(except)
int except;
{
  int f;
  {
    struct pollfd pfd[1024];
    int maxfd, i, ret, z;

    i = 3; /* skip stdin, stdout and stderr */
    maxfd = getdtablesize();

    while (i < maxfd)
      {
        memset(pfd, 0, sizeof(pfd));

        z = 0;
        for (f = i; f < maxfd && f < i + 1024; f++)
          pfd[z++].fd = f;

        ret = poll(pfd, f - i, 0);
        if (ret < 0)
          Panic(errno, "poll");

        z = 0;
        for (f = i; f < maxfd && f < i + 1024; f++)
          if (!(pfd[z++].revents & POLLNVAL) && f != except)
            close(f);

        i = f;
      }
  }
}



/*
 *  Security - switch to real uid
 */

static int UserSTAT;

int
UserContext()
{
  xseteuid(real_uid);
  xsetegid(real_gid);
  return 1;
}

void
UserReturn(val)
int val;
{
  xseteuid(eff_uid);
  xsetegid(eff_gid);
  UserSTAT = val;
}

int
UserStatus()
{
  return UserSTAT;
}



int
AddXChar(buf, ch)
char *buf;
int ch;
{
  char *p = buf;

  if (ch < ' ' || ch == 0x7f)
    {
      *p++ = '^';
      *p++ = ch ^ 0x40;
    }
  else if (ch >= 0x80)
    {
      *p++ = '\\';
      *p++ = (ch >> 6 & 7) + '0';
      *p++ = (ch >> 3 & 7) + '0';
      *p++ = (ch >> 0 & 7) + '0';
    }
  else
    *p++ = ch;
  return p - buf;
}

int
AddXChars(buf, len, str)
char *buf, *str;
int len;
{
  char *p;

  if (str == 0)
    {
      *buf = 0;
      return 0;
    }
  len -= 4;     /* longest sequence produced by AddXChar() */
  for (p = buf; p < buf + len && *str; str++)
    {
      if (*str == ' ')
        *p++ = *str;
      else
        p += AddXChar(p, *str);
    }
  *p = 0;
  return p - buf;
}



void
sleep1000(msec)
int msec;

{
  struct timeval t;

  t.tv_sec = (long) (msec / 1000);
  t.tv_usec = (long) ((msec % 1000) * 1000);
  select(0, (fd_set *)0, (fd_set *)0, (fd_set *)0, &t);
}


/*
 * This uses either setenv() or putenv(). If it is putenv() we cannot dare
 * to free the buffer after putenv(), unless it it the one found in putenv.c
 */
void
xsetenv(var, value)
char *var;
char *value;
{
  setenv(var, value, 1);
}

/*
 * This is a replacement for the buggy _delay function from the termcap
 * emulation of libcurses, which ignores ospeed.
 */
int
_delay(delay, outc)
register int delay;
int (*outc) __P((int));
{
  int pad;
  extern short ospeed;
  static short osp2pad[] = {
    0,2000,1333,909,743,666,500,333,166,83,55,41,20,10,5,2,1,1
  };

  if (ospeed <= 0 || ospeed >= (int)(sizeof(osp2pad)/sizeof(*osp2pad)))
    return 0;
  pad =osp2pad[ospeed];
  delay = (delay + pad / 2) / pad;
  while (delay-- > 0)
    (*outc)(0);
  return 0;
}





# define xva_arg(s, t, tn) va_arg(s, t)
# define xva_list va_list




time_t SessionCreationTime(const char *fifo) {
  char ppath[20];
  int pfd;
  char pdata[512];
  char *jiffies;

  int pid = atoi(fifo);
  if (pid <= 0) return 0;
  sprintf(ppath, "/proc/%u/stat", pid);
  pfd = open(ppath, O_RDONLY);
  if (pfd < 0) return 0;
  while (1) {
    int R=0, RR;
    RR = read(pfd, pdata + R, 512-R);
    if (RR < 0) {close(pfd); return 0;}
    else if (RR == 0) break;
  }
  close(pfd);

  for (pfd=21, jiffies=pdata; pfd; --pfd) {
    jiffies = strchr(jiffies, ' ');
    if (!jiffies) break; else ++jiffies;
  }
  if (!jiffies) return 0;

  return atol(jiffies) / 100;
}

time_t GetUptime(void) {
  char uptimestr[32];
  int fd = open("/proc/uptime", O_RDONLY);
  if (fd < 0) return 0;
  (void)read(fd, uptimestr, 32);
  close(fd);
  return atol(uptimestr);
}

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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#include "config.h"
#include "screen.h"

# include <sys/ioctl.h>

/* for solaris 2.1, Unixware (SVR4.2) and possibly others */




#include "extern.h"

/*
 * if no PTYRANGE[01] is in the config file, we pick a default
 */
# define PTYRANGE0 "qpr"
# define PTYRANGE1 "0123456789abcdef"

/* SVR4 pseudo ttys don't seem to work with SCO-5 */

extern int eff_uid;

/* used for opening a new pty-pair: */
static char PtyName[32], TtyName[32];


static void initmaster __P((int));

int pty_preopen = 0;

/*
 *  Open all ptys with O_NOCTTY, just to be on the safe side
 *  (RISCos mips breaks otherwise)
 */

/***************************************************************/

static void
initmaster(f)
int f;
{
  tcflush(f, TCIOFLUSH);
}

void
InitPTY(f)
int f;
{
  if (f < 0)
    return;
}

/***************************************************************/


/***************************************************************/


/***************************************************************/


/***************************************************************/


/***************************************************************/

#define PTY_DONE
int
OpenPTY(ttyn)
char **ttyn;
{
  register int f;
  char *m, *ptsname();
  int unlockpt __P((int)), grantpt __P((int));
  int getpt __P((void));
  sigret_t (*sigcld)__P(SIGPROTOARG);

  strcpy(PtyName, "/dev/ptmx");
  if ((f = getpt()) == -1)
    return -1;

  /*
   * SIGCHLD set to SIG_DFL for grantpt() because it fork()s and
   * exec()s pt_chmod
   */
  sigcld = signal(SIGCHLD, SIG_DFL);
  if ((m = ptsname(f)) == NULL || grantpt(f) || unlockpt(f))
    {
      signal(SIGCHLD, sigcld);
      close(f);
      return -1;
    }
  signal(SIGCHLD, sigcld);
  if (strlen(m) < sizeof(TtyName))
    strcpy(TtyName, m);
  else
    {
      close(f);
      return -1;
    }
  initmaster(f);
  *ttyn = TtyName;
  return f;
}

/***************************************************************/


/***************************************************************/


/***************************************************************/


/* len(/proc/self/fd/) + len(max 64 bit int) */
#define MAX_PTS_SYMLINK (14 + 21)
char *GetPtsPathOrSymlink(int fd)
{
	int ret;
	char *tty_name;
	static char tty_symlink[MAX_PTS_SYMLINK];

	errno = 0;
	tty_name = ttyname(fd);
	if (!tty_name && errno == ENODEV) {
		ret = snprintf(tty_symlink, MAX_PTS_SYMLINK, "/proc/self/fd/%d", fd);
		if (ret < 0 || ret >= MAX_PTS_SYMLINK)
			return NULL;
		/* We are setting errno to ENODEV to allow callers to check
		 * whether the pts device exists in another namespace.
		 */
		errno = ENODEV;
		return tty_symlink;
	}

	return tty_name;
}

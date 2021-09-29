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

#include "config.h"
#include "screen.h"
#include "extern.h"



extern struct display *display;
extern struct win *fore;
extern char *LoginName;
extern int real_uid, eff_uid;


/*
 *  UTNOKEEP: A (ugly) hack for apollo that does two things:
 *    1) Always close and reopen the utmp file descriptor. (I don't know
 *       for what reason this is done...)
 *    2) Implement an unsorted utmp file much like GETUTENT.
 *  (split into UT_CLOSE and UT_UNSORTED)
 */



# define UT_CLOSE


/*
 *  we have a suid-root helper app that changes the utmp for us
 *  (won't work for login-slots)
 */





static slot_t TtyNameSlot __P((char *));
static void makeuser __P((struct utmp *, char *, char *, int));
static void makedead __P((struct utmp *));
static int  pututslot __P((slot_t, struct utmp *, char *, struct win *));
static struct utmp *getutslot __P((slot_t));
static struct utmp *xpututline __P((struct utmp *utmp));
# define pututline xpututline


static int utmpok;
static char UtmpName[] = UTMPFILE;
static int utmpfd = -1;


extern struct utmp *getutline(), *pututline();


# undef  D_loginhost
# define D_loginhost D_utmp_logintty.ut_host




/*
 * SlotToggle - modify the utmp slot of the fore window.
 *
 * how > 0	do try to set a utmp slot.
 * how = 0	try to withdraw a utmp slot.
 *
 * w_slot = -1  window not logged in.
 * w_slot = 0   window not logged in, but should be logged in. 
 *              (unable to write utmp, or detached).
 */





void
SlotToggle(how)
int how;
{
  debug1("SlotToggle %d\n", how);
  if (fore->w_type != W_TYPE_PTY)
    {
      Msg(0, "Can only work with normal windows.\n");
      return;
    }
  if (how)
    {
      debug(" try to log in\n");
      if ((fore->w_slot == (slot_t) -1) || (fore->w_slot == (slot_t) 0))
	{
	  if (SetUtmp(fore) == 0)
	    Msg(0, "This window is now logged in.");
	  else
	    Msg(0, "This window should now be logged in.");
	  WindowChanged(fore, 'f');
	}
      else
	Msg(0, "This window is already logged in.");
    }
  else
    {
      debug(" try to log out\n");
      if (fore->w_slot == (slot_t) -1)
	Msg(0, "This window is already logged out\n");
      else if (fore->w_slot == (slot_t) 0)
	{
	  debug("What a relief! In fact, it was not logged in\n");
	  Msg(0, "This window is not logged in.");
	  fore->w_slot = (slot_t) -1;
	}
      else
	{
	  RemoveUtmp(fore);
	  if (fore->w_slot != (slot_t) -1)
	    Msg(0, "What? Cannot remove Utmp slot?");
	  else
	    Msg(0, "This window is no longer logged in.");
	  WindowChanged(fore, 'f');
	}
    }
}




void
InitUtmp()
{
  debug1("InitUtmp testing '%s'...\n", UtmpName);
  if ((utmpfd = open(UtmpName, O_RDWR)) == -1)
    {
      if (errno != EACCES)
	Msg(errno, "%s", UtmpName);
      debug("InitUtmp failed.\n");
      utmpok = 0;
      return;
    }
  close(utmpfd);	/* it was just a test */
  utmpfd = -1;
  utmpok = 1;
}





/*
 * the utmp entry for tty is located and removed.
 * it is stored in D_utmp_logintty.
 */
void
RemoveLoginSlot()
{
  struct utmp u, *uu;

  ASSERT(display);
  debug("RemoveLoginSlot: removing your logintty\n");
  D_loginslot = TtyNameSlot(D_usertty);
  if (D_loginslot == (slot_t)0 || D_loginslot == (slot_t)-1)
    return;
  if (!utmpok)
    {
      D_loginslot = 0;
      debug("RemoveLoginSlot: utmpok == 0\n");
    }
  else
    {

      if ((uu = getutslot(D_loginslot)) == 0)
	{
	  debug("Utmp slot not found -> not removed");
	  D_loginslot = 0;
	}
      else
	{
	  D_utmp_logintty = *uu;
	  u = *uu;
	  makedead(&u);
	  if (pututslot(D_loginslot, &u, (char *)0, (struct win *)0) == 0)
	    D_loginslot = 0;
	}
      UT_CLOSE;
    }
  debug1(" slot %d zapped\n", (int)D_loginslot);
  if (D_loginslot == (slot_t)0)
    {
      /* couldn't remove slot, do a 'mesg n' at least. */
      struct stat stb;
      char *tty;
      debug("couln't zap slot -> do mesg n\n");
      D_loginttymode = 0;
      if ((tty = GetPtsPathOrSymlink(D_userfd)) && stat(tty, &stb) == 0 && (int)stb.st_uid == real_uid && !CheckTtyname(tty) && ((int)stb.st_mode & 0777) != 0666)
	{
	  D_loginttymode = (int)stb.st_mode & 0777;
	  chmod(D_usertty, stb.st_mode & 0600);
	}
    }
}

/*
 * D_utmp_logintty is reinserted into utmp
 */
void
RestoreLoginSlot()
{
  char *tty;

  debug("RestoreLoginSlot()\n");
  ASSERT(display);
  if (utmpok && D_loginslot != (slot_t)0 && D_loginslot != (slot_t)-1)
    {
      debug1(" logging you in again (slot %#x)\n", (int)D_loginslot);
      if (pututslot(D_loginslot, &D_utmp_logintty, D_loginhost, (struct win *)0) == 0)
        Msg(errno,"Could not write %s", UtmpName);
    }
  UT_CLOSE;
  D_loginslot = (slot_t)0;
  if (D_loginttymode && (tty = GetPtsPathOrSymlink(D_userfd)) && !CheckTtyname(tty))
    chmod(tty, D_loginttymode);
}



/*
 * Construct a utmp entry for window wi.
 * the hostname field reflects what we know about the user (display)
 * location. If d_loginhost is not set, then he is local and we write
 * down the name of his terminal line; else he is remote and we keep
 * the hostname here. The letter S and the window id will be appended.
 * A saved utmp entry in wi->w_savut serves as a template, usually.
 */ 

int
SetUtmp(wi)
struct win *wi;
{
  register slot_t slot;
  struct utmp u;
  int saved_ut;
  char *p;
  char host[sizeof(D_loginhost) + 15];

  wi->w_slot = (slot_t)0;
  if (!utmpok || wi->w_type != W_TYPE_PTY)
    return -1;
  if ((slot = TtyNameSlot(wi->w_tty)) == (slot_t)0)
    {
      debug1("SetUtmp failed (tty %s).\n",wi->w_tty);
      return -1;
    }
  debug2("SetUtmp %d will get slot %d...\n", wi->w_number, (int)slot);

  bzero((char *)&u, sizeof(u));
  if ((saved_ut = bcmp((char *) &wi->w_savut, (char *)&u, sizeof(u))))
    /* restore original, of which we will adopt all fields but ut_host */
    bcopy((char *)&wi->w_savut, (char *) &u, sizeof(u));

  if (!saved_ut)
    makeuser(&u, stripdev(wi->w_tty), LoginName, wi->w_pid);

  host[sizeof(host) - 15] = '\0';
  if (display)
    {
      strncpy(host, D_loginhost, sizeof(host) - 15);
      if (D_loginslot != (slot_t)0 && D_loginslot != (slot_t)-1 && host[0] != '\0')
	{
	  /*
	   * we want to set our ut_host field to something like
	   * ":ttyhf:s.0" or
	   * "faui45:s.0" or
	   * "132.199.81.4:s.0" (even this may hurt..), but not
	   * "faui45.informati"......:s.0
	   * HPUX uses host:0.0, so chop at "." and ":" (Eric Backus)
	   */
	  for (p = host; *p; p++)
	    if ((*p < '0' || *p > '9') && (*p != '.'))
	      break;
	  if (*p)
	    {
	      for (p = host; *p; p++)
		if (*p == '.' || (*p == ':' && p != host))
		  {
		    *p = '\0';
		    break;
		  }
	    }
	}
      else
	{
	  strncpy(host + 1, stripdev(D_usertty), sizeof(host) - 15 - 1);
	  host[0] = ':';
	}
    }
  else
    strncpy(host, "local", sizeof(host) - 15);

  sprintf(host + strlen(host), ":S.%d", wi->w_number);
  debug1("rlogin hostname: '%s'\n", host);

  strncpy(u.ut_host, host, sizeof(u.ut_host));

  if (pututslot(slot, &u, host, wi) == 0)
    {
      Msg(errno,"Could not write %s", UtmpName);
      UT_CLOSE;
      return -1;
    }
  debug("SetUtmp successful\n");
  wi->w_slot = slot;
  bcopy((char *)&u, (char *)&wi->w_savut, sizeof(u));
  UT_CLOSE;
  return 0;
}

/*
 * if slot could be removed or was 0,  wi->w_slot = -1;
 * else not changed.
 */

int
RemoveUtmp(wi)
struct win *wi;
{
  struct utmp u, *uu;
  slot_t slot;

  slot = wi->w_slot;
  debug1("RemoveUtmp slot=%#x\n", slot);
  if (!utmpok)
    return -1;
  if (slot == (slot_t)0 || slot == (slot_t)-1)
    {
      wi->w_slot = (slot_t)-1;
      return 0;
    }
  bzero((char *) &u, sizeof(u));
  if ((uu = getutslot(slot)) == 0)
    {
      Msg(0, "Utmp slot not found -> not removed");
      return -1;
    }
  bcopy((char *)uu, (char *)&wi->w_savut, sizeof(wi->w_savut));
  u = *uu;
  makedead(&u);
  if (pututslot(slot, &u, (char *)0, wi) == 0)
    {
      Msg(errno,"Could not write %s", UtmpName);
      UT_CLOSE;
      return -1;
    }
  debug("RemoveUtmp successfull\n");
  wi->w_slot = (slot_t)-1;
  UT_CLOSE;
  return 0;
}



/*********************************************************************
 *
 *  routines using the getut* api
 */


#define SLOT_USED(u) (u->ut_type == USER_PROCESS)

static struct utmp *
getutslot(slot)
slot_t slot;
{
  struct utmp u;
  bzero((char *)&u, sizeof(u));
  strncpy(u.ut_line, slot, sizeof(u.ut_line));
  setutent();
  return getutline(&u);
}

static int
pututslot(slot, u, host, wi)
slot_t slot;
struct utmp *u;
char *host;
struct win *wi;
{
  setutent();
  return pututline(u) != 0;
}

static void
makedead(u)
struct utmp *u;
{
  u->ut_type = DEAD_PROCESS;
  u->ut_exit.e_termination = 0;
  u->ut_exit.e_exit = 0;
  u->ut_user[0] = 0;	/* for Digital UNIX, kilbi@rad.rwth-aachen.de */
}

static void
makeuser(u, line, user, pid)
struct utmp *u;
char *line, *user;
int pid;
{
  time_t now;
  u->ut_type = USER_PROCESS;
  strncpy(u->ut_user, user, sizeof(u->ut_user));
  /* Now the tricky part... guess ut_id */
  strncpy(u->ut_id, line + 3, sizeof(u->ut_id));
  strncpy(u->ut_line, line, sizeof(u->ut_line));
  u->ut_pid = pid;
  /* must use temp variable because of NetBSD/sparc64, where
   * ut_xtime is long(64) but time_t is int(32) */
  (void)time(&now);
  u->ut_time = now;
}

static slot_t
TtyNameSlot(nam)
char *nam;
{
  return stripdev(nam);
}





/*********************************************************************
 *
 *  Cheap plastic imitation of ttyent routines.
 */








/*********************************************************************
 *
 *  getlogin() replacement (for SVR4 machines)
 */


# undef pututline

/* aargh, linux' pututline returns void! */
struct utmp *
xpututline(u)
struct utmp *u;
{
  struct utmp *u2;
  pututline(u);
  setutent();
  u2 = getutline(u);
  if (u2 == 0)
    return u->ut_type == DEAD_PROCESS ? u : 0;
  return u->ut_type == u2->ut_type ? u : 0;
}


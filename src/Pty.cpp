/*
    This file is part of Konsole, an X terminal.
    Copyright (C) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

// Own
#include "Pty.h"

// System
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>

// Qt
#include <QtCore/QStringList>

// KDE
#include <KStandardDirs>
#include <KLocale>
#include <KDebug>
#include <KPty>

using namespace Konsole;

void Pty::donePty()
{
  emit done(exitStatus());
}

void Pty::setWindowSize(int lines, int cols)
{
  pty()->setWinSize(lines, cols);
}

void Pty::setXonXoff(bool on)
{
  pty()->setXonXoff(on);
}

void Pty::setUtf8Mode(bool on)
{
  pty()->setUtf8Mode(on);
}

void Pty::setErase(char erase)
{
  struct termios tios;
  int fd = pty()->slaveFd();
  
  if(tcgetattr(fd, &tios))
  {
    qWarning("Unable to get terminal attributes.");
    return;
  }
  tios.c_cc[VERASE] = erase;
  if(tcsetattr(fd, TCSANOW, &tios))
    qWarning("Unable to set terminal attributes.");
}

int Pty::start(const QString& program, 
               const QStringList& programArguments, 
               const QString& term, 
               ulong winid, 
               bool addToUtmp,
               const QString& dbusService, 
               const QString& dbusSession)
{
  clearArguments();

  setBinaryExecutable(program.toLatin1());

  QStringListIterator it( programArguments );
  while (it.hasNext())
    arguments.append( it.next().toUtf8() );

  if ( !term.isEmpty() )
     setEnvironment("TERM",term);
  if ( !dbusService.isEmpty() )
     setEnvironment("KONSOLE_DBUS_SERVICE",dbusService);
  if ( !dbusSession.isEmpty() )
     setEnvironment("KONSOLE_DBUS_SESSION", dbusSession);

  setEnvironment("WINDOWID", QString::number(winid));

  setUsePty(All, addToUtmp);

  if ( K3Process::start(NotifyOnExit, (Communication) (Stdin | Stdout)) == false )
     return -1;

  resume(); // Start...
  return 0;

}

void Pty::setWriteable(bool writeable)
{
  struct stat sbuf;
  stat(pty()->ttyName(), &sbuf);
  if (writeable)
    chmod(pty()->ttyName(), sbuf.st_mode | S_IWGRP);
  else
    chmod(pty()->ttyName(), sbuf.st_mode & ~(S_IWGRP|S_IWOTH));
}

Pty::Pty()
{
  _bufferFull = false;
  connect(this, SIGNAL(receivedStdout(K3Process *, char *, int )),
	  this, SLOT(dataReceived(K3Process *,char *, int)));
  connect(this, SIGNAL(processExited(K3Process *)),
          this, SLOT(donePty()));
  connect(this, SIGNAL(wroteStdin(K3Process *)),
          this, SLOT(writeReady()));

  setUsePty(All, false); // utmp will be overridden later
}

Pty::~Pty()
{
}

void Pty::writeReady()
{
  _pendingSendJobs.erase(_pendingSendJobs.begin());
  _bufferFull = false;
  doSendJobs();
}

void Pty::doSendJobs() {
  if(_pendingSendJobs.isEmpty())
  {
     emit bufferEmpty(); 
     return;
  }
  
  SendJob& job = _pendingSendJobs.first();

  
  if (!writeStdin( job.data(), job.length() ))
  {
    qWarning("Pty::doSendJobs - Could not send input data to terminal process.");
    return;
  }
  _bufferFull = true;
}

void Pty::appendSendJob(const char* s, int len)
{
  _pendingSendJobs.append(SendJob(s,len));
}

void Pty::sendData(const char* s, int len)
{
  appendSendJob(s,len);
  if (!_bufferFull)
     doSendJobs();
}

void Pty::dataReceived(K3Process *,char *buf, int len)
{
  emit receivedData(buf,len);
}

void Pty::lockPty(bool lock)
{
  if (lock)
    suspend();
  else
    resume();
}

int Pty::foregroundProcessGroup() const
{
    int pid = tcgetpgrp(pty()->masterFd());

    if ( pid != -1 )
    {
        return pid;
    } 

    return 0;
}

#include "Pty.moc"
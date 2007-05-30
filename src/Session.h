/*
    This file is part of Konsole, an X terminal.
    
    Copyright (C) 2007 by Robert Knight <robertknight@gmail.com>
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

#ifndef SESSION_H
#define SESSION_H

// Qt
#include <QtCore/QStringList>
#include <QtCore/QByteRef>

// KDE
#include <KApplication>
#include <KMainWindow>

// Konsole
#include "History.h"

class K3ProcIO;
class K3Process;

namespace Konsole
{

class ColorScheme;
class Emulation;
class Pty;
class TerminalDisplay;
class ZModemDialog;

/**
 * Represents a terminal session consisting of a pseudo-teletype and a terminal emulation.
 * The pseudo-teletype (or PTY) handles I/O between the terminal process and Konsole.
 * The terminal emulation ( Emulation and subclasses ) processes the output stream from the 
 * PTY and produces a character image which is then shown on views connected to the session. 
 *
 * Each Session can be connected to one or more views by using the addView() method.
 * The attached views can then display output from the program running in the terminal
 * or send input to the program in the terminal in the form of keypresses and mouse
 * activity.
 */
class Session : public QObject
{ 
Q_OBJECT

public:
  Q_PROPERTY(QString name READ nameTitle)
  Q_PROPERTY(int processId READ processId)
  Q_PROPERTY(QString keyBindings READ keyBindings WRITE setKeyBindings)
  Q_PROPERTY(QSize size READ size WRITE setSize)

  /** 
   * Constructs a new session.
   * 
   * To start the terminal process, call the run() method,
   * after specifying the program and arguments
   * using setProgram() and setArguments()
   *
   * If no program or arguments are specified explicitly, the Session
   * falls back to using the program specified in the SHELL environment 
   * variable.
   */
  Session();
  ~Session();

  /** 
   * Returns true if the session is currently running.  This will be true
   * after run() has been called successfully.
   */
  bool running() const;

  /** 
   * Sets the profile associated with this session.
   *
   * @param profileKey A key which can be used to obtain the current 
   * profile settings from the SessionManager 
   */
  void setProfileKey(const QString& profileKey);
  /** 
   * Returns the profile key associated with this session.
   * This can be passed to the SessionManager to obtain the current
   * profile settings. 
   */
  QString profileKey() const;

  /** 
   * Adds a new view for this session.    
   * 
   * The viewing widget will display the output from the terminal and 
   * input from the viewing widget (key presses, mouse activity etc.) 
   * will be sent to the terminal.
   *
   * Since terminal applications assume a single terminal screen, 
   * all views of a session will display the same number of lines and 
   * columns.
   *
   */
  void addView(TerminalDisplay* widget);
  /** 
   * Removes a view from this session.  
   *
   * @p widget will no longer display output from or send input
   * to the terminal 
   */
  void removeView(TerminalDisplay* widget);

  /**
   * Returns the views connected to this session
   */
  QList<TerminalDisplay*> views() const;

  /**
   * Returns the terminal emulation instance being used to encode / decode 
   * characters to / from the process.
   */
  Emulation*  emulation() const;    
  
  /** 
   * Returns the value of the TERM environment variable which will 
   * be used in the session's environment when it is started using 
   * the run() method.
   * 
   * Defaults to "xterm".
   */  
  QString terminalType() const;
  /** 
   * Sets the value of the TERM variable which will be used in the 
   * session's environment when it is started using the run() method.  
   * Changing this once the session has been started using run() has no effect.
   *
   * Defaults to "xterm" if not set explicitly
   */
  void setTerminalType(const QString& terminalType);

  /** Returns the unique ID for this session. */
  int sessionId() const;
  
  /** 
   * Return the session title set by the user (ie. the program running 
   * in the terminal), or an empty string if the user has not set a custom title
   */
  QString userTitle() const;
 
  /** 
   * This enum describes the contexts for which separate
   * tab title formats may be specified.
   */
  enum TabTitleContext
  {
    /** Default tab title format */
    LocalTabTitle,
    /** 
     * Tab title format used session currently contains
     * a connection to a remote computer (via SSH)
     */
    RemoteTabTitle
  };
  /** 
   * Sets the format used by this session for tab titles. 
   * 
   * @param context The context whoose format should be set.
   * @param format The tab title format.  This may be a mixture
   * of plain text and dynamic elements denoted by a '%' character
   * followed by a letter.  (eg. %d for directory).  The dynamic
   * elements available depend on the @p context 
   */
  void setTabTitleFormat(TabTitleContext context , const QString& format);
  /** Returns the format used by this session for tab titles. */
  QString tabTitleFormat(TabTitleContext context) const;


  /** Returns the arguments passed to the shell process when run() is called. */
  QStringList arguments() const;
  /** Returns the program name of the shell process started when run() is called. */
  QString program() const;

  /** 
   * Sets the command line arguments which the session's program will be passed when
   * run() is called.
   */
  void setArguments(const QStringList& arguments);
  /** Sets the program to be executed when run() is called. */
  void setProgram(const QString& program);

  /** Returns the session's current working directory. */
  QString initialWorkingDirectory() { return _initialWorkingDir; }
  
  /** 
   * Sets the initial working directory for the session when it is run 
   * This has no effect once the session has been started.
   */
  void setInitialWorkingDirectory( const QString& dir ) { _initialWorkingDir = dir; }

  /**
   * Sets the type of history store used by this session.
   * Lines of output produced by the terminal are added
   * to the history store.  The type of history store
   * used affects the number of lines which can be
   * remembered before they are lost and the storage
   * (in memory, on-disk etc.) used. 
   */ 
  void setHistoryType(const HistoryType& type);
  /**
   * Returns the type of history store used by this session.
   */
  const HistoryType& historyType() const;
  /**
   * Clears the history store used by this session.
   */ 
  void clearHistory();

  /** 
   * Enables monitoring for activity in the session.
   * This will cause notifySessionState() to be emitted
   * with the NOTIFYACTIVITY state flag when output is 
   * received from the terminal. 
   */
  void setMonitorActivity(bool);
  /** Returns true if monitoring for activity is enabled. */
  bool isMonitorActivity() const;
  
  /**
   * Enables monitoring for silence in the session.
   * This will cause notifySessionState() to be emitted
   * with the NOTIFYSILENCE state flag when output is not
   * received from the terminal for a certain period of
   * time, specified with setMonitorSilenceSeconds()
   */
  void setMonitorSilence(bool);
  /** 
   * Returns true if monitoring for inactivity (silence)
   * in the session is enabled.
   */
  bool isMonitorSilence()  const;
  /** See setMonitorSilence() */
  void setMonitorSilenceSeconds(int seconds);
 
  /**
   * Sets the key bindings used by this session.  The bindings
   * specify how input key sequences are translated into 
   * the character stream which is sent to the terminal.
   *
   * @param id The name of the key bindings to use.  The
   * names of available key bindings can be determined using the 
   * KeyboardTranslatorManager class.
   */ 
  void setKeyBindings(const QString& id);
  /** Returns the name of the key bindings used by this session. */
  QString keyBindings() const;

  /**
   * This enum describes the available title roles.
   */
  enum TitleRole
  {
      /** The name of the session. */
      NameRole,
      /** The title of the session which is displayed in tabs etc. */
      DisplayedTitleRole
  };

  /** Sets the session's title for the specified @p role to @p title. */
  void setTitle(TitleRole role , const QString& title);
  /** Returns the session's title for the specified @p role. */
  QString title(TitleRole role) const;
  /** Convenience method used to read the name property.  Returns title(Session::NameRole). */
  QString nameTitle() const { return title(Session::NameRole); }

  /** Sets the name of the icon associated with this session. */
  void setIconName(const QString& iconName);
  /** Returns the name of the icon associated with this session. */
  QString iconName() const;
 
  /** TODO: Document me */ 
  void setIconText(const QString& iconText);
  /** TODO: Document me */
  QString iconText() const;

  /** TODO: Document me */
  void setAddToUtmp(bool);
  
  /** Sends the specified @p signal to the terminal process. */
  bool sendSignal(int signal);

  /** TODO: Document me */
  void setAutoClose(bool b) { _autoClose = b; }
  
  /**
   * Sets whether flow control is enabled for this terminal
   * session.
   */
  void setFlowControlEnabled(bool enabled);
  
  /** 
   * Sends @p text to the current foreground terminal program.
   */
  void sendText(const QString& text) const;

  /** 
   * Returns the process id of the terminal process. 
   * This is the id used by the system API to refer to the process.
   */
  int processId() const;
 
  /**
   * Returns the process id of the terminal's foreground process.
   * This is initially the same as sessionProcessId() but can change
   * as the user starts other programs inside the terminal.
   */
  int foregroundProcessId() const;

  void startZModem(const QString &rz, const QString &dir, const QStringList &list);
  void cancelZModem();
  bool isZModemBusy() { return _zmodemBusy; }

  /** Returns the terminal session's window size in lines and columns. */
  QSize size();
  /** 
   * Emits a request to resize the session to accommodate
   * the specified window size.
   *
   * @param size The size in lines and columns to request.
   */
  void setSize(const QSize& size);
  
public slots:

  /** 
   * Starts the terminal session. 
   *
   * This creates the terminal process and connects the teletype to it.
   */
  void run();
    
  /**
   * Closes the terminal session.  This sends a hangup signal
   * (SIGHUP) to the terminal process and causes the done(Session*)
   * signal to be emitted.
   */   
  void close();
 
  /** TODO: Document me */ 
  void setUserTitle( int, const QString &caption );
  
signals:
  
  /** 
   * Emitted when the terminal process exits.
   */
  void finished();

  /** 
   * Emitted when output is received from the terminal process. 
   */
  void receivedData( const QString& text );
  
  /** Emitted when the session's title has changed. */
  void titleChanged();

  /** Emitted when the session's profile has changed. */
  void profileChanged(const QString& profile);

  /** 
   * Emitted when the activity state of this session changes.
   *
   * @param state The new state of the session.  This may be one
   * of NOTIFYNORMAL, NOTIFYSILENCE or NOTIFYACTIVITY
   */
  void stateChanged(int state);

  /** Emitted when a bell event occurs in the session. */
  void bellRequest( const QString& message );
 
  /** 
   * Requests that the color the text for any tabs associated with
   * this session should be changed;
   *
   * TODO: Document what the parameter does
   */
  void changeTabTextColorRequest(int);

  /**
   * Requests that the background color of views on this session
   * should be changed.
   */
  void changeBackgroundColorRequest(const QColor&);

  void openUrlRequest(const QString& url);

  /** TODO: Document me. */
  void zmodemDetected();

  /**
   * Emitted when the terminal process requests a change
   * in the size of the terminal window.  
   *
   * @param size The requested window size in terms of lines and columns.
   */
  void resizeRequest(const QSize& size);

private slots:
  void done(int);
    
  void fireZModemDetected();
  
  void ptyError();
  
  void onReceiveBlock( const char* buffer, int len );
  void monitorTimerDone();
  
  void onViewSizeChange(int height, int width);
  void onEmulationSizeChange(int lines , int columns);

  void activityStateSet(int);

  //automatically detach views from sessions when view is destroyed
  void viewDestroyed(QObject* view);

  void zmodemStatus(K3Process *, char *data, int len);
  void zmodemSendBlock(K3Process *, char *data, int len);
  void zmodemRcvBlock(const char *data, int len);
  void zmodemDone();
  void zmodemContinue();

private:

  void updateTerminalSize();

  int            _uniqueIdentifier;

  Pty*          _shellProcess;
  Emulation*    _emulation;

  QList<TerminalDisplay*> _views;

  bool           _monitorActivity;
  bool           _monitorSilence;
  bool           _notifiedActivity;
  bool           _masterMode;
  bool           _autoClose;
  bool           _wantedClose;
  QTimer*        _monitorTimer;

  int            _silenceSeconds;

  QString        _nameTitle;
  QString        _displayTitle;
  QString        _userTitle;

  QString        _localTabTitleFormat;
  QString        _remoteTabTitleFormat;

  QString        _iconName;
  QString        _iconText; // as set by: echo -en '\033]1;IconText\007
  bool           _addToUtmp;
  bool           _flowControl;
  bool           _fullScripting;

  QString        _program;
  QStringList    _arguments;

  QString        _term;
  ulong          _winId;
  int            _sessionId;

  QString        _initialWorkingDir;

  // ZModem
  bool           _zmodemBusy;
  K3ProcIO*       _zmodemProc;
  ZModemDialog*  _zmodemProgress;

  // Color/Font Changes by ESC Sequences

  QColor         _modifiedBackground; // as set by: echo -en '\033]11;Color\007

  QString        _profileKey;

  static int lastSessionId;
  
};

}

#endif
/*
    Copyright (C) 2006-2007 by Robert Knight <robertknight@gmail.com>

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
#include "MainWindow.h"

// Qt
#include <QtGui/QBoxLayout>

// KDE
#include <KAction>
#include <KActionCollection>
#include <KActionMenu>
#include <KShortcutsDialog>
#include <KLocale>
#include <KMenu>
#include <KMenuBar>
#include <KService>
#include <KToggleAction>
#include <KToggleFullScreenAction>
#include <KToolInvocation>
#include <kstandardaction.h>
#include <KXMLGUIFactory>

// Konsole
#include "BookmarkHandler.h"
#include "IncrementalSearchBar.h"
#include "RemoteConnectionDialog.h"
#include "SessionController.h"
#include "ProfileList.h"
#include "ManageProfilesDialog.h"
#include "ViewManager.h"
#include "ViewSplitter.h"

using namespace Konsole;

MainWindow::MainWindow()
 : KXmlGuiWindow() ,
   _bookmarkHandler(0),
   _pluggedController(0)
{
    // create actions for menus
    // the directory ('konsole') is included in the path here so that the XML
    // file can be found when this code is being used in the Konsole part.
    setXMLFile("konsole/konsoleui.rc");
    setupActions();

    // create view manager
        _viewManager = new ViewManager(this,actionCollection());
    connect( _viewManager , SIGNAL(empty()) , this , SLOT(close()) );
    connect( _viewManager , SIGNAL(activeViewChanged(SessionController*)) , this ,
            SLOT(activeViewChanged(SessionController*)) );
    connect( _viewManager , SIGNAL(viewPropertiesChanged(const QList<ViewProperties*>&)) ,
           bookmarkHandler() , SLOT(setViews(const QList<ViewProperties*>&)) ); 

    connect( _viewManager , SIGNAL(setMenuBarVisible(bool)) , menuBar() ,
            SLOT(setVisible(bool)) );

    // create main window widgets
    setupWidgets();

    // create menus
    createGUI();
}

ViewManager* MainWindow::viewManager() const
{
    return _viewManager;
}

void MainWindow::activeViewChanged(SessionController* controller)
{
    if ( _pluggedController == controller )
        return;

    // associated bookmark menu with current session
    bookmarkHandler()->setActiveView(controller);
    disconnect( bookmarkHandler() , SIGNAL(openUrl(const KUrl&)) , 0 , 0 );
    connect( bookmarkHandler() , SIGNAL(openUrl(const KUrl&)) , controller ,
             SLOT(openUrl(const KUrl&)) );

    // set the current session's search bar
    controller->setSearchBar( searchBar() );
    controller->setShowMenuAction( _toggleMenuBarAction );

    // listen for title changes from the current session
    if ( _pluggedController )
    {
        disconnect( _pluggedController , SIGNAL(titleChanged(ViewProperties*)) 
                     , this , SLOT(activeViewTitleChanged(ViewProperties*)) );
        guiFactory()->removeClient(_pluggedController);
    }

    Q_ASSERT( controller );

    connect( controller , SIGNAL(titleChanged(ViewProperties*)) , 
            this , SLOT(activeViewTitleChanged(ViewProperties*)) );

    guiFactory()->addClient(controller);

    // update session title to match newly activated session
    activeViewTitleChanged(controller);

    _pluggedController = controller;
}

void MainWindow::activeViewTitleChanged(ViewProperties* properties)
{
    setPlainCaption(properties->title());
    setWindowIcon(properties->icon());
}

IncrementalSearchBar* MainWindow::searchBar() const
{
    return _searchBar;
}

void MainWindow::setupActions()
{
    KActionCollection* collection = actionCollection();

    // File Menu
    QAction* newTabAction = collection->addAction("new-tab"); 
    newTabAction->setIcon( KIcon("openterm") );
    newTabAction->setText( i18n("New &Tab") );
    newTabAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_N) );
    connect( newTabAction , SIGNAL(triggered()) , this , SLOT(newTab()) );

    QAction* newWindowAction = collection->addAction("new-window"); 
    newWindowAction->setIcon( KIcon("window-new") );
    newWindowAction->setText( i18n("New &Window") );
    newWindowAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_M) );
    connect( newWindowAction , SIGNAL(triggered()) , this , SLOT(newWindow()) );

    QAction* newFromProfileAction = collection->addAction("new-from-profile");
    newFromProfileAction->setText( i18n("New From Profile...") );
    // TODO Implement "New from Profile"

    QAction* remoteConnectionAction = collection->addAction("remote-connection");
    remoteConnectionAction->setText( i18n("Remote Connection...") );
    remoteConnectionAction->setIcon( KIcon("network") );
    remoteConnectionAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_R) );
    connect( remoteConnectionAction , SIGNAL(triggered()) , this , SLOT(showRemoteConnectionDialog()) ); 

       
#ifndef KONSOLE_PART 
    QAction* quitAction = KStandardAction::quit( this , SLOT(close()) , collection );
    // the default shortcut for quit is typically Ctrl+[Some Letter, usually Q] but that is reserved for 
    // use by terminal applications
    quitAction->setShortcut(Qt::CTRL+Qt::SHIFT+Qt::Key_Q);
#endif

    // Bookmark Menu
    KActionMenu* bookmarkMenu = new KActionMenu(i18n("&Bookmarks") , collection );
    _bookmarkHandler = new BookmarkHandler( collection , bookmarkMenu->menu() , true , this );
    collection->addAction("bookmark" , bookmarkMenu);

    //TODO - The 'Add Bookmark' menu action currently has a Ctrl+B shortcut by
    // default which cannot be overridden

    // View Menu
    _toggleMenuBarAction = new KToggleAction(this);
    _toggleMenuBarAction->setText( i18n("Show Menu Bar") );
    _toggleMenuBarAction->setIcon( KIcon("show-menu") );
    _toggleMenuBarAction->setCheckedState( KGuiItem("Hide Menu Bar","show-menu") ); 
    _toggleMenuBarAction->setChecked( !menuBar()->isHidden() );
    connect( _toggleMenuBarAction , SIGNAL(toggled(bool)) , menuBar() , SLOT(setVisible(bool)) );
    collection->addAction("show-menubar",_toggleMenuBarAction);

    KToggleFullScreenAction* fullScreenAction = new KToggleFullScreenAction(this);
    fullScreenAction->setWindow(this);
    fullScreenAction->setShortcut( Qt::CTRL + Qt::SHIFT + Qt::Key_F11 );
    collection->addAction("view-full-screen",fullScreenAction);
    connect( fullScreenAction , SIGNAL(toggled(bool)) , this , SLOT(viewFullScreen(bool)) ); 
    //TODO - Implmement this correctly
    //
    //QAction* mergeAction = collection->addAction("merge-windows");
    //mergeAction->setText( i18n("&Merge Windows") );
    //connect( mergeAction , SIGNAL(triggered()) , this , SLOT(mergeWindows()) );

    // Settings Menu
    KStandardAction::configureNotifications( 0 , 0 , collection  );
    KStandardAction::keyBindings( this , SLOT(showShortcutsDialog()) , collection  );

    QAction* manageProfilesAction = collection->addAction("manage-profiles");
    manageProfilesAction->setText( i18n("Manage Profiles...") );
    manageProfilesAction->setIcon( KIcon("configure") );
    connect( manageProfilesAction , SIGNAL(triggered()) , this , SLOT(showManageProfilesDialog()) );

}

void MainWindow::viewFullScreen(bool fullScreen) 
{
    if ( fullScreen )
        showFullScreen();
    else
        showNormal();
}
BookmarkHandler* MainWindow::bookmarkHandler() const
{
    return _bookmarkHandler;
}

void MainWindow::setSessionList(ProfileList* list)
{
    sessionListChanged(list->actions());

    connect( list , SIGNAL(profileSelected(const QString&)) , this , 
            SLOT(newFromProfile(const QString&)) );

    connect( list , SIGNAL(actionsChanged(const QList<QAction*>&)) , this ,
            SLOT(sessionListChanged(const QList<QAction*>&)) );
}

void MainWindow::sessionListChanged(const QList<QAction*>& actions)
{
    unplugActionList("favorite-profiles");
    plugActionList("favorite-profiles",actions);
}

void MainWindow::newTab()
{
    emit newSessionRequest(QString(),_viewManager);
}

void MainWindow::newWindow()
{
    emit newWindowRequest(QString());
}

void MainWindow::showShortcutsDialog()
{
    KShortcutsDialog::configure( actionCollection() ,
                                 KShortcutsEditor::LetterShortcutsDisallowed );
}

void MainWindow::newFromProfile(const QString& key)
{
    emit newSessionRequest(key,_viewManager);
}
void MainWindow::showManageProfilesDialog()
{
    ManageProfilesDialog* dialog = new ManageProfilesDialog(this);
    dialog->show();
}

void MainWindow::showRemoteConnectionDialog()
{
    RemoteConnectionDialog dialog(this);
    if ( dialog.exec() == QDialog::Accepted )
        emit newSessionRequest(dialog.sessionKey(),_viewManager);
}

void MainWindow::setupWidgets()
{
    QWidget* widget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout();

    _searchBar = new IncrementalSearchBar( IncrementalSearchBar::AllFeatures , this);
    _searchBar->setVisible(false);

    layout->addWidget( _viewManager->widget() );
    layout->addWidget( _searchBar );
    layout->setMargin(0);
    layout->setSpacing(0);

    widget->setLayout(layout);
    
    setCentralWidget(widget);
}

#include "MainWindow.moc"
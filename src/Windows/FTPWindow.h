/*
	NppFTP: FTP/SFTP functionality for Notepad++
	Copyright (C) 2010  Harry (harrybharry@users.sourceforge.net)

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FTPWINDOW_H
#define FTPWINDOW_H

#include "DockableWindow.h"

#include "Toolbar.h"
#include "Treeview.h"
#include "ProfileObject.h"
#include "QueueWindow.h"
#include "FTPProfile.h"
#include "FTPSettings.h"
#include "QueueOperation.h"
#include "OutputWindow.h"
#include "SettingsDialog.h"
#include "ProfilesDialog.h"
#include "DragDropSupport.h"
#include "DragDropWindow.h"
#include "ProfilesWindow.h"

#include <string>
#include <vector>

class FTPSession;

class FTPWindow : public DockableWindow, public DropTargetWindow, public DropDataWindow, public ProfilesWindow {
	friend class NppFTP;	//for output window/ratio
	friend class ProfilesWindow;
public:
							FTPWindow();
	virtual					~FTPWindow();

	////////////////////////
	//DockableWindow
	virtual int				Create(HWND hParent, HWND hNpp, int MenuID, int MenuCommand);
	virtual int				Destroy();

	virtual int				Show(bool show);
	virtual int				Focus();

	virtual int				Init(FTPSession * session, vProfile * vProfiles, FTPSettings * ftpSettings);

	virtual int				OnSize(int newWidth, int newHeight);
	virtual int				OnProfileChange();
	virtual int				OnActivateLocalFile(const TCHAR* filename);
	virtual int				UploadCurrentFile(bool promptForFile = false);

	static int				RegisterClass();

	virtual LRESULT			MessageProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

	virtual int				OnFileItemDrop(FileObject* item, FileObject* parent, bool bIsMove);
	////////////////////////
	//DropTargetWindow
	virtual bool			AcceptType(LPDATAOBJECT pDataObj);
	virtual HRESULT			OnDragEnter(LPDATAOBJECT pDataObj, DWORD grfKeyState, POINTL pt, LPDWORD pdwEffect);
	virtual HRESULT			OnDragOver(DWORD grfKeyState, POINTL pt, LPDWORD pdwEffect);
	virtual HRESULT			OnDragLeave();
	virtual HRESULT			OnDrop(LPDATAOBJECT pDataObj, DWORD grfKeyState, POINTL pt, LPDWORD pdwEffect);

	////////////////////////
	//DropDataWindow
	virtual int				GetNrFiles();
	virtual int				GetFileDescriptor(FILEDESCRIPTOR * fd, int index);
	virtual int				StreamData(CStreamData * stream, int index);
	virtual int				OnEndDnD();

protected:
	virtual int				CreateMenus();
	virtual int				SetToolbarState();

	virtual int				OnEvent(QueueOperation * queueOp, int code, void * data, bool isStart);
	virtual int				OnDirectoryRefresh(FileObject * parent, FTPFile * files, int count);
	virtual int				OnError(QueueOperation * queueOp, int code, void * data, bool isStart);
	virtual int				ShowOperationFailure(QueueOperation * queueOp);

	virtual int				OnItemActivation();

	virtual int				OnConnect(int code);
	virtual int				OnDisconnect(int code);

	virtual int				CreateDirectory(FileObject * parent);
	virtual int				DeleteDirectory(FileObject * dir);

	virtual int				CreateFile(FileObject * parent);
	virtual int				DeleteFile(FileObject* file);

	virtual int				Rename(FileObject* fo, const TCHAR* newName);
	virtual int				Move(FileObject* fo, FileObject* newParent);
	virtual int				Chmod(FileObject * fo);

	virtual int				Copy(FileObject* fo, FileObject* _newParent);
	virtual int				VScrollTreeView(LONG pos);
	virtual int				CreateRemoteBrowser();
	virtual int				DestroyRemoteBrowser();
	virtual int				LayoutRemoteBrowser();
	virtual int				ShowRemoteBrowser(bool show);
	virtual int				SetRemoteCurrentDir(FileObject * dir, bool refresh);
	virtual int				FillRemoteList();
	virtual int				UpdateRemotePathControls();
	virtual int				AddRemoteRecentDir(const char * path);
	virtual int				NavigateRemotePathFromCombo();
	virtual int				NavigateRemotePath(const char * path);
	virtual int				ActivateRemoteListSelection();
	virtual FileObject*		GetRemoteListSelection();
	virtual int				OnRemoteListSelectionChanged();
	virtual int				PromptRemoteRename(FileObject * fo);
	virtual int				SelectRemoteUploadFiles(FileObject * target);
	virtual int				QueueDirectRemoteUploads(FileObject * target, const std::vector<std::basic_string<TCHAR> > & paths);
	virtual int				BeginRemoteDirectoryUpload(const TCHAR * localDirectory, FileObject * target);
	virtual int				ConfirmRemoteUploadCollisions(RemoteUploadPlan * plan);
	virtual void			RecordRemoteUploadFailure(RemoteUploadBatch * batch, const TCHAR * operation, QueueOperation * op);
	virtual void			ShowRemoteUploadSummary(RemoteUploadBatch * batch);
	virtual FileObject*		GetRemoteDropTarget(POINTL point, int * itemIndex = NULL);
	virtual bool			IsRemoteParentItem(int itemIndex);

	static LRESULT CALLBACK	RemoteDirComboProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK	RemoteListProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR data);

	//virtual int				UploadOtherFile(FileObject * parent);

	Toolbar					m_toolbar;
	Rebar					m_rebar;
	Treeview				m_treeview;
	TreeImageList			m_treeimagelist;
	QueueWindow				m_queueWindow;
	SettingsDialog			m_settingsDialog;
	ProfilesDialog			m_profilesDialog;
	WindowSplitter			m_splitter;

	OutputWindow			m_outputWindow;
	bool					m_outputShown;
	bool					m_remoteBrowserShown;
	bool					m_remoteBusyCursor;
	bool					m_overwriteAll;

	HBRUSH					m_backgroundBrush;

	FileObject*				m_currentSelection;
	FileObject*				m_remoteCurrentDir;
	bool					m_localFileExists;

	HWND					m_remoteHostLabel;
	HWND					m_remotePathLabel;
	HWND					m_remoteSearchLabel;
	HWND					m_remoteSearchEdit;
	HWND					m_remoteDirLabel;
	HWND					m_remoteDirCombo;
	HWND					m_remoteList;
	WNDPROC					m_remoteDirComboProc;
	char					m_remotePendingPath[MAX_PATH];

	HMENU					m_popupProfile;
	HMENU					m_popupSettings;
	HMENU					m_popupFile;
	HMENU					m_popupDir;
	HMENU					m_popupRootDir;
	HMENU					m_popupLink;
	HMENU					m_popupQueueActive;
	HMENU					m_popupQueueHold;
	HMENU					m_popupRemoteFile;
	HMENU					m_popupRemoteDir;
	HMENU					m_popupRemoteBlank;

	FTPSession*				m_ftpSession;
	vProfile*				m_vProfiles;
	FTPSettings*			m_ftpSettings;

	bool					m_connecting;
	bool					m_busy;
	QueueOperation*			m_cancelOperation;

	DragDropWindow			m_dndWindow;
	FileObject*				m_currentDropObject;
	FileObject*				m_currentDragObject;

	static const TCHAR * FTPWINDOWCLASS;

};

#endif //FTPWINDOW_H

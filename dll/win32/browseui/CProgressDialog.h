/*
 * Progress dialog
 *
 * Copyright 2014              Huw Campbell
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef _PROGRESSDIALOG_H_
#define _PROGRESSDIALOG_H_

#define WM_DLG_UPDATE   (WM_APP+1)  /* set to the dialog when it should update */
#define WM_DLG_DESTROY  (WM_APP+2)  /* DestroyWindow must be called from the owning thread */

class CProgressDialog :
    public CDialogImpl<CProgressDialog>,
    public CComCoClass<CProgressDialog, &CLSID_ProgressDialog>,
    public CComObjectRootEx<CComMultiThreadModelNoCS>,
    public IProgressDialog,
    public IOleWindow
{
public:
    HANDLE m_hThreadStartEvent;
    CComCriticalSection m_cs;
    HWND m_hwndParent;
    HWND m_hwnd;
    DWORD m_dwFlags;
    DWORD m_dwUpdate;
    LPWSTR m_strings[5];
    BOOL m_isCancelled;
    ULONGLONG m_ullCompleted;
    ULONGLONG m_ullTotal;
    HWND m_hwndDisabledParent;

    UINT m_clockHand;
    struct progressMark {
        ULONGLONG ullMark;
        DWORD     dwTime;
    };
    progressMark m_progressClock[30];
    DWORD m_dwStartTime;

    void set_progress_marquee();
    void update_dialog(DWORD dwUpdate);
    void end_dialog();

    LRESULT OnInitDialog(UINT nMessage, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnTimer(UINT nMessage, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnDlgUpdate(UINT nMessage, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnDlgDestroy(UINT nMessage, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnCommand(UINT nMessage, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

    CProgressDialog();
    ~CProgressDialog();

    // IProgressDialog
    virtual HRESULT WINAPI StartProgressDialog(HWND hwndParent, IUnknown *punkEnableModeless, DWORD dwFlags, LPCVOID reserved);
    virtual HRESULT WINAPI StopProgressDialog();
    virtual HRESULT WINAPI SetTitle(LPCWSTR pwzTitle);
    virtual HRESULT WINAPI SetAnimation(HINSTANCE hInstance, UINT uiResourceId);
    virtual BOOL    WINAPI HasUserCancelled();
    virtual HRESULT WINAPI SetProgress64(ULONGLONG ullCompleted, ULONGLONG ullTotal);
    virtual HRESULT WINAPI SetProgress(DWORD dwCompleted, DWORD dwTotal);
    virtual HRESULT WINAPI SetLine(DWORD dwLineNum, LPCWSTR pwzLine, BOOL bPath, LPCVOID reserved);
    virtual HRESULT WINAPI SetCancelMsg(LPCWSTR pwzMsg, LPCVOID reserved);
    virtual HRESULT WINAPI Timer(DWORD dwTimerAction, LPCVOID reserved);

    //////// IOleWindow
    virtual HRESULT WINAPI GetWindow(HWND* phwnd);
    virtual HRESULT WINAPI ContextSensitiveHelp(BOOL fEnterMode);

DECLARE_REGISTRY_RESOURCEID(IDR_PROGRESSDIALOG)
DECLARE_NOT_AGGREGATABLE(CProgressDialog)

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CProgressDialog)
    COM_INTERFACE_ENTRY_IID(IID_IProgressDialog, IProgressDialog)
    COM_INTERFACE_ENTRY_IID(IID_IOleWindow, IOleWindow)
END_COM_MAP()

enum { IDD = IDD_PROGRESS_DLG };

BEGIN_MSG_MAP(CProgressDialog)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    MESSAGE_HANDLER(WM_TIMER, OnTimer)
    MESSAGE_HANDLER(WM_DLG_UPDATE, OnDlgUpdate)
    MESSAGE_HANDLER(WM_DLG_DESTROY, OnDlgDestroy)
    MESSAGE_HANDLER(WM_CLOSE, OnCommand)
    MESSAGE_HANDLER(WM_COMMAND, OnCommand)
END_MSG_MAP()
};

#endif /* _PROGRESSDIALOG_H_ */

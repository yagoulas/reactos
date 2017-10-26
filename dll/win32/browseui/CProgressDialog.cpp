/*
 *	Progress dialog
 *
 *  Copyright 2007  Mikolaj Zalewski
 *	Copyright 2014	Huw Campbell
 *
 * this library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * this library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <precomp.h>

#define COBJMACROS

#define CANCEL_MSG_LINE 2

#define CANCEL_MSG_INDEX 3

#define TITLE_INDEX 4

/* Note: to avoid a deadlock we don't want to send messages to the dialog
 * with the critical section held. Instead we only mark what fields should be
 * updated and the dialog proc does the update */
#define UPDATE_PROGRESS         0x1
#define UPDATE_TITLE            0x2
#define UPDATE_LINE1            0x4
#define UPDATE_LINE2            (UPDATE_LINE1<<1)
#define UPDATE_LINE3            (UPDATE_LINE1<<2)

#define ID_3SECONDS 101

#define BUFFER_SIZE 256

CProgressDialog::CProgressDialog()
{
    for (int i = 0; i< 5; i++)
    {
        m_strings[i] = (LPWSTR)HeapAlloc(GetProcessHeap(), 0, BUFFER_SIZE);
        m_strings[i][0] = UNICODE_NULL;
    }

    m_clockHand = -1;
    m_progressClock[29].ullMark = 0ull;
    m_dwStartTime = GetTickCount();

    m_hwndParent = NULL;
    m_hThreadStartEvent = NULL;

    m_cs.Init();
}

CProgressDialog::~CProgressDialog()
{
    if (m_hwnd)
        end_dialog();

    for (int i = 0; i< 5; i++)
        HeapFree(GetProcessHeap(), 0, m_strings[i]);

    m_cs.Term();
}

static void set_buffer(LPWSTR buffer, LPCWSTR string)
{
    if (!string)
    {
        buffer[0] = UNICODE_NULL;
        return;
    }

    StringCbCopyW(buffer, BUFFER_SIZE, string);
}

static void load_string(LPWSTR buffer, HINSTANCE hInstance, UINT uiResourceId)
{
    LoadStringW(hInstance, uiResourceId, buffer, BUFFER_SIZE);
}

void CProgressDialog::set_progress_marquee()
{
    HWND hProgress = GetDlgItem(IDC_PROGRESS_BAR);
    ::SetWindowLongW(hProgress, GWL_STYLE,
        ::GetWindowLongW(hProgress, GWL_STYLE)|PBS_MARQUEE);
}

void CProgressDialog::update_dialog(DWORD dwUpdate)
{
    WCHAR empty[] = {0};

    if (dwUpdate & UPDATE_TITLE)
        SetWindowTextW(m_strings[TITLE_INDEX]);

    if (dwUpdate & UPDATE_LINE1)
        SetDlgItemTextW(IDC_TEXT_LINE, (m_isCancelled ? empty : m_strings[0]));
    if (dwUpdate & UPDATE_LINE2)
        SetDlgItemTextW(IDC_TEXT_LINE+1, (m_isCancelled ? empty : m_strings[1]));
    if (dwUpdate & UPDATE_LINE3)
        SetDlgItemTextW(IDC_TEXT_LINE+2, (m_isCancelled ? m_strings[CANCEL_MSG_INDEX] : m_strings[2]));

    if (dwUpdate & UPDATE_PROGRESS)
    {
        ULONGLONG ullTotal = m_ullTotal;
        ULONGLONG ullCompleted = m_ullCompleted;

        /* progress bar requires 32-bit coordinates */
        while (ullTotal >> 32)
        {
            ullTotal >>= 1;
            ullCompleted >>= 1;
        }

        SendDlgItemMessage(IDC_PROGRESS_BAR, PBM_SETRANGE32, 0, (DWORD)ullTotal);
        SendDlgItemMessage(IDC_PROGRESS_BAR, PBM_SETPOS, (DWORD)ullCompleted, 0);
    }
}

void CProgressDialog::end_dialog()
{
    SendMessage(WM_DLG_DESTROY, 0, 0);
    /* native doesn't re-enable the window? */
    if (m_hwndDisabledParent)
        ::EnableWindow(m_hwndDisabledParent, TRUE);
}

LRESULT CProgressDialog::OnInitDialog(UINT nMessage, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if (m_dwFlags & PROGDLG_NOPROGRESSBAR)
        ::ShowWindow(GetDlgItem(IDC_PROGRESS_BAR), SW_HIDE);
    if (m_dwFlags & PROGDLG_NOCANCEL)
        ::ShowWindow(GetDlgItem(IDCANCEL), SW_HIDE);
    if (m_dwFlags & PROGDLG_MARQUEEPROGRESS)
        set_progress_marquee();
    if (m_dwFlags & PROGDLG_NOMINIMIZE)
        SetWindowLongW(GWL_STYLE, GetWindowLongW(GWL_STYLE) & (~WS_MINIMIZEBOX));

    update_dialog(0xffffffff);
    m_dwUpdate = 0;
    m_isCancelled = FALSE;

    SetTimer(ID_3SECONDS, 3 * 1000, NULL);
    
    SetEvent(m_hThreadStartEvent);
    return TRUE;
}

LRESULT CProgressDialog::OnDlgUpdate(UINT nMessage, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    CComCritSecLock<CComCriticalSection> lock(m_cs, true);
    update_dialog(m_dwUpdate);
    m_dwUpdate = 0;
    return TRUE;
}

LRESULT CProgressDialog::OnDlgDestroy(UINT nMessage, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    DestroyWindow();
    PostThreadMessageW(GetCurrentThreadId(), WM_NULL, 0, 0); /* wake up the GetMessage */
    KillTimer(ID_3SECONDS);
    return TRUE;
}

LRESULT CProgressDialog::OnCommand(UINT nMessage, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if (nMessage == WM_CLOSE || wParam == IDCANCEL)
    {
        CComCritSecLock<CComCriticalSection> lock(m_cs, true);
        m_isCancelled = TRUE;

        if (!m_strings[CANCEL_MSG_INDEX][0]) {
            load_string(m_strings[CANCEL_MSG_INDEX], _AtlBaseModule.GetResourceInstance(), IDS_CANCELLING);
        }

        set_progress_marquee();
        ::EnableWindow(GetDlgItem(IDCANCEL), FALSE);
        update_dialog(UPDATE_LINE1|UPDATE_LINE2|UPDATE_LINE3);
    }
    return TRUE;
}

LRESULT CProgressDialog::OnTimer(UINT nMessage, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    CComCritSecLock<CComCriticalSection> lock(m_cs, true);
    if (m_progressClock[29].ullMark != 0ull) {
        // We have enough info to take a guess
        ULONGLONG sizeDiff = m_progressClock[m_clockHand].ullMark - 
                             m_progressClock[(m_clockHand + 29) % 30].ullMark;
        DWORD     timeDiff = m_progressClock[m_clockHand].dwTime - 
                             m_progressClock[(m_clockHand + 29) % 30].dwTime;
        DWORD      runDiff = m_progressClock[m_clockHand].dwTime - 
                             m_dwStartTime;
        ULONGLONG sizeLeft = m_ullTotal - m_progressClock[m_clockHand].ullMark;

        // A guess for time remaining based on the recent slope.
        DWORD timeLeftD = (DWORD) timeDiff * ((double) sizeLeft) / ((double) sizeDiff);
        // A guess for time remaining based on the start time and current position
        DWORD timeLeftI = (DWORD) runDiff * ((double) sizeLeft) / ((double) m_progressClock[m_clockHand].ullMark);

        StrFromTimeIntervalW(m_strings[2], 128, timeLeftD * 0.3 + timeLeftI * 0.7 , 2);
        update_dialog( UPDATE_LINE1 << 2 );
    }
    return TRUE;
}

static DWORD WINAPI dialog_thread(LPVOID lpParameter)
{
    /* Note: until we set the hEvent in WM_INITDIALOG, the ProgressDialog object
     * is protected by the critical section held by StartProgress */
    CProgressDialog* pThis = reinterpret_cast<CProgressDialog*>(lpParameter);
     
    HWND hwnd = pThis->Create(pThis->m_hwndParent);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        if (!IsWindow(hwnd))
            break;
        if(!IsDialogMessageW(hwnd, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    return 0;
}

HRESULT WINAPI CProgressDialog::StartProgressDialog(HWND hwndParent, IUnknown *punkEnableModeless, DWORD dwFlags, LPCVOID reserved)
{
    static const INITCOMMONCONTROLSEX init = { sizeof(init), ICC_ANIMATE_CLASS };

    // TRACE("(%p, %p, %x, %p)\n", this, punkEnableModeless, dwFlags, reserved);
    if (punkEnableModeless || reserved)
        FIXME("Reserved parameters not null (%p, %p)\n", punkEnableModeless, reserved);
    if (dwFlags & PROGDLG_AUTOTIME)
        FIXME("Flags PROGDLG_AUTOTIME not supported\n");
    if (dwFlags & PROGDLG_NOTIME)
        FIXME("Flags PROGDLG_NOTIME not supported\n");

    InitCommonControlsEx( &init );

    CComCritSecLock<CComCriticalSection> lock(m_cs, true);

    if (m_hwnd)
        return S_OK;  /* as on XP */

    m_dwFlags = dwFlags;
    m_hwndParent = hwndParent;
    m_hThreadStartEvent = CreateEventW(NULL, TRUE, FALSE, NULL);

    HANDLE hThread = CreateThread(NULL, 0, dialog_thread, this, 0, NULL);
    WaitForSingleObject(m_hThreadStartEvent, INFINITE);
    CloseHandle(m_hThreadStartEvent);
    CloseHandle(hThread);

    m_hwndDisabledParent = NULL;
    if (hwndParent && (dwFlags & PROGDLG_MODAL))
    {
        HWND hwndDisable = GetAncestor(hwndParent, GA_ROOT);
        if (::EnableWindow(hwndDisable, FALSE))
            m_hwndDisabledParent = hwndDisable;
    }

    return S_OK;
}

HRESULT WINAPI CProgressDialog::StopProgressDialog()
{
    CComCritSecLock<CComCriticalSection> lock(m_cs, true);
    if (m_hwnd)
        end_dialog();

    return S_OK;
}

HRESULT WINAPI CProgressDialog::SetTitle(LPCWSTR pwzTitle)
{
    HWND hwnd;

    m_cs.Lock();
    set_buffer(m_strings[TITLE_INDEX], pwzTitle);
    m_dwUpdate |= UPDATE_TITLE;
    hwnd = m_hwnd;
    m_cs.Unlock();

    if (hwnd)
        SendMessageW(hwnd, WM_DLG_UPDATE, 0, 0);

    return S_OK;
}

HRESULT WINAPI CProgressDialog::SetAnimation(HINSTANCE hInstance, UINT uiResourceId)
{
    HWND hAnimation = GetDlgItem(IDD_PROGRESS_DLG);
    ::SetWindowLongW(hAnimation, GWL_STYLE,
        ::GetWindowLongW(hAnimation, GWL_STYLE)|ACS_TRANSPARENT|ACS_CENTER|ACS_AUTOPLAY);

    if(!Animate_OpenEx(hAnimation,hInstance,MAKEINTRESOURCEW(uiResourceId)))
        return S_FALSE;

    return S_OK;
}

BOOL WINAPI CProgressDialog::HasUserCancelled()
{
    return m_isCancelled;
}

HRESULT WINAPI CProgressDialog::SetProgress64(ULONGLONG ullCompleted, ULONGLONG ullTotal)
{    
    HWND hwnd;

    m_cs.Lock();
    m_ullTotal = ullTotal;
    m_ullCompleted = ullCompleted;

    if (GetTickCount() - m_progressClock[(m_clockHand + 29) % 30].dwTime > 20)
    {
        m_clockHand = (m_clockHand + 1) % 30;
        m_progressClock[m_clockHand].ullMark = ullCompleted;
        m_progressClock[m_clockHand].dwTime = GetTickCount();
    }

    m_dwUpdate |= UPDATE_PROGRESS;
    hwnd = m_hwnd;
    m_cs.Unlock();

    if (hwnd)
        SendMessageW(hwnd, WM_DLG_UPDATE, 0, 0);

    return S_OK;  /* Windows sometimes returns S_FALSE */
}

HRESULT WINAPI CProgressDialog::SetProgress(DWORD dwCompleted, DWORD dwTotal)
{
    return SetProgress64(dwCompleted, dwTotal);
}

HRESULT WINAPI CProgressDialog::SetLine(DWORD dwLineNum, LPCWSTR pwzLine, BOOL bPath, LPCVOID reserved)
{
    HWND hwnd;

    if (reserved)
        FIXME("reserved pointer not null (%p)\n", reserved);

    dwLineNum--;
    if (dwLineNum >= 3)  /* Windows seems to do something like that */
        dwLineNum = 0;

    m_cs.Lock();
    set_buffer(m_strings[dwLineNum], pwzLine);
    m_dwUpdate |= UPDATE_LINE1 << dwLineNum;
    hwnd = (m_isCancelled ? NULL : m_hwnd); /* no sense to send the message if window cancelled */
    m_cs.Unlock();

    if (hwnd)
        SendMessageW(hwnd, WM_DLG_UPDATE, 0, 0);

    return S_OK;
}

HRESULT WINAPI CProgressDialog::SetCancelMsg(LPCWSTR pwzMsg, LPCVOID reserved)
{    
    HWND hwnd;

    if (reserved)
        FIXME("reserved pointer not null (%p)\n", reserved);

    m_cs.Lock();
    set_buffer(m_strings[CANCEL_MSG_INDEX], pwzMsg);
    m_dwUpdate |= UPDATE_LINE1 << CANCEL_MSG_LINE;
    hwnd = (m_isCancelled ? m_hwnd : NULL); /* no sense to send the message if window not cancelled */
    m_cs.Unlock();

    if (hwnd)
        SendMessageW(hwnd, WM_DLG_UPDATE, 0, 0);

    return S_OK;
}

HRESULT WINAPI CProgressDialog::Timer(DWORD dwTimerAction, LPCVOID reserved)
{
     if (reserved)
         FIXME("Reserved field not NULL but %p\n", reserved);

    return S_OK;
}

HRESULT WINAPI CProgressDialog::GetWindow(HWND* phwnd)
{
    CComCritSecLock<CComCriticalSection> lock(m_cs, true);
    *phwnd = m_hwnd;
    return S_OK;
}

HRESULT WINAPI CProgressDialog::ContextSensitiveHelp(BOOL fEnterMode)
{
    return E_NOTIMPL;
}

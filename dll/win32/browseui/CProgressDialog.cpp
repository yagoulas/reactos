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

/* Note: to avoid a deadlock we don't want to send messages to the dialog
 * with the critical section held. Instead we only mark what fields should be
 * updated and the dialog proc does the update */
#define UPDATE_PROGRESS         0x1
#define UPDATE_TITLE            0x2
#define UPDATE_LINE1            0x4
#define UPDATE_LINE2            (UPDATE_LINE1<<1)
#define UPDATE_LINE3            (UPDATE_LINE1<<2)


#define WM_DLG_UPDATE   (WM_APP+1)  /* set to the dialog when it should update */
#define WM_DLG_DESTROY  (WM_APP+2)  /* DestroyWindow must be called from the owning thread */

#define ID_3SECONDS 101

#define BUFFER_SIZE 256

CProgressDialog::CProgressDialog()
{
    m_lines[0]  = (LPWSTR) HeapAlloc(GetProcessHeap(), 0, BUFFER_SIZE);
    m_lines[1]  = (LPWSTR) HeapAlloc(GetProcessHeap(), 0, BUFFER_SIZE);
    m_lines[2]  = (LPWSTR) HeapAlloc(GetProcessHeap(), 0, BUFFER_SIZE);
    m_cancelMsg = (LPWSTR) HeapAlloc(GetProcessHeap(), 0, BUFFER_SIZE);
    m_title     = (LPWSTR) HeapAlloc(GetProcessHeap(), 0, BUFFER_SIZE);

    m_lines[0][0] = m_lines[1][0] = m_lines[2][0] = UNICODE_NULL;
    m_cancelMsg[0] = m_title[0] = UNICODE_NULL;

    m_clockHand = -1;
    m_progressClock[29].ullMark = 0ull;
    m_dwStartTime = GetTickCount();

    InitializeCriticalSection(&m_cs);
}

CProgressDialog::~CProgressDialog()
{
    if (m_hwnd)
        end_dialog();
    HeapFree(GetProcessHeap(), 0, m_lines[0]);
    HeapFree(GetProcessHeap(), 0, m_lines[1]);
    HeapFree(GetProcessHeap(), 0, m_lines[2]);
    HeapFree(GetProcessHeap(), 0, m_cancelMsg);
    HeapFree(GetProcessHeap(), 0, m_title);
    DeleteCriticalSection(&m_cs);
}

static void set_buffer(LPWSTR *buffer, LPCWSTR string)
{
    if (!string)
    {
        (*buffer)[0] = UNICODE_NULL;
        return;
    }

    StringCbCopyW(*buffer, BUFFER_SIZE, string);
}

struct create_params
{
    CProgressDialog *This;
    HANDLE hEvent;
    HWND hwndParent;
};

static void load_string(LPWSTR *buffer, HINSTANCE hInstance, UINT uiResourceId)
{
    WCHAR string[256];

    LoadStringW(hInstance, uiResourceId, string, sizeof(string)/sizeof(string[0]));

    set_buffer(buffer, string);
}

void CProgressDialog::set_progress_marquee()
{
    HWND hProgress = GetDlgItem(m_hwnd, IDC_PROGRESS_BAR);
    SetWindowLongW(hProgress, GWL_STYLE,
        GetWindowLongW(hProgress, GWL_STYLE)|PBS_MARQUEE);
}

void CProgressDialog::update_dialog(DWORD dwUpdate)
{
    WCHAR empty[] = {0};

    if (dwUpdate & UPDATE_TITLE)
        SetWindowTextW(m_hwnd, m_title);

    if (dwUpdate & UPDATE_LINE1)
        SetDlgItemTextW(m_hwnd, IDC_TEXT_LINE, (m_isCancelled ? empty : m_lines[0]));
    if (dwUpdate & UPDATE_LINE2)
        SetDlgItemTextW(m_hwnd, IDC_TEXT_LINE+1, (m_isCancelled ? empty : m_lines[1]));
    if (dwUpdate & UPDATE_LINE3)
        SetDlgItemTextW(m_hwnd, IDC_TEXT_LINE+2, (m_isCancelled ? m_cancelMsg : m_lines[2]));

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

        SendDlgItemMessageW(m_hwnd, IDC_PROGRESS_BAR, PBM_SETRANGE32, 0, (DWORD)ullTotal);
        SendDlgItemMessageW(m_hwnd, IDC_PROGRESS_BAR, PBM_SETPOS, (DWORD)ullCompleted, 0);
    }
}

void CProgressDialog::end_dialog()
{
    SendMessageW(m_hwnd, WM_DLG_DESTROY, 0, 0);
    /* native doesn't re-enable the window? */
    if (m_hwndDisabledParent)
        EnableWindow(m_hwndDisabledParent, TRUE);
    m_hwnd = NULL;
}

static INT_PTR CALLBACK dialog_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    CProgressDialog *This = (CProgressDialog *)GetWindowLongPtrW(hwnd, DWLP_USER);

    switch (msg)
    {
        case WM_INITDIALOG:
        {
            struct create_params *params = (struct create_params *)lParam;

            /* Note: until we set the hEvent, the object is protected by
             * the critical section held by StartProgress */
            SetWindowLongPtrW(hwnd, DWLP_USER, (LONG_PTR)params->This);
            This = params->This;
            This->m_hwnd = hwnd;

            if (This->m_dwFlags & PROGDLG_NOPROGRESSBAR)
                ShowWindow(GetDlgItem(hwnd, IDC_PROGRESS_BAR), SW_HIDE);
            if (This->m_dwFlags & PROGDLG_NOCANCEL)
                ShowWindow(GetDlgItem(hwnd, IDCANCEL), SW_HIDE);
            if (This->m_dwFlags & PROGDLG_MARQUEEPROGRESS)
                This->set_progress_marquee();
            if (This->m_dwFlags & PROGDLG_NOMINIMIZE)
                SetWindowLongW(hwnd, GWL_STYLE, GetWindowLongW(hwnd, GWL_STYLE) & (~WS_MINIMIZEBOX));

            This->update_dialog(0xffffffff);
            This->m_dwUpdate = 0;
            This->m_isCancelled = FALSE;

            SetTimer(hwnd, ID_3SECONDS, 3 * 1000, NULL);
            
            SetEvent(params->hEvent);
            return TRUE;
        }

        case WM_DLG_UPDATE:
            EnterCriticalSection(&This->m_cs);
            This->update_dialog(This->m_dwUpdate);
            This->m_dwUpdate = 0;
            LeaveCriticalSection(&This->m_cs);
            return TRUE;

        case WM_DLG_DESTROY:
            DestroyWindow(hwnd);
            PostThreadMessageW(GetCurrentThreadId(), WM_NULL, 0, 0); /* wake up the GetMessage */
            KillTimer(hwnd, ID_3SECONDS);

            return TRUE;

        case WM_CLOSE:
        case WM_COMMAND:
            if (msg == WM_CLOSE || wParam == IDCANCEL)
            {
                EnterCriticalSection(&This->m_cs);
                This->m_isCancelled = TRUE;

                if (!This->m_cancelMsg[0]) {
                    load_string(&This->m_cancelMsg, _AtlBaseModule.GetResourceInstance(), IDS_CANCELLING);
                }

                This->set_progress_marquee();
                EnableWindow(GetDlgItem(This->m_hwnd, IDCANCEL), FALSE);
                This->update_dialog(UPDATE_LINE1|UPDATE_LINE2|UPDATE_LINE3);
                LeaveCriticalSection(&This->m_cs);
            }
            return TRUE;

        case WM_TIMER:
            EnterCriticalSection(&This->m_cs);
            if (This->m_progressClock[29].ullMark != 0ull) {
                // We have enough info to take a guess
                ULONGLONG sizeDiff = This->m_progressClock[This->m_clockHand].ullMark - 
                                     This->m_progressClock[(This->m_clockHand + 29) % 30].ullMark;
                DWORD     timeDiff = This->m_progressClock[This->m_clockHand].dwTime - 
                                     This->m_progressClock[(This->m_clockHand + 29) % 30].dwTime;
                DWORD      runDiff = This->m_progressClock[This->m_clockHand].dwTime - 
                                     This->m_dwStartTime;
                ULONGLONG sizeLeft = This->m_ullTotal - This->m_progressClock[This->m_clockHand].ullMark;

                // A guess for time remaining based on the recent slope.
                DWORD timeLeftD = (DWORD) timeDiff * ((double) sizeLeft) / ((double) sizeDiff);
                // A guess for time remaining based on the start time and current position
                DWORD timeLeftI = (DWORD) runDiff * ((double) sizeLeft) / ((double) This->m_progressClock[This->m_clockHand].ullMark);

                StrFromTimeIntervalW(This->m_lines[2], 128, timeLeftD * 0.3 + timeLeftI * 0.7 , 2);
                This->update_dialog( UPDATE_LINE1 << 2 );
            }
            LeaveCriticalSection(&This->m_cs);

            return TRUE;
    }
    return FALSE;
}

static DWORD WINAPI dialog_thread(LPVOID lpParameter)
{
    /* Note: until we set the hEvent in WM_INITDIALOG, the ProgressDialog object
     * is protected by the critical section held by StartProgress */
    struct create_params *params = (struct create_params *) lpParameter;
    HWND hwnd;
    MSG msg;

    hwnd = CreateDialogParamW(_AtlBaseModule.GetResourceInstance(),
                              MAKEINTRESOURCEW(IDD_PROGRESS_DLG),
                              params->hwndParent, 
                              dialog_proc, 
                             (LPARAM)params);

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

    struct create_params params;
    HANDLE hThread;

    // TRACE("(%p, %p, %x, %p)\n", this, punkEnableModeless, dwFlags, reserved);
    if (punkEnableModeless || reserved)
        FIXME("Reserved parameters not null (%p, %p)\n", punkEnableModeless, reserved);
    if (dwFlags & PROGDLG_AUTOTIME)
        FIXME("Flags PROGDLG_AUTOTIME not supported\n");
    if (dwFlags & PROGDLG_NOTIME)
        FIXME("Flags PROGDLG_NOTIME not supported\n");

    InitCommonControlsEx( &init );

    EnterCriticalSection(&m_cs);

    if (m_hwnd)
    {
        LeaveCriticalSection(&m_cs);
        return S_OK;  /* as on XP */
    }
    m_dwFlags = dwFlags;
    params.This = this;
    params.hwndParent = hwndParent;
    params.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);

    hThread = CreateThread(NULL, 0, dialog_thread, &params, 0, NULL);
    WaitForSingleObject(params.hEvent, INFINITE);
    CloseHandle(params.hEvent);
    CloseHandle(hThread);

    m_hwndDisabledParent = NULL;
    if (hwndParent && (dwFlags & PROGDLG_MODAL))
    {
        HWND hwndDisable = GetAncestor(hwndParent, GA_ROOT);
        if (EnableWindow(hwndDisable, FALSE))
            m_hwndDisabledParent = hwndDisable;
    }

    LeaveCriticalSection(&m_cs);

    return S_OK;
}

HRESULT WINAPI CProgressDialog::StopProgressDialog()
{
    EnterCriticalSection(&m_cs);
    if (m_hwnd)
        end_dialog();
    LeaveCriticalSection(&m_cs);

    return S_OK;
}

HRESULT WINAPI CProgressDialog::SetTitle(LPCWSTR pwzTitle)
{
    HWND hwnd;

    EnterCriticalSection(&m_cs);
    set_buffer(&m_title, pwzTitle);
    m_dwUpdate |= UPDATE_TITLE;
    hwnd = m_hwnd;
    LeaveCriticalSection(&m_cs);

    if (hwnd)
        SendMessageW(hwnd, WM_DLG_UPDATE, 0, 0);

    return S_OK;
}

HRESULT WINAPI CProgressDialog::SetAnimation(HINSTANCE hInstance, UINT uiResourceId)
{
    HWND hAnimation = GetDlgItem(m_hwnd, IDD_PROGRESS_DLG);
    SetWindowLongW(hAnimation, GWL_STYLE,
        GetWindowLongW(hAnimation, GWL_STYLE)|ACS_TRANSPARENT|ACS_CENTER|ACS_AUTOPLAY);

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

    EnterCriticalSection(&m_cs);
    m_ullTotal = ullTotal;
    m_ullCompleted = ullCompleted;

    if (GetTickCount() - m_progressClock[(m_clockHand + 29) % 30].dwTime > 20) {
        m_clockHand = (m_clockHand + 1) % 30;
        m_progressClock[m_clockHand].ullMark = ullCompleted;
        m_progressClock[m_clockHand].dwTime = GetTickCount();
    }

    m_dwUpdate |= UPDATE_PROGRESS;
    hwnd = m_hwnd;
    LeaveCriticalSection(&m_cs);

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

    EnterCriticalSection(&m_cs);
    set_buffer(&m_lines[dwLineNum], pwzLine);
    m_dwUpdate |= UPDATE_LINE1 << dwLineNum;
    hwnd = (m_isCancelled ? NULL : m_hwnd); /* no sense to send the message if window cancelled */
    LeaveCriticalSection(&m_cs);

    if (hwnd)
        SendMessageW(hwnd, WM_DLG_UPDATE, 0, 0);

    return S_OK;
}

HRESULT WINAPI CProgressDialog::SetCancelMsg(LPCWSTR pwzMsg, LPCVOID reserved)
{    
    HWND hwnd;

    if (reserved)
        FIXME("reserved pointer not null (%p)\n", reserved);

    EnterCriticalSection(&m_cs);
    set_buffer(&m_cancelMsg, pwzMsg);
    m_dwUpdate |= UPDATE_LINE1 << CANCEL_MSG_LINE;
    hwnd = (m_isCancelled ? m_hwnd : NULL); /* no sense to send the message if window not cancelled */
    LeaveCriticalSection(&m_cs);

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
    EnterCriticalSection(&m_cs);
    *phwnd = m_hwnd;
    LeaveCriticalSection(&m_cs);
    return S_OK;
}

HRESULT WINAPI CProgressDialog::ContextSensitiveHelp(BOOL fEnterMode)
{
    return E_NOTIMPL;
}

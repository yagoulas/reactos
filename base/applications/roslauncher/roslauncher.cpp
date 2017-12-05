#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <atlbase.h>
#include <atlcom.h>
#include <atlwin.h>
#include <atlstr.h>
#include <rosdlgs.h>
#include "resource.h"

class CLauncherModule : public CComModule
{
public:
};

BEGIN_OBJECT_MAP(ObjectMap)
END_OBJECT_MAP()

CLauncherModule gModule;
CAtlWinModule gWinModule;

const WCHAR *ImageExecOptionsString = L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\";

class CLauncher
{
public:
    CStringW strExecutable;
    CStringW strStartDir;
    CStringW strParams;
    CStringW strLogFileName;
    bool bLogToFile;
    bool bLogToConsole;
    DWORD gflags;


    bool Launch()
    {
        WriteGFlags();

        WCHAR loggerparams[MAX_PATH];
        DWORD dwCreationFlags = bLogToConsole == true ? 0 : DETACHED_PROCESS;
        const WCHAR *pStartDir = strStartDir.GetLength() == 0 ? NULL : strStartDir.GetString();

        if (bLogToFile == true)
            wsprintf(loggerparams, L"roslogger.exe -c \"%s %s\" -o %s", strExecutable.GetString(), strParams.GetString(), strLogFileName.GetString());
        else
            wsprintf(loggerparams, L"roslogger.exe -c \"%s %s\"", strExecutable.GetString(), strParams.GetString());

        PROCESS_INFORMATION pi2;
        STARTUPINFOW si2 = { 0 };

        BOOL ret = CreateProcessW(NULL, loggerparams, NULL, NULL, FALSE, dwCreationFlags, NULL, 
                             pStartDir, &si2, &pi2);
        if (!ret)
        {
            WCHAR buffer[200];
            wsprintf(buffer, L"CreateProcessW failed. ret = %d, last error: %d\n", ret, GetLastError());
            MessageBoxW(NULL, buffer, L"Error", 0);
        }

        CloseHandle(pi2.hThread);
        CloseHandle(pi2.hProcess);

        return true;
    }

    CStringW GetFileName()
    {
        CStringW FileName;
        int i = strExecutable.ReverseFind(L'\\');
        if (i != -1)
            FileName = strExecutable.Right(strExecutable.GetLength() - i - 1);
        return FileName;
    }

    void ReadGFlags()
    {
        gflags = 0;
        CStringW filename = GetFileName();
        if (filename.GetLength() > 0)
        {
            CRegKey key1, key2;
            if (key1.Open(HKEY_LOCAL_MACHINE, ImageExecOptionsString) == ERROR_SUCCESS)
            {
                if (key2.Open(key1, filename.GetString(), KEY_READ) == ERROR_SUCCESS)
                    key2.QueryDWORDValue(L"GlobalFlag", gflags);
            }
        }
    }

    void WriteGFlags()
    {
        CStringW filename = GetFileName();
        if (filename.GetLength() > 0)
        {
            CRegKey key1, key2;
            if (key1.Open(HKEY_LOCAL_MACHINE, ImageExecOptionsString) == ERROR_SUCCESS)
            {
                if (key2.Open(key1, filename.GetString(), KEY_WRITE) != ERROR_SUCCESS)
                {
                    if (gflags == 0)
                        return;

                    if (key2.Create(HKEY_LOCAL_MACHINE,filename.GetString()) != ERROR_SUCCESS)
                        return;
                }        
                key2.SetDWORDValue(L"GlobalFlag", gflags);
            }
        }
    }

};

class CMainPage : public CPropertyPageImpl<CMainPage>
{
private:
    CLauncher *m_launcher;
public: 
    enum { IDD = IDD_LAUNCHER_MAIN };

    const static UINT TimerId = 200;

    BEGIN_MSG_MAP(CMainPage)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_TIMER, OnTimer)
        COMMAND_ID_HANDLER(IDC_BROWSE, OnBrowse)
        COMMAND_ID_HANDLER(IDC_BROWSELOG, OnBrowseLog)
        COMMAND_CODE_HANDLER(EN_CHANGE , OnEditChange)
        CHAIN_MSG_MAP(CPropertyPageImpl<CMainPage>)
    END_MSG_MAP()

    LRESULT OnBrowse(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
    {
        CStringW strFileName = m_launcher->strExecutable.GetString();

        OPENFILENAMEW ofn = {sizeof(ofn)};
        ofn.hwndOwner = m_hWnd;
        ofn.hInstance = _AtlBaseModule.GetModuleInstance();
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFile = strFileName.GetBuffer(ofn.nMaxFile);
        strFileName.ReleaseBuffer();

        ofn.lpstrFilter = L"Exe files (*.exe)\0*.exe\0All Files\0*.*\0\0";
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

        if (GetOpenFileNameW(&ofn))
            SetDlgItemText(IDC_EXECUTABLE, strFileName);

        return 0;
    }

    LRESULT OnBrowseLog(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
    {
        CStringW strFileName = m_launcher->strLogFileName.GetString();

        OPENFILENAMEW ofn = {sizeof(ofn)};
        ofn.hwndOwner = m_hWnd;
        ofn.hInstance = _AtlBaseModule.GetModuleInstance();
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFile = strFileName.GetBuffer(ofn.nMaxFile);
        strFileName.ReleaseBuffer();

        ofn.lpstrFilter = L"All Files\0*.*\0\0";
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
        if (GetSaveFileNameW(&ofn))
            SetDlgItemText(IDC_LOGFILE, strFileName);

        return 0;
    }

    LRESULT OnEditChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
    {
        if (hWndCtl == GetDlgItem(IDC_EXECUTABLE))
        {    
            SetTimer(TimerId, 300);
        }
        return 0;
    }

    LRESULT OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
    {
        if (wParam == TimerId)
        {
            KillTimer(TimerId);
            GetDlgItemText(IDC_EXECUTABLE, m_launcher->strExecutable.GetBuffer(MAX_PATH), MAX_PATH);
            m_launcher->strExecutable.ReleaseBuffer();
            m_launcher->ReadGFlags();
            CheckDlgButton(IDC_LDRSNAPS, (m_launcher->gflags & 2) ? BST_CHECKED : 0);
            CheckDlgButton(IDC_DPH, (m_launcher->gflags & 0x02000000) ? BST_CHECKED : 0);
        }
        return 0;
    }

    LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
    {
        m_launcher->ReadGFlags();

        SetDlgItemText(IDC_EXECUTABLE, m_launcher->strExecutable);
        SetDlgItemText(IDC_PARAMS, m_launcher->strParams);
        SetDlgItemText(IDC_LOGFILE, m_launcher->strLogFileName);
        SetDlgItemText(IDC_STARTDIR, m_launcher->strStartDir);
        if (m_launcher->bLogToFile == true && m_launcher->bLogToConsole  == true)
            CheckDlgButton(IDC_LOGTOBOTH, BST_CHECKED);
        else if (m_launcher->bLogToFile == true)
            CheckDlgButton(IDC_LOGTOFILE, BST_CHECKED);
        else if (m_launcher->bLogToConsole == true)
            CheckDlgButton(IDC_LOGTOCON, BST_CHECKED);

        CheckDlgButton(IDC_LDRSNAPS, (m_launcher->gflags & 2) ? BST_CHECKED : 0);
        CheckDlgButton(IDC_DPH, (m_launcher->gflags & 0x02000000) ? BST_CHECKED : 0);

        return TRUE;
    }

    int OnApply()
    {
        GetDlgItemText(IDC_EXECUTABLE, m_launcher->strExecutable.GetBuffer(MAX_PATH), MAX_PATH);
        m_launcher->strExecutable.ReleaseBuffer();
        GetDlgItemText(IDC_PARAMS, m_launcher->strParams.GetBuffer(MAX_PATH), MAX_PATH);
        m_launcher->strParams.ReleaseBuffer();
        GetDlgItemText(IDC_LOGFILE, m_launcher->strLogFileName.GetBuffer(MAX_PATH), MAX_PATH);
        m_launcher->strLogFileName.ReleaseBuffer();
        GetDlgItemText(IDC_STARTDIR, m_launcher->strStartDir.GetBuffer(MAX_PATH), MAX_PATH);
        m_launcher->strStartDir.ReleaseBuffer();
        
        if (IsDlgButtonChecked(IDC_LOGTOBOTH))
        {
            m_launcher->bLogToFile = m_launcher->bLogToConsole = true;
        }
        else
        {
            m_launcher->bLogToFile = IsDlgButtonChecked(IDC_LOGTOFILE);
            m_launcher->bLogToConsole = IsDlgButtonChecked(IDC_LOGTOCON);
        }

        if (IsDlgButtonChecked(IDC_DPH))
            m_launcher->gflags |= 0x02000000;
        else
            m_launcher->gflags &= ~0x02000000;

        if (IsDlgButtonChecked(IDC_LDRSNAPS))
            m_launcher->gflags |= 2;
        else
            m_launcher->gflags &= ~2;

        return PSNRET_NOERROR;
    }    
    
    CMainPage(CLauncher *launcher):
        m_launcher(launcher)
    {
    }
    
};

class CEditDialog : public CDialogImpl<CEditDialog>
{
private:
    CStringW m_ValueType;
public:
    CStringW m_Value, m_ValueName;
    enum { IDD = IDD_EDIT_VALUE };

    BEGIN_MSG_MAP(CEditDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        COMMAND_ID_HANDLER(IDOK, OnOk)
    END_MSG_MAP()

    CEditDialog(CStringW ValueType, CStringW ValueName, CStringW Value):
        m_ValueType(ValueType), m_ValueName(ValueName), m_Value(Value)
    {
    }

    LRESULT OnInitDialog(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
        WCHAR buffer[100];
        GetWindowText(buffer, 100);
        wcscat(buffer, m_ValueType.GetString());
        SetWindowText(buffer);
        SetDlgItemText(IDC_VALUE_NAME, m_ValueName);
        SetDlgItemText(IDC_VALUE_DATA, m_Value);
    }

    LRESULT OnOk(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
    {
        GetDlgItemText(IDC_VALUE_NAME, m_ValueName.GetBuffer(MAX_PATH), MAX_PATH);
        m_ValueName.ReleaseBuffer();
        GetDlgItemText(IDC_VALUE_DATA, m_Value.GetBuffer(MAX_PATH), MAX_PATH);
        m_Value.ReleaseBuffer();
    }
};

class CEnvironmentPage : public CPropertyPageImpl<CEnvironmentPage>
{
private:
    CLauncher *m_launcher;
public: 
    enum { IDD = IDD_LAUNCHER_ENVIRONMENT };

    CEnvironmentPage(CLauncher *launcher):
        m_launcher(launcher)
    {
            
    }
};

class CChannelsPage : public CPropertyPageImpl<CChannelsPage>
{
private:
    CLauncher *m_launcher;
public: 
    enum { IDD = IDD_LAUNCHER_CHANNELS };

    CChannelsPage(CLauncher *launcher):
        m_launcher(launcher)
    {
            
    }
};

int CALLBACK PropSheetProc(HWND hwndDlg, UINT uMsg, LPARAM lParam)
{
    if (uMsg == PSCB_INITIALIZED)
    {
        SetDlgItemText(hwndDlg, IDOK, L"Launch");
    }

    return 0;
}

static bool EditLaucherSettings(CLauncher *launcher)
{
    CMainPage mainPage(launcher);
    CEnvironmentPage envPage(launcher);
    CChannelsPage channelsPage(launcher);

    HPROPSHEETPAGE hpsp[3];
    hpsp[0] = mainPage.Create();
    hpsp[1] = envPage.Create();
    hpsp[2] = channelsPage.Create();

    PROPSHEETHEADER psh = {sizeof(psh), PSH_NOAPPLYNOW | PSH_USECALLBACK};
    psh.pfnCallback = PropSheetProc;
    psh.pszCaption = L"Reactos logger";
    psh.nPages = _countof(hpsp);
    psh.phpage = hpsp;

    return (PropertySheet(&psh) >= 1);
}

INT WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, INT nShowCmd)
{
    gModule.Init(ObjectMap, hInstance, NULL);
    InitCommonControls();

    CLauncher launcher;

    if (lpCmdLine && lpCmdLine[0])
        launcher.strExecutable = lpCmdLine;
    launcher.strLogFileName = L"c:\\log.txt";
    launcher.bLogToFile = launcher.bLogToConsole = true;
    
    if (!EditLaucherSettings(&launcher))
        return 0;

    if (!launcher.Launch())
        MessageBoxW(NULL, L"Launch failed\n", L"Error\n", 0);

    return 0;
}
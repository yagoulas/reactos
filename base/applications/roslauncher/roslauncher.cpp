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

class Launcher
{
public:
    CStringW strExecutable;
    CStringW strStartDir;
    CStringW strParams;
    CStringW strLogFileName;
    bool bLogToFile;
    bool bLogToConsole;

    bool Launch()
    {
        PROCESS_INFORMATION pi;
        STARTUPINFOW si = { 0 };
        BOOL ret;

        ret = CreateProcessW(strExecutable.GetString(), 
                             (LPWSTR)strParams.GetString(), NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, 
                             strStartDir.GetString(), &si, &pi);
        if (!ret)
            return ret;

        WCHAR loggerparams[MAX_PATH];
        DWORD dwCreationFlags = bLogToConsole == true ? 0 : DETACHED_PROCESS;
        if (bLogToFile == true)
            wsprintf(loggerparams, L"-p %d -r %d -o %s", pi.dwProcessId, pi.dwThreadId, strLogFileName.GetString());
        else
            wsprintf(loggerparams, L"-p %d -r %d", pi.dwProcessId, pi.dwThreadId);

        PROCESS_INFORMATION pi2;
        STARTUPINFOW si2 = { 0 };

        ret = CreateProcessW(L"roslogger.exe", loggerparams, NULL, NULL, FALSE, dwCreationFlags, NULL, 
                             NULL, &si2, &pi2);

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        CloseHandle(pi2.hThread);
        CloseHandle(pi2.hProcess);

        return true;
    }

};

class CMainPage : public CPropertyPageImpl<CMainPage>
{
private:
    Launcher *m_launcher;
public: 
    enum { IDD = IDD_LAUNCHER_MAIN };

    BEGIN_MSG_MAP(CMainPage)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        COMMAND_ID_HANDLER(IDC_BROWSE, OnBrowse)
        COMMAND_ID_HANDLER(IDC_BROWSELOG, OnBrowseLog)
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

    LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
    {
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

        return PSNRET_NOERROR;
    }    
    
    CMainPage(Launcher *launcher):
        m_launcher(launcher)
    {
    }
    
};

class CEnvironmentPage : public CPropertyPageImpl<CEnvironmentPage>
{
private:
    Launcher *m_launcher;
public: 
    enum { IDD = IDD_LAUNCHERS_ENVIRONMENT };

    CEnvironmentPage(Launcher *launcher):
        m_launcher(launcher)
    {
            
    }
};

class CChannelsPage : public CPropertyPageImpl<CChannelsPage>
{
private:
    Launcher *m_launcher;
public: 
    enum { IDD = IDD_LAUNCHER_CHANNELS };

    CChannelsPage(Launcher *launcher):
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

static bool EditLaucherSettings(Launcher *launcher)
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

    Launcher launcher;

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
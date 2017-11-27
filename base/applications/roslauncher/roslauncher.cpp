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

struct LaucherSettings
{
    CStringW strExecutable;
    CStringW strStartDir;
    CStringW strParams;
    CStringW strLogFileName;
    bool bLogToFile;
    bool bLogToConsole;
};

class CMainPage : public CPropertyPageImpl<CMainPage>
{
private:
    LaucherSettings *m_settings;
public: 
    enum { IDD = IDD_LAUNCHER_MAIN };

    BEGIN_MSG_MAP(CMainPage)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        COMMAND_ID_HANDLER(IDC_BROWSE, OnBrowse)
        CHAIN_MSG_MAP(CPropertyPageImpl<CMainPage>)
    END_MSG_MAP()

    LRESULT OnBrowse(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
    {
        OPENFILENAMEW ofn = {sizeof(ofn)};
        CStringW strFileName;

        ofn.hwndOwner = m_hWnd;
        ofn.hInstance = _AtlBaseModule.GetModuleInstance();
        ofn.lpstrFilter = L"Exe files (*.exe)\0*.exe\0All Files\0*.*\0\0";
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFile = strFileName.GetBuffer(ofn.nMaxFile);
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;

        strFileName.ReleaseBuffer();

        if (GetOpenFileNameW(&ofn))
            SetDlgItemText(IDC_EXECUTABLE, strFileName);

        return 0;
    }

    LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
    {
        SetDlgItemText(IDC_EXECUTABLE, m_settings->strExecutable);
        SetDlgItemText(IDC_PARAMS, m_settings->strParams);
        SetDlgItemText(IDC_LOGFILE, m_settings->strLogFileName);
        SetDlgItemText(IDC_STARTDIR, m_settings->strStartDir);
        if (m_settings->bLogToFile == true && m_settings->bLogToConsole  == true)
            CheckDlgButton(IDC_LOGTOBOTH, BST_CHECKED);
        else if (m_settings->bLogToFile == true)
            CheckDlgButton(IDC_LOGTOFILE, BST_CHECKED);
        else if (m_settings->bLogToConsole == true)
            CheckDlgButton(IDC_LOGTOCON, BST_CHECKED);

        return TRUE;
    }

    int OnApply()
    {
        GetDlgItemText(IDC_EXECUTABLE, m_settings->strExecutable.GetBuffer(MAX_PATH), MAX_PATH);
        m_settings->strExecutable.ReleaseBuffer();
        GetDlgItemText(IDC_PARAMS, m_settings->strParams.GetBuffer(MAX_PATH), MAX_PATH);
        m_settings->strParams.ReleaseBuffer();
        GetDlgItemText(IDC_LOGFILE, m_settings->strLogFileName.GetBuffer(MAX_PATH), MAX_PATH);
        m_settings->strLogFileName.ReleaseBuffer();
        GetDlgItemText(IDC_STARTDIR, m_settings->strStartDir.GetBuffer(MAX_PATH), MAX_PATH);
        m_settings->strStartDir.ReleaseBuffer();
        
        if (IsDlgButtonChecked(IDC_LOGTOBOTH))
        {
            m_settings->bLogToFile = m_settings->bLogToConsole = true;
        }
        else
        {
            m_settings->bLogToFile = IsDlgButtonChecked(IDC_LOGTOFILE);
            m_settings->bLogToConsole = IsDlgButtonChecked(IDC_LOGTOCON);
        }

        return PSNRET_NOERROR;
    }    
    
    CMainPage(LaucherSettings *settings):
        m_settings(settings)
    {
    }
    
};

class CEnvironmentPage : public CPropertyPageImpl<CEnvironmentPage>
{
private:
    LaucherSettings *m_settings;
public: 
    enum { IDD = IDD_LAUNCHERS_ENVIRONMENT };

    CEnvironmentPage(LaucherSettings *settings):
        m_settings(settings)
    {
            
    }
};

class CChannelsPage : public CPropertyPageImpl<CChannelsPage>
{
private:
    LaucherSettings *m_settings;
public: 
    enum { IDD = IDD_LAUNCHER_CHANNELS };

    CChannelsPage(LaucherSettings *settings):
        m_settings(settings)
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

static bool EditLaucherSettings(LaucherSettings* settings)
{
    CMainPage mainPage(settings);
    CEnvironmentPage envPage(settings);
    CChannelsPage channelsPage(settings);

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

static void Launch(LaucherSettings& settings)
{
    PROCESS_INFORMATION pi;
    STARTUPINFOW si = { 0 };
    BOOL ret;

    ret = CreateProcessW(settings.strExecutable.GetString(), 
                         (LPWSTR)settings.strParams.GetString(), NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, 
                         settings.strStartDir.GetString(), &si, &pi);
    if (!ret)
        MessageBox(NULL, L"CreateProcessW failed!\n", L"Error", 0);

    WCHAR loggerparams[MAX_PATH];
    DWORD dwCreationFlags = settings.bLogToConsole == true ? 0 : DETACHED_PROCESS;
    if (settings.bLogToFile == true)
        wsprintf(loggerparams, L"-p %d -o %s -r %d", pi.dwProcessId, settings.strLogFileName.GetString(), pi.dwThreadId);
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
}

INT WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, INT nShowCmd)
{
    gModule.Init(ObjectMap, hInstance, NULL);
    InitCommonControls();

    LaucherSettings settings;

    if (lpCmdLine && lpCmdLine[0])
        settings.strExecutable = lpCmdLine;
    settings.strLogFileName = L"c:\\log.txt";
    settings.bLogToFile = settings.bLogToConsole = true;
    
    if (!EditLaucherSettings(&settings))
        return 0;

    Launch(settings);
    
    return 0;
}
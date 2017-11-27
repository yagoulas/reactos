#include <windows.h>
#include <commctrl.h>
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
        CHAIN_MSG_MAP(CPropertyPageImpl<CMainPage>)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
    {
        SetDlgItemText(IDC_EDIT1, m_settings->strExecutable);
        SetDlgItemText(IDC_EDIT2, m_settings->strParams);
        SetDlgItemText(IDC_EDIT3, m_settings->strLogFileName);
        SetDlgItemText(IDC_EDIT4, m_settings->strStartDir);
        if (m_settings->bLogToFile == true && m_settings->bLogToConsole  == true)
            CheckDlgButton(IDC_RADIO3, BST_CHECKED);
        else if (m_settings->bLogToFile == true)
            CheckDlgButton(IDC_RADIO2, BST_CHECKED);
        else if (m_settings->bLogToConsole == true)
            CheckDlgButton(IDC_RADIO1, BST_CHECKED);

        return TRUE;
    }

    int OnApply()
    {
        GetDlgItemText(IDC_EDIT1, m_settings->strExecutable.GetBuffer(MAX_PATH), MAX_PATH);
        m_settings->strExecutable.ReleaseBuffer();
        GetDlgItemText(IDC_EDIT2, m_settings->strParams.GetBuffer(MAX_PATH), MAX_PATH);
        m_settings->strParams.ReleaseBuffer();
        GetDlgItemText(IDC_EDIT3, m_settings->strLogFileName.GetBuffer(MAX_PATH), MAX_PATH);
        m_settings->strLogFileName.ReleaseBuffer();
        GetDlgItemText(IDC_EDIT4, m_settings->strStartDir.GetBuffer(MAX_PATH), MAX_PATH);
        m_settings->strStartDir.ReleaseBuffer();
        
        if (IsDlgButtonChecked(IDC_RADIO3))
        {
            m_settings->bLogToFile = m_settings->bLogToConsole = true;
        }
        else
        {
            m_settings->bLogToFile = IsDlgButtonChecked(IDC_RADIO2);
            m_settings->bLogToConsole = IsDlgButtonChecked(IDC_RADIO1);
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

static void EditLaucherSettings(LaucherSettings* settings)
{
    PROPSHEETHEADER psh;
    HPROPSHEETPAGE hpsp[3];
    CMainPage mainPage(settings);
    CEnvironmentPage envPage(settings);
    CChannelsPage channelsPage(settings);

    hpsp[0] = mainPage.Create();
    hpsp[1] = envPage.Create();
    hpsp[2] = channelsPage.Create();

    ZeroMemory(&psh, sizeof(psh));
    psh.dwSize = sizeof(psh);
    psh.dwFlags = PSH_NOAPPLYNOW | PSH_USECALLBACK;
    psh.pfnCallback = PropSheetProc;
    psh.hwndParent = NULL;
    psh.hInstance = NULL;
    psh.hIcon = NULL;
    psh.pszCaption = L"Reactos logger";
    psh.nPages = _countof(hpsp);
    psh.nStartPage = 0;
    psh.phpage = hpsp;

    PropertySheet(&psh);
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
    
    EditLaucherSettings(&settings);

    Launch(settings);
    
    return 0;
}
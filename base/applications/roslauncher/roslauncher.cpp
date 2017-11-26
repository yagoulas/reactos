#include <windows.h>
#include <commctrl.h>
#include <atlbase.h>
#include <atlcom.h>
#include <atlwin.h>
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

class CMainPage : public CPropertyPageImpl<CMainPage>
{
public: 
    enum { IDD = IDD_LAUNCHER_MAIN };

};

class CEnvironmentPage : public CPropertyPageImpl<CEnvironmentPage>
{
public: 
    enum { IDD = IDD_LAUNCHERS_ENVIRONMENT };

};

class CChannelsPage : public CPropertyPageImpl<CChannelsPage>
{
public: 
    enum { IDD = IDD_LAUNCHER_CHANNELS };

};

int CALLBACK PropSheetProc(HWND hwndDlg, UINT uMsg, LPARAM lParam)
{
    if (uMsg == PSCB_INITIALIZED)
    {
        SetDlgItemText(hwndDlg, IDOK, L"Launch");
    }

    return 0;
}

INT WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, INT nShowCmd)
{
    gModule.Init(ObjectMap, hInstance, NULL);
    InitCommonControls();

    PROPSHEETHEADER psh;
    HPROPSHEETPAGE hpsp[3];
    CMainPage mainPage;
    CEnvironmentPage envPage;
    CChannelsPage channelsPage;

    hpsp[0] = mainPage.Create();
    hpsp[1] = envPage.Create();
    hpsp[2] = channelsPage.Create();

    ZeroMemory(&psh, sizeof(psh));
    psh.dwSize = sizeof(psh);
    psh.dwFlags = PSH_NOAPPLYNOW | PSH_USECALLBACK;
    psh.pfnCallback = PropSheetProc;
    psh.hwndParent = NULL;
    psh.hInstance = hInstance;
    psh.hIcon = NULL;
    psh.pszCaption = L"Reactos logger";
    psh.nPages = _countof(hpsp);
    psh.nStartPage = 0;
    psh.phpage = hpsp;

    PropertySheet(&psh);

    return 0;
}
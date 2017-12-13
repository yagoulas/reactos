#define WIN32_NO_STATUS

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <atlbase.h>
#include <atlcom.h>
#include <atlwin.h>
#include <atlstr.h>
#include <atlsimpcoll.h>
#include <rosdlgs.h>
#include <shlobj.h>
#include <shellutils.h>
#define NTOS_MODE_USER
#include <ndk/rtlfuncs.h>
#include <userenv.h>
#include <strsafe.h>
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
    CStringW strDbgChannels;
    bool bLogToFile;
    bool bLogToConsole;
    DWORD gflags;
    PWSTR pEnvirnment;

    CLauncher() :
        bLogToFile(FALSE),
        bLogToConsole(TRUE),
        gflags(0),
        pEnvirnment(NULL)
    {
    }

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

        if (strDbgChannels.GetLength() > 0)
        {
            UNICODE_STRING Name, Value;
            RtlInitUnicodeString(&Name, L"DEBUGCHANNEL");
            RtlInitUnicodeString(&Value, strDbgChannels.GetString());
            if (pEnvirnment)
                RtlSetEnvironmentVariable(&pEnvirnment, &Name, &Value);
            else
                RtlSetEnvironmentVariable(NULL, &Name, &Value);
        }

        if(pEnvirnment)
            dwCreationFlags |= CREATE_UNICODE_ENVIRONMENT;

        BOOL ret = CreateProcessW(NULL, loggerparams, NULL, NULL, FALSE, dwCreationFlags, pEnvirnment,
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
        COMMAND_ID_HANDLER(IDC_BROWSELOG, OnBrowse)
        COMMAND_CODE_HANDLER(EN_CHANGE , OnEditChange)
        CHAIN_MSG_MAP(CPropertyPageImpl<CMainPage>)
    END_MSG_MAP()

    LRESULT OnBrowse(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
    {
        WCHAR buffer[MAX_PATH];
        if (wID == IDC_BROWSE)
            wcscpy(buffer, m_launcher->strExecutable.GetString());
        else
            wcscpy(buffer, m_launcher->strLogFileName.GetString());

        OPENFILENAMEW ofn = {sizeof(ofn)};
        ofn.hwndOwner = m_hWnd;
        ofn.hInstance = _AtlBaseModule.GetModuleInstance();
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFile = buffer;

        if (wID == IDC_BROWSE)
        {
            ofn.lpstrFilter = L"Exe files (*.exe)\0*.exe\0All Files\0*.*\0\0";
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
            if (GetOpenFileNameW(&ofn))
                SetDlgItemText(IDC_EXECUTABLE, buffer);
        }
        else
        {
            ofn.lpstrFilter = L"All Files\0*.*\0\0";
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
            if (GetSaveFileNameW(&ofn))
                SetDlgItemText(IDC_LOGFILE, buffer);
        }
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
            GetDlgItemText(IDC_EXECUTABLE, m_launcher->strExecutable);
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
        GetDlgItemText(IDC_EXECUTABLE, m_launcher->strExecutable);
        GetDlgItemText(IDC_PARAMS, m_launcher->strParams);
        GetDlgItemText(IDC_LOGFILE, m_launcher->strLogFileName);
        GetDlgItemText(IDC_STARTDIR, m_launcher->strStartDir);
        
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
        COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
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
        return 0;
    }

    LRESULT OnCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
    {
        EndDialog(FALSE);
        return 0;
    }

    LRESULT OnOk(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
    {
        GetDlgItemText(IDC_VALUE_NAME, m_ValueName);
        GetDlgItemText(IDC_VALUE_DATA, m_Value);
        EndDialog(TRUE);
        return 0;
    }
};

struct ListItem
{
    CStringW Value;
    CStringW ValueName;
};

class CListEditor: CWindow
{
private:
    CStringW m_ValueType;

public:
    CSimpleArray<ListItem> m_items;

    void Init(HWND hWnd, CSimpleArray<ListItem> items, CStringW ValueType)
    {
        m_ValueType = ValueType;
        m_items = items;
        m_hWnd = hWnd;
        for (int i = 0; i < m_items.GetSize(); i++)
            AddItem(m_items[i], FALSE);

        int tabstop = 100;
        SendMessageW(LB_SETTABSTOPS, 1, (LPARAM)&tabstop);
    }

    void AddItem(ListItem& item, BOOL bAddToList = TRUE, int index = -1)
    {
        if (bAddToList)
            m_items.Add(item);

        CStringW itemstr = item.ValueName;
        itemstr += L'\t';
        itemstr += item.Value;
        SendMessageW(LB_INSERTSTRING , index, (LPARAM)itemstr.GetString());
    }

    void RemoveItem()
    {
        int i = SendMessageW(LB_GETCURSEL, 0, 0);
        if (i < 0)
            return;
        m_items.RemoveAt(i);
        SendMessageW(LB_DELETESTRING, i, NULL);
    }

    void EditItem()
    {
        int i = SendMessageW(LB_GETCURSEL, 0, 0);
        if (i < 0)
            return;
        CEditDialog editor(m_ValueType, m_items[i].ValueName, m_items[i].Value);
        if (editor.DoModal(m_hWnd))
        {
            m_items[i].ValueName = editor.m_ValueName;
            m_items[i].Value = editor.m_Value;
            SendMessageW(LB_DELETESTRING, i, NULL);
            AddItem(m_items[i], FALSE, i);
        }
    }
};

class CEnvironmentPage : public CPropertyPageImpl<CEnvironmentPage>
{
private:
    CLauncher *m_launcher;
    CListEditor m_editor;
    PWSTR m_EnvBlock;
    CSimpleArray<ListItem> m_items;
public: 
    enum { IDD = IDD_LAUNCHER_ENVIRONMENT };

    BEGIN_MSG_MAP(CEnvironmentPage)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        COMMAND_ID_HANDLER(IDC_ADD, OnAdd)
        COMMAND_ID_HANDLER(IDC_EDIT, OnEdit)
        COMMAND_ID_HANDLER(IDC_REMOVE, OnRemove)
        COMMAND_CODE_HANDLER(LBN_DBLCLK, OnEdit)
        CHAIN_MSG_MAP(CPropertyPageImpl<CEnvironmentPage>)
    END_MSG_MAP()

    CEnvironmentPage(CLauncher *launcher):
        m_launcher(launcher)
    {
        m_EnvBlock = launcher->pEnvirnment;
    }

    LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
    {
        if (!m_EnvBlock)
        {
            CreateEnvironmentBlock((LPVOID *)&m_EnvBlock, NULL, TRUE);

            PWCHAR psep, penv = (PWCHAR)m_EnvBlock;
            WCHAR buffer[MAX_PATH];
            while(*penv)
            {
                ListItem item;
                StringCchCopy(buffer, MAX_PATH, penv);
                psep = wcschr(buffer, L'=');
                if(psep)
                {
                    *psep = NULL;
                    psep++;
                    item.Value = psep;
                }
                item.ValueName = buffer;
                m_items.Add(item);
                penv += wcslen(penv)+1;
            }
        }

        m_editor.Init(GetDlgItem(IDC_ENVLIST), m_items, L"environment variable");
        return TRUE;
    }

    LRESULT OnAdd(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
    {
        ListItem newitem;
        GetDlgItemText(IDC_NEWVAR, newitem.ValueName);
        m_editor.AddItem(newitem);
        return 0;
    }

    LRESULT OnEdit(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
    {
        m_editor.EditItem();
        return 0;
    }

    LRESULT OnRemove(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
    {
        m_editor.RemoveItem();
        return 0;
    }

    int OnApply()
    {
        if (m_EnvBlock)
        {
            RtlDestroyEnvironment(m_EnvBlock);
            RtlCreateEnvironment(FALSE, &m_EnvBlock);
            m_items = m_editor.m_items;
            for (int i = 0; i < m_items.GetSize(); i++)
            {
                if (m_items[i].ValueName.GetLength() > 0)
                {
                    UNICODE_STRING Name, Value;
                    RtlInitUnicodeString(&Name, m_items[i].ValueName.GetString());
                    RtlInitUnicodeString(&Value, m_items[i].Value.GetString());
                    RtlSetEnvironmentVariable(&m_EnvBlock, &Name, &Value);
                }
            }
        }
        m_launcher->pEnvirnment = m_EnvBlock;

        return PSNRET_NOERROR;
    }
};

class CChannelsStringList :
    public CComObjectRootEx<CComMultiThreadModelNoCS>,
    public IEnumString
{
public:
    CChannelsStringList()
    {
        m_pTxtFile = NULL;

        HMODULE hmod = _AtlBaseModule.GetModuleInstance();
        if (!hmod)
            return;

        HRSRC hrsc = FindResourceW(hmod, MAKEINTRESOURCE(ID_CHANNELS), L"TXT");
        if (!hrsc)
            return;

        HGLOBAL hResData = LoadResource(hmod, hrsc);
        if (!hResData)
            return;

        m_TxtFileSize = SizeofResource(hmod, hrsc);
        if (!m_TxtFileSize)
            return;

        m_pTxtFile = (LPCWSTR)LockResource(hResData);

        Reset();
    }

    ~CChannelsStringList()
    {
    }

    virtual HRESULT STDMETHODCALLTYPE Next(ULONG celt, LPOLESTR *rgelt, ULONG *pceltFetched)
    {
        if (pceltFetched)
            *pceltFetched = 0;

        while (m_pTxtFile && celt && m_pCurrent < m_pTxtFile + m_TxtFileSize)
        {
            LPCWSTR pSeparator = wcschr(m_pCurrent, L'\n');
            if (!pSeparator)
                break;

            ULONG Size = pSeparator - m_pCurrent + 1;

            *rgelt = (LPOLESTR)::CoTaskMemAlloc(Size * sizeof(WCHAR));
            StringCchCopyW(*rgelt, Size, m_pCurrent);

            if (pceltFetched)
                (*pceltFetched)++;

            m_pCurrent = pSeparator + 1;
            celt--;
            rgelt++;
        }
        return celt ? S_FALSE : S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE Skip(ULONG celt)
    {
        while (m_pTxtFile && celt && m_pCurrent < m_pTxtFile + m_TxtFileSize)
        {
            m_pCurrent = wcschr(m_pCurrent, L'\n');
            if (!m_pCurrent)
                break;
            m_pCurrent++;
            --celt;
        }
        return celt ? S_FALSE : S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE Reset()
    {
        if (!m_pTxtFile)
            return E_FAIL;

        m_pCurrent = m_pTxtFile;
        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE Clone(IEnumString **ppenum)
    {
        return E_NOTIMPL;
    }

protected:
    LPCWSTR m_pTxtFile;
    LPCWSTR m_pCurrent;
    DWORD m_TxtFileSize;

public:
    BEGIN_COM_MAP(CChannelsStringList)
        COM_INTERFACE_ENTRY_IID(IID_IEnumString, IEnumString)
    END_COM_MAP()
};

class CChannelsPage : public CPropertyPageImpl<CChannelsPage>
{
private:
    CLauncher *m_launcher;
    CListEditor m_editor;
    CSimpleArray<ListItem> m_items;
    CComPtr<IAutoComplete> pAC;
    CComPtr<IEnumString> pES;
public: 
    enum { IDD = IDD_LAUNCHER_CHANNELS };

    CChannelsPage(CLauncher *launcher):
        m_launcher(launcher)
    {
        CoCreateInstance(CLSID_AutoComplete, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARG(IAutoComplete, &pAC));
        ShellObjectCreator<CChannelsStringList>(IID_PPV_ARG(IEnumString, &pES));
    }

    BEGIN_MSG_MAP(CChannelsPage)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        COMMAND_ID_HANDLER(IDC_ADD, OnAdd)
        COMMAND_ID_HANDLER(IDC_EDIT, OnEdit)
        COMMAND_ID_HANDLER(IDC_REMOVE, OnRemove)
        COMMAND_CODE_HANDLER(LBN_DBLCLK , OnEdit)
        CHAIN_MSG_MAP(CPropertyPageImpl<CChannelsPage>)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
    {
        m_editor.Init(GetDlgItem(IDC_CHNLLIST), m_items, L"debug channel");
        if (pAC && pES)
        {
            HWND hwndCombo = GetDlgItem(IDC_NEWCHNL);
            HWND hwndEdit = ::GetWindow(hwndCombo, GW_CHILD);

            while (TRUE)
            {
                CComHeapPtr<OLECHAR> str;
                HRESULT hr = pES->Next(1, &str, NULL);
                if (hr != S_OK)
                    break;
                ComboBox_AddString(hwndCombo, str);
            }
            pES->Reset();

            pAC->Init(hwndEdit, pES, NULL, NULL);
        }
        SetModified();
        return TRUE;
    }

    LRESULT OnAdd(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
    {
        ListItem newitem;
        GetDlgItemText(IDC_NEWCHNL, newitem.ValueName);
		newitem.Value = L"+";
        m_editor.AddItem(newitem);
        return 0;
    }

    LRESULT OnEdit(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
    {
        m_editor.EditItem();
        return 0;
    }

    LRESULT OnRemove(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
    {
        m_editor.RemoveItem();
        return 0;
    }

    int OnApply()
    {
        CStringW channelsstr;
        m_items = m_editor.m_items;
        for (int i = 0; i < m_items.GetSize(); i++)
        {
			channelsstr += m_items[i].Value;
			channelsstr += m_items[i].ValueName;
            channelsstr += L',';
        }

        m_launcher->strDbgChannels = channelsstr;

        return PSNRET_NOERROR;
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
	CoInitialize(NULL);

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
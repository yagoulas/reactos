/*
 * PROJECT:     ReactOS Shell
 * LICENSE:     LGPL-2.1-or-later (https://spdx.org/licenses/LGPL-2.1-or-later)
 * PURPOSE:     CNetConnectionPropertyUi: Network connection configuration dialog
 * COPYRIGHT:   Copyright 2008 Johannes Anderwald (johannes.anderwald@reactos.org)
 */

#include "precomp.h"

CNetConnectionPropertyUi::CNetConnectionPropertyUi() :
    m_pProperties(NULL)
{
}

CNetConnectionPropertyUi::~CNetConnectionPropertyUi()
{
    if (m_pNCfg)
        m_pNCfg->Uninitialize();

    if (m_pProperties)
        NcFreeNetconProperties(m_pProperties);
}

HRESULT WINAPI CNetConnectionPropertyUi::FinalConstruct()
{
    HRESULT hr = CoCreateInstance(CLSID_CNetCfg, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARG(INetCfg, &m_pNCfg));
    if (FAILED_UNEXPECTEDLY(hr))
        return hr;

    hr = m_pNCfg->QueryInterface(IID_PPV_ARG(INetCfgLock, &m_NCfgLock));
    if (FAILED_UNEXPECTEDLY(hr))
        return hr;

    CComHeapPtr<WCHAR> pDisplayName;
    hr = m_NCfgLock->AcquireWriteLock(100, L"", &pDisplayName);
    if (hr == S_FALSE)
        return hr;

    hr = m_pNCfg->Initialize(NULL);
    if (FAILED_UNEXPECTEDLY(hr))
        return hr;

    return S_OK;
}
    
VOID
CNetConnectionPropertyUi::_AddItemToListView(PNET_ITEM pItem, LPWSTR szName, BOOL bChecked)
{
    LVITEMW lvItem = {0};
    lvItem.mask  = LVIF_TEXT | LVIF_PARAM;
    lvItem.pszText = szName;
    lvItem.lParam = (LPARAM)pItem;
    lvItem.iItem = m_ListView.GetItemCount();
    lvItem.iItem = m_ListView.InsertItem(&lvItem);
    m_ListView.SetCheckState(lvItem.iItem, bChecked);
}

BOOL
CNetConnectionPropertyUi::_GetNetCfgAdapterComponent()
{
    HRESULT hr;
    ULONG Fetched;
    CComPtr<IEnumNetCfgComponent> pEnumCfg;
    int res;

    hr = m_pNCfg->EnumComponents(&GUID_DEVCLASS_NET, &pEnumCfg);
    if (FAILED_UNEXPECTEDLY(hr))
        return FALSE;

    while (TRUE)
    {
        CComPtr<INetCfgComponent> pNCg;
        hr = pEnumCfg->Next(1, &pNCg, &Fetched);
        if (hr != S_OK)
            break;

        CComHeapPtr<WCHAR> pName;
        hr = pNCg->GetDisplayName(&pName);
        if (SUCCEEDED(hr))
        {
            res = _wcsicmp(pName, m_pProperties->pszwDeviceName);
            if (!res)
            {
                m_pAdapterCfgComp = pNCg;
                return TRUE;
            }
        }
    }
    return FALSE;
}

VOID
CNetConnectionPropertyUi::_EnumComponents(const GUID *CompGuid, UINT Type)
{
    HRESULT hr;
    CComPtr<IEnumNetCfgComponent> pENetCfg;
    ULONG Num;
    DWORD dwCharacteristics;
    PNET_ITEM pItem;
    BOOL bChecked;

    hr = m_pNCfg->EnumComponents(CompGuid, &pENetCfg);
    if (FAILED_UNEXPECTEDLY(hr))
        return;

    while (TRUE)
    {
        CComPtr<INetCfgComponent> pNCfgComp;
        CComPtr<INetCfgComponentBindings> pCompBind;
        CComHeapPtr<WCHAR> pDisplayName;
        CComHeapPtr<WCHAR> pHelpText;

        hr = pENetCfg->Next(1, &pNCfgComp, &Num);
        if (hr != S_OK)
            break;

        hr = pNCfgComp->GetCharacteristics(&dwCharacteristics);
        if (SUCCEEDED(hr) && (dwCharacteristics & NCF_HIDDEN))
            continue;

        hr = pNCfgComp->GetDisplayName(&pDisplayName);
        hr = pNCfgComp->GetHelpText(&pHelpText);
        bChecked = TRUE; //ReactOS hack
        hr = pNCfgComp->QueryInterface(IID_PPV_ARG(INetCfgComponentBindings, &pCompBind));
        if (SUCCEEDED(hr))
        {
            hr = pCompBind->IsBoundTo(m_pAdapterCfgComp);
            if (hr == S_OK)
                bChecked = TRUE;
            else
                bChecked = FALSE;
        }

        pItem = new NET_ITEM();
        if (!pItem)
            continue;

        pItem->dwCharacteristics = dwCharacteristics;
        pItem->szHelp = pHelpText;
        pItem->Type = (NET_TYPE)Type;
        pItem->pNCfgComp = pNCfgComp;
        pItem->NumPropDialogOpen = 0;

        _AddItemToListView(pItem, pDisplayName, bChecked);
    }
}

LRESULT CNetConnectionPropertyUi::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
{
    RECT rc;

    SetDlgItemText(IDC_NETCARDNAME, m_pProperties->pszwDeviceName);
    if (m_pProperties->dwCharacter & NCCF_SHOW_ICON)
    {
        /* check show item on taskbar*/
        CheckDlgButton(IDC_SHOWTASKBAR, BST_CHECKED);
    }
    if (m_pProperties->dwCharacter & NCCF_NOTIFY_DISCONNECTED)
    {
        /* check notify item */
        CheckDlgButton(IDC_NOTIFYNOCONNECTION, BST_CHECKED);
    }

    m_ListView.Attach(GetDlgItem(IDC_COMPONENTSLIST));
    m_ListView.GetClientRect(&rc);
    m_ListView.InsertColumn(0, L"", LVCFMT_FIXED_WIDTH, rc.right - rc.left, 0);
    m_ListView.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES, 
                                      LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES);

    _EnumComponents(&GUID_DEVCLASS_NETCLIENT, NET_TYPE_CLIENT);
    _EnumComponents(&GUID_DEVCLASS_NETSERVICE, NET_TYPE_SERVICE);
    _EnumComponents(&GUID_DEVCLASS_NETTRANS, NET_TYPE_PROTOCOL);

    m_ListView.SetItemState(0, LVIS_FOCUSED|LVIS_SELECTED, (UINT)-1);

    return TRUE;
}

LRESULT CNetConnectionPropertyUi::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
{
    int Count = m_ListView.GetItemCount();
    for (int i = 0; i < Count; i++)
    {
        PNET_ITEM pItem = (PNET_ITEM)m_ListView.GetItemData(i);
        delete pItem;
    }
    m_ListView.DeleteAllItems();

    return 0;
}

LRESULT CNetConnectionPropertyUi::OnPropertiesButton(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
{
    int lvIndex = m_ListView.GetNextItem(-1,  LVNI_SELECTED);
    if (lvIndex == -1)
        return 0;

    PNET_ITEM pItem = (PNET_ITEM)m_ListView.GetItemData(lvIndex);

    HRESULT hr;
    hr = pItem->pNCfgComp->RaisePropertyUi(GetParent(), NCRP_QUERY_PROPERTY_UI, (INetConnectionConnectUi*)this);
    if (SUCCEEDED(hr))
    {
        hr = pItem->pNCfgComp->RaisePropertyUi(GetParent(), NCRP_SHOW_PROPERTY_UI, (INetConnectionConnectUi*)this);
    }

    return 0;
}

int CNetConnectionPropertyUi::OnApply()
{
    WCHAR szKey[200];
    HKEY hKey;
    CComHeapPtr<WCHAR> pStr;
    DWORD dwShowIcon, dwNotifyDisconnect;
    HRESULT hr;

    if (m_pNCfg)
    {
        hr = m_pNCfg->Apply();
        if (FAILED_UNEXPECTEDLY(hr))
            return PSNRET_INVALID;
    }

    dwShowIcon = (IsDlgButtonChecked(IDC_SHOWTASKBAR) == BST_CHECKED);
    dwNotifyDisconnect = (IsDlgButtonChecked(IDC_NOTIFYNOCONNECTION) == BST_CHECKED);

    //NOTE: Windows write these setting with the undocumented INetLanConnection::SetInfo
    if (StringFromCLSID((CLSID)m_pProperties->guidId, &pStr) == ERROR_SUCCESS)
    {
        swprintf(szKey, L"SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}\\%s\\Connection", static_cast<WCHAR*>(pStr));
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, szKey, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS)
        {
            RegSetValueExW(hKey, L"ShowIcon", 0, REG_DWORD, (LPBYTE)&dwShowIcon, sizeof(DWORD));
            RegSetValueExW(hKey, L"IpCheckingEnabled", 0, REG_DWORD, (LPBYTE)&dwNotifyDisconnect, sizeof(DWORD));
            RegCloseKey(hKey);
        }
    }

    return PSNRET_NOERROR;
}

int CNetConnectionPropertyUi::OnCancel()
{
    if (m_pNCfg)
    {
        HRESULT hr = m_pNCfg->Cancel();
        if (SUCCEEDED(hr))
            return PSNRET_NOERROR;
        else
            return PSNRET_INVALID;
    }
    return PSNRET_NOERROR;

}

LRESULT CNetConnectionPropertyUi::OnLVItemChanging(INT uCode, LPNMHDR hdr, BOOL& bHandled)
{
    LPNMLISTVIEW lppl = (LPNMLISTVIEW) hdr;
    PNET_ITEM pItem = (PNET_ITEM)m_ListView.GetItemData(lppl->iItem);
    if (!pItem)
        return TRUE;

    if (!(lppl->uOldState & LVIS_FOCUSED) && (lppl->uNewState & LVIS_FOCUSED))
    {
        /* new focused item */
        if (pItem->dwCharacteristics & NCF_NOT_USER_REMOVABLE)
            ::EnableWindow(GetDlgItem(IDC_UNINSTALL), FALSE);
        else
            ::EnableWindow(GetDlgItem(IDC_UNINSTALL), TRUE);

        if (pItem->dwCharacteristics & NCF_HAS_UI)
            ::EnableWindow(GetDlgItem(IDC_PROPERTIES), TRUE);
        else
            ::EnableWindow(GetDlgItem(IDC_PROPERTIES), FALSE);

        SetDlgItemText(IDC_DESCRIPTION, pItem->szHelp);
    }

    return FALSE;
}

LRESULT CNetConnectionPropertyUi::OnConfigureButton(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
{
    CComHeapPtr<WCHAR> DeviceInstanceID;
    HRESULT hr = m_pAdapterCfgComp->GetPnpDevNodeId(&DeviceInstanceID);
    if (FAILED_UNEXPECTEDLY(hr))
        return 0;

    WCHAR wszCmd[2*MAX_PATH];
    StringCbPrintfW(wszCmd, sizeof(wszCmd), L"rundll32.exe devmgr.dll,DeviceProperties_RunDLL /DeviceID %s", DeviceInstanceID);

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    if (CreateProcessW(NULL, wszCmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
       CloseHandle(pi.hProcess);
       CloseHandle(pi.hThread);
    }

    return 0;
}

//
//  INetConnectionPropertyUi2 interface
//

HRESULT
WINAPI
CNetConnectionPropertyUi::AddPages(HWND hwndParent, LPFNADDPROPSHEETPAGE pfnAddPage, LPARAM lParam)
{
    HPROPSHEETPAGE hPage = Create();
    if (hPage && !pfnAddPage(hPage, lParam))
        DestroyPropertySheetPage(hPage);

    return S_OK;
}

HRESULT
WINAPI
CNetConnectionPropertyUi::GetIcon(
    DWORD dwSize,
    HICON *phIcon)
{
    return E_NOTIMPL;
}

//
//  INetLanConnectionUiInfo interface
//

HRESULT
WINAPI
CNetConnectionPropertyUi::GetDeviceGuid(GUID *pGuid)
{
    CopyMemory(pGuid, &m_pProperties->guidId, sizeof(GUID));
    return S_OK;
}

//
//  INetConnectionConnectUi interface
//

HRESULT
WINAPI
CNetConnectionPropertyUi::SetConnection(INetConnection* pCon)
{
    if (!pCon)
        return E_POINTER;

    m_pCon = NULL;
    if (m_pProperties)
        NcFreeNetconProperties(m_pProperties);

    HRESULT hr = pCon->GetProperties(&m_pProperties);
    if (FAILED_UNEXPECTEDLY(hr))
        return hr;

    hr = _GetNetCfgAdapterComponent();
    if (FAILED_UNEXPECTEDLY(hr))
        return hr;

    m_pCon = pCon;

    return S_OK;
}

HRESULT
WINAPI
CNetConnectionPropertyUi::Connect(
    HWND hwndParent,
    DWORD dwFlags)
{
    if (!m_pCon)
        return E_POINTER; //FIXME

    if (dwFlags & NCUC_NO_UI)
        return m_pCon->Connect();

    return E_FAIL;
}

HRESULT
WINAPI
CNetConnectionPropertyUi::Disconnect(
    HWND hwndParent,
    DWORD dwFlags)
{
    WCHAR szBuffer[100];
    swprintf(szBuffer, L"INetConnectionConnectUi_fnDisconnect flags %x\n", dwFlags);
    ::MessageBoxW(NULL, szBuffer, NULL, MB_OK);

    return S_OK;
}

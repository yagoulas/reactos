
/// CLASSID
/// {7007ACC5-3202-11D1-AAD2-00805FC1270E}
/// open network properties and wlan properties

typedef enum
{
    NET_TYPE_CLIENT = 1,
    NET_TYPE_SERVICE = 2,
    NET_TYPE_PROTOCOL = 3
} NET_TYPE;

typedef struct
{
    NET_TYPE Type;
    DWORD dwCharacteristics;
    CComHeapPtr<WCHAR> szHelp;
    CComPtr<INetCfgComponent> pNCfgComp;
    UINT NumPropDialogOpen;
} NET_ITEM, *PNET_ITEM;

class CNetConnectionPropertyUi:
    public CPropertyPageImpl<CNetConnectionPropertyUi>,
    public CComCoClass<CNetConnectionPropertyUi, &CLSID_LanConnectionUi>,
    public CComObjectRootEx<CComMultiThreadModelNoCS>,
    public INetConnectionConnectUi,
    public INetConnectionPropertyUi2,
    public INetLanConnectionUiInfo
{
    public:
        CNetConnectionPropertyUi();
        ~CNetConnectionPropertyUi();
        HRESULT WINAPI FinalConstruct();

        LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);
        LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);
        LRESULT OnLVItemChanging(INT uCode, LPNMHDR hdr, BOOL& bHandled);
        LRESULT OnPropertiesButton(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);
        LRESULT OnConfigureButton(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);
        int OnApply();
        int OnCancel();

        // INetConnectionPropertyUi2
        virtual HRESULT WINAPI AddPages(HWND hwndParent, LPFNADDPROPSHEETPAGE pfnAddPage, LPARAM lParam);
        virtual HRESULT WINAPI GetIcon(DWORD dwSize, HICON *phIcon);

        // INetLanConnectionUiInfo
        virtual HRESULT WINAPI GetDeviceGuid(GUID *pGuid);

        // INetConnectionConnectUi
        virtual HRESULT WINAPI SetConnection(INetConnection* pCon);
        virtual HRESULT WINAPI Connect(HWND hwndParent, DWORD dwFlags);
        virtual HRESULT WINAPI Disconnect(HWND hwndParent, DWORD dwFlags);

    private:
        VOID _AddItemToListView(PNET_ITEM pItem, LPWSTR szName, BOOL bChecked);
        BOOL _GetNetCfgAdapterComponent();
        VOID _EnumComponents(const GUID *CompGuid, UINT Type);

        CComPtr<INetConnection> m_pCon;
        CComPtr<INetCfgLock> m_NCfgLock;
        CComPtr<INetCfg> m_pNCfg;
        CComPtr<INetCfgComponent> m_pAdapterCfgComp;
        NETCON_PROPERTIES * m_pProperties;
        CListView m_ListView;

    public:
        enum { IDD = IDD_NETPROPERTIES };

        BEGIN_MSG_MAP(CNetConnectionPropertyUi)
            MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
            MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
            NOTIFY_CODE_HANDLER(LVN_ITEMCHANGING, OnLVItemChanging)
            COMMAND_ID_HANDLER(IDC_PROPERTIES, OnPropertiesButton)
            COMMAND_ID_HANDLER(IDC_CONFIGURE, OnConfigureButton)
            CHAIN_MSG_MAP(CPropertyPageImpl<CNetConnectionPropertyUi>)
        END_MSG_MAP()

        DECLARE_NO_REGISTRY()
        DECLARE_NOT_AGGREGATABLE(CNetConnectionPropertyUi)
        DECLARE_PROTECT_FINAL_CONSTRUCT()

        BEGIN_COM_MAP(CNetConnectionPropertyUi)
            COM_INTERFACE_ENTRY_IID(IID_INetConnectionConnectUi, INetConnectionConnectUi)
            COM_INTERFACE_ENTRY_IID(IID_INetConnectionPropertyUi, INetConnectionPropertyUi2)
            COM_INTERFACE_ENTRY_IID(IID_INetConnectionPropertyUi2, INetConnectionPropertyUi2)
            COM_INTERFACE_ENTRY_IID(IID_INetLanConnectionUiInfo, INetLanConnectionUiInfo)
        END_COM_MAP()
};

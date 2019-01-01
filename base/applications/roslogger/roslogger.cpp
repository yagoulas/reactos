#define WIN32_NO_STATUS

#include <windef.h>
#include <winbase.h>
#include <psapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <wchar.h>

#define NTOS_MODE_USER
#include <ndk/psfuncs.h>

class BaseDebugger
{
public:
    BaseDebugger()
        :m_Process(NULL)
    {
    }

    virtual DWORD OnDebugStringEvent(DEBUG_EVENT& evt)
    {
        return DBG_CONTINUE;
    }

    virtual DWORD OnLoadDllEvent(DEBUG_EVENT& evt)
    {
        return DBG_CONTINUE;
    }

    virtual DWORD OnExceptionEvent(DEBUG_EVENT& evt)
    {
        return DBG_CONTINUE;
    }
    
    virtual DWORD handle_event(DEBUG_EVENT& evt)
    {
        HANDLE close_handle = NULL;
        DWORD ret = DBG_CONTINUE;

        switch(evt.dwDebugEventCode)
        {
            case OUTPUT_DEBUG_STRING_EVENT:
                ret = OnDebugStringEvent(evt);
                break;
            case LOAD_DLL_DEBUG_EVENT:
                ret = OnLoadDllEvent(evt);
                break;
            case EXCEPTION_DEBUG_EVENT:
                ret = OnExceptionEvent(evt);
                break;
        }

        if (evt.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT)
        {
            close_handle = evt.u.CreateProcessInfo.hFile;
        }
        else if (evt.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT)
        {
            close_handle = evt.u.LoadDll.hFile;
        }

        if (close_handle)
            CloseHandle(close_handle);

        return ret;
    }
    
    BOOL debugger_loop()
    {
        BOOL ret = FALSE;

        for (;;)
        {
            DEBUG_EVENT ev = { 0 };
            ret = WaitForDebugEvent(&ev, INFINITE);

            if (ev.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT)
                m_Process = ev.u.CreateProcessInfo.hProcess;

            DWORD dwContinueStatus = handle_event(ev);

            if (ev.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT)
                break;
            
            ret = ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, dwContinueStatus);

            if (!ret)
                break;
        }
        m_Process = NULL;
        
        return ret;
    }

    BOOL run(const wchar_t* process, wchar_t* cmdline)
    {
        PROCESS_INFORMATION pi;
        STARTUPINFOW si = { 0 };
        BOOL ret;

        ret = CreateProcessW(process, cmdline, NULL, NULL, FALSE, DEBUG_PROCESS, NULL, NULL, &si, &pi);
        if (!ret)
            return ret;

        debugger_loop();

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);

        return ret;
    }

    void attach(DWORD pid)
    {
        DebugActiveProcess(pid);
    }

    HANDLE m_Process;

};

class CaptureDbgOut :public BaseDebugger
{
public:
    virtual DWORD OnDebugStringEvent(DEBUG_EVENT& evt)
    {
        DWORD dwSize = evt.u.DebugString.nDebugStringLength * (evt.u.DebugString.fUnicode ? 2 : 1);
        char* buffer = new char[dwSize];
        DWORD dwRead;
        ReadProcessMemory(m_Process, evt.u.DebugString.lpDebugStringData, buffer, dwSize, &dwRead);
        if (evt.u.DebugString.fUnicode)
        {
            log_message((wchar_t*)buffer);
        }
        else
        {
            log_message(buffer);
        }

        delete[] buffer;

        return DBG_CONTINUE;
    }

    virtual void log_message(const char* buffer)
    {
        printf("%s", buffer);
    }

    virtual void log_message(const wchar_t* buffer)
    {
        printf("%ls", buffer);
    }
};

class LdrLogger :public CaptureDbgOut
{
private:

    virtual DWORD OnLoadDllEvent(DEBUG_EVENT& evt)
    {
        HANDLE hFileMapping = CreateFileMappingA(evt.u.LoadDll.hFile, NULL, PAGE_READONLY, 0, 0, "temp");
        HANDLE hView = MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
        char fileName[MAX_PATH];
        /*DWORD length = */GetMappedFileNameA(m_Process, evt.u.LoadDll.lpBaseOfDll, fileName, MAX_PATH);
        UnmapViewOfFile(hView);
        CloseHandle(hFileMapping);
        log_message(L"Loaded dll: ");
        log_message(fileName);
        log_message("\n");
        return DBG_CONTINUE;
    }
};

void PrintStackBacktrace(FILE* output, HANDLE ProcessHandle, HANDLE ThreadHandle, CONTEXT& Context);
void BeginStackBacktrace(HANDLE ProcessHandle);
void EndStackBacktrace(HANDLE ProcessHandle);

class DbgLogger :public LdrLogger
{
private: 
    FILE * m_file;

    virtual DWORD OnExceptionEvent(DEBUG_EVENT& evt)
    {
        if (evt.u.Exception.dwFirstChance)
            log_message(L"Got first change exception:\n");
        else
            log_message(L"Got second change exception:\n");

        HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, evt.dwThreadId);
        HANDLE hProcess = OpenThread(PROCESS_ALL_ACCESS, FALSE, evt.dwProcessId);
        CONTEXT Context;
        Context.ContextFlags = CONTEXT_INTEGER | CONTEXT_CONTROL | CONTEXT_DEBUG_REGISTERS;
        GetThreadContext(hThread, &Context);
        BeginStackBacktrace(hProcess);
        PrintStackBacktrace(m_file, hProcess, hThread, Context);
        EndStackBacktrace(hProcess);

        return DBG_CONTINUE;
    }

    void log_message(const char* buffer)
    {
        printf("%s", buffer);

        if (m_file)
        {
            fprintf(m_file, "%s", buffer);
            fflush(m_file);
        }
    }

    void log_message(const wchar_t* buffer)
    {
        printf("%ls", buffer);

        if (m_file)
        {
            fprintf(m_file, "%ls", buffer);
            fflush(m_file);
        }
    }

public:
    DbgLogger(WCHAR* file_name = NULL)
    {
        if (file_name)
            m_file = _wfopen(file_name, L"a+");
        else
            m_file = NULL;
    }
};


int wmain(int argc, WCHAR **argv)
{
    DWORD pid = 0;
    WCHAR* output = NULL;
    WCHAR* cmd = NULL;

    for (int n = 0; n < argc; ++n)
    {
        WCHAR* arg = argv[n];

        if (!wcscmp(arg, L"-p"))
        {
            if (n + 1 < argc)
            {
                pid = wcstoul(argv[n+1], NULL, 10);
                n++;
            }
        }
        else if (!wcscmp(arg, L"-o"))
        {
            if (n + 1 < argc)
            {
                output = argv[n+1];
                n++;
            }
        }
        else if (!wcscmp(arg, L"-c"))
        {
            if (n + 1 < argc)
            {
                cmd = argv[n+1];
                n++;
            }
        }
    }

    if (!pid && !cmd)
    {
        printf("Expected a process id or a command\n");
        return 0;
    }
    
    DbgLogger debugger(output);
    DebugSetProcessKillOnExit(FALSE);
    
    if (pid)
    {
        debugger.attach(pid);
        debugger.debugger_loop();
    }
    else
    {
        int ret = debugger.run(cmd, NULL);
        if (!ret)
        {
            printf ("CreateProcessW failed, ret:%d, error:%lx\n", ret, GetLastError());
            return 0;
        }
    }

    printf("Log written to %ls\nPress any key to exit\n", output);
    fflush(stdin);
    _getch();

    return 0;
}

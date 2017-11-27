#include <windef.h>
#include <winbase.h>
#include <psapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

class BaseDebugger
{
public:
    BaseDebugger()
        :mProcess(NULL)
    {
    }

    virtual DWORD handle_event(DEBUG_EVENT& evt)
    {
        HANDLE close_handle = NULL;
        if (evt.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT)
        {
            close_handle = evt.u.CreateProcessInfo.hFile;
        }
        else if (evt.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT)
        {
            close_handle = evt.u.LoadDll.hFile;
        }
        else if (evt.dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
        {
            return DBG_EXCEPTION_NOT_HANDLED;
        }

        if (close_handle)
            CloseHandle(close_handle);

        return DBG_CONTINUE;
    }
    
    BOOL debugger_loop()
    {
        BOOL ret = FALSE;

        for (;;)
        {
            DEBUG_EVENT ev = { 0 };
            ret = WaitForDebugEvent(&ev, INFINITE);

            if (ev.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT)
                mProcess = ev.u.CreateProcessInfo.hProcess;

            DWORD dwContinueStatus = handle_event(ev);

            if (ev.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT)
                break;

            ret = ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, dwContinueStatus);

            if (!ret)
                break;
        }
        mProcess = NULL;
        
        return ret;
    }

    BOOL run(const char* process, char* cmdline)
    {
        PROCESS_INFORMATION pi;
        STARTUPINFOA si = { 0 };
        BOOL ret;

        ret = CreateProcessA(process, cmdline, NULL, NULL, FALSE, DEBUG_PROCESS, NULL, NULL, &si, &pi);
        if (!ret)
            return ret;

        debugger_loop();

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);

        return ret;
    }
    HANDLE mProcess;

};

class CaptureDbgOut :public BaseDebugger
{
public:
    virtual DWORD handle_event(DEBUG_EVENT& evt)
    {
        if (evt.dwDebugEventCode == OUTPUT_DEBUG_STRING_EVENT)
        {
            read_dbgstr(mProcess, evt.u.DebugString);
        }
        return BaseDebugger::handle_event(evt);
    }

    void read_dbgstr(HANDLE hProcess, OUTPUT_DEBUG_STRING_INFO& info)
    {
        DWORD dwSize = info.nDebugStringLength * (info.fUnicode ? 2 : 1);
        char* buffer = new char[dwSize];
        DWORD dwRead;
        ReadProcessMemory(hProcess, info.lpDebugStringData, buffer, dwSize, &dwRead);
        if (info.fUnicode)
        {
            log_message((wchar_t*)buffer);
        }
        else
        {
            log_message(buffer);
        }

        delete[] buffer;
    }
    
    virtual void log_message(char* buffer)
    {
        printf("%s", buffer);
    }

    virtual void log_message(wchar_t* buffer)
    {
        printf("%ls", buffer);
    }
};

class LdrLogger :public CaptureDbgOut
{
private:

    virtual DWORD handle_event(DEBUG_EVENT& evt)
    {
        if (evt.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT)
        {
            HANDLE hFileMapping = CreateFileMappingA(evt.u.LoadDll.hFile, NULL, PAGE_READONLY, 0, 0, "temp");
            HANDLE hView = MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
            char fileName[MAX_PATH];
            /*DWORD length = */GetMappedFileNameA(mProcess, evt.u.LoadDll.lpBaseOfDll, fileName, MAX_PATH);
            UnmapViewOfFile(hView);
            CloseHandle(hFileMapping);
            log_message(L"Loaded dll: ");
            log_message(fileName);
            log_message("\n");
            
        }

        if (evt.dwDebugEventCode == OUTPUT_DEBUG_STRING_EVENT)
        {
            read_dbgstr(mProcess, evt.u.DebugString);
        }

        return BaseDebugger::handle_event(evt);
    }
};

class DbgLogger :public LdrLogger
{
private: 
    FILE * file;
    
    void log_message(char* buffer)
    {
        printf("%s", buffer);
        if (file)
        {
            fprintf(file, "%s", buffer);
            fflush(file);
        }
    }

    void log_message(wchar_t* buffer)
    {
        printf("%ls", buffer);
        if (file)
        {
            fprintf(file, "%ls", buffer);
            fflush(file);
        }
    }

public:
    DbgLogger(WCHAR* file_name)
    {
        if (file_name)
            file = _wfopen(file_name, L"a+");
        else
            file = NULL;
    }
};


int wmain(int argc, WCHAR **argv)
{
    DWORD pid = 0, tid = 0;
    bool resume = false;
    WCHAR* output = NULL;

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
        else if (!wcscmp(arg, L"-r"))
        {
            resume = true;
            if (n + 1 < argc)
            {
                tid = wcstoul(argv[n+1], NULL, 10);
                n++;
            }

        }
        else if (!wcscmp(arg, L"-o"))
        {
            if (n + 1 < argc)
            {
                output = argv[n+1];
                printf("output file: %ls\n", output);
                n++;
            }
            
        }
    }

    if (!pid)
    {
        printf("Expected a process id\n");
        return 0;
    }
    
    DebugActiveProcess(pid);
    DebugSetProcessKillOnExit(FALSE);
    
    if (resume == true)
    {
        HANDLE threadHandle = OpenThread(THREAD_ALL_ACCESS , FALSE, tid);
        ResumeThread(threadHandle);
        CloseHandle(threadHandle);
    }
    
    DbgLogger debugger(output);
    debugger.debugger_loop();
    
    return 0;
}

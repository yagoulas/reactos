/*
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         ReactOS Console Server DLL
 * FILE:            win32ss/user/winsrv/consrv/shutdown.c
 * PURPOSE:         Processes Shutdown
 * PROGRAMMERS:     Alex Ionescu
 */

/* INCLUDES *******************************************************************/

#include "consrv.h"
#include <psapi.h>

#define NDEBUG
#include <debug.h>

/* FUNCTIONS ******************************************************************/

extern ULONG_PTR LogonProcessId;
extern ULONG_PTR ServicesProcessId;

static void 
NotifyConsoleProcessForShutdown(IN PCSR_PROCESS CsrProcess,
                                IN PCONSOLE_PROCESS_DATA ProcessData,
                                IN ULONG Flags)
{
    DPRINT1("ConsoleClientShutdown(0x%p, 0x%x, %s) - Console process [0x%x, 0x%x]\n",
             CsrProcess, Flags, CsrProcess->ClientId.UniqueProcess, CsrProcess->ClientId.UniqueThread);

    /* Send a log-off event. In reality this should be way more complex */
    ConSrvConsoleCtrlEventTimeout(CTRL_LOGOFF_EVENT,
                                  ProcessData,
                                  ShutdownSettings.WaitToKillAppTimeout,
                                  NULL);
}

static BOOL
NotifyGenericProcessForShutdown(IN PCSR_PROCESS CsrProcess,
                                IN PCONSOLE_PROCESS_DATA ProcessData,
                                IN ULONG Flags)
{
    NTSTATUS Status;
    ULONG EventFlags = 0;
    BOOLEAN ExitProcess;
    ULONG ThreadExitCode;
    ULONG ProcessExitCode;
    ULONG Timeout;
    ULONG EventType = (Flags & EWX_SHUTDOWN) ? CTRL_SHUTDOWN_EVENT : CTRL_LOGOFF_EVENT;

    if (CsrProcess->ShutdownFlags & (CsrShutdownSystem | CsrShutdownOther))
    {
        //
        // System context - make sure we don't cause it to exit, make
        // sure we don't bring up retry dialogs.
        //
        DPRINT("This is a system app\n");

        ExitProcess = FALSE;

        //
        // This EventFlag will be passed on down to the CtrlRoutine()
        // on the client side. That way that side knows not to exit
        // this process.
        //

        EventFlags = 0x80000000;
    }
    else
    {
        ExitProcess = TRUE;
    }

    if (EventType == CTRL_CLOSE_EVENT)
     {
        Timeout = ShutdownSettings.HungAppTimeout;
     }
    else if (EventType == CTRL_LOGOFF_EVENT)
     {
        Timeout = ShutdownSettings.WaitToKillAppTimeout;
    }
    else if (EventType == CTRL_SHUTDOWN_EVENT)
    {
        if (CsrProcess->ClientId.UniqueProcess == UlongToHandle(ServicesProcessId))
        {
            Timeout = ShutdownSettings.WaitToKillServiceTimeout;
        }
        else
        {
            Timeout = ShutdownSettings.WaitToKillAppTimeout;
        }
    }
    else
    {
        DPRINT1("failed\n");
        return ExitProcess;
    }

    DPRINT("timeout: %d\n", Timeout);

    Status = ConSrvConsoleCtrlEventTimeout(EventType | EventFlags, 
                                           ProcessData,
                                           Timeout,
                                           &ThreadExitCode);

    if (Status == WAIT_TIMEOUT)
    {
        DPRINT1("Wait timed out!\n");
        return ExitProcess;
    }

    DPRINT("Getting exit codes\n");
    GetExitCodeProcess(ProcessData->Process->ProcessHandle, &ProcessExitCode);
    DPRINT("Exit codes: %lx %lx\n", ThreadExitCode, ProcessExitCode);

    if ((ThreadExitCode == EventType && ProcessExitCode == STILL_ACTIVE))
    {
        /* The notification thread exited but the process is sitll running */
        DPRINT("Process still alive: %d\n", ExitProcess);
        if (ExitProcess == TRUE)
        {
            DPRINT("Waiting on the process now...\n");
            Status = WaitForSingleObject(ProcessData->Process->ProcessHandle, Timeout);
            if (Status == 0)
            {
                DPRINT("All good\n");
                ExitProcess = FALSE;
            }
            else if (Status == WAIT_TIMEOUT)
            {
                DPRINT("Process still not dead!!\n");
            }
        }
    }
    else
    {
        ExitProcess = FALSE;
    }

    //
    // If we get here, we know that all wait conditions have
    // been satisfied.  Time to finish with the process.
    //

    return ExitProcess;
}

ULONG
NTAPI
NonConsoleProcessShutdown(IN PCSR_PROCESS Process,
                          IN PCONSOLE_PROCESS_DATA ProcessData,
                          IN ULONG Flags)
{
    BOOL KillIt;

    /* Do not kill system processes when a user is logging off */
    if ((Flags & EWX_SHUTDOWN) == EWX_LOGOFF &&
        (Process->ShutdownFlags & (CsrShutdownSystem | CsrShutdownOther)))
    {
        DPRINT("Do not kill a system process in a logoff request!\n");
        return CsrShutdownNonCsrProcess;
    }

    /* Do not kill Winlogon or CSRSS */
    if (Process->ClientId.UniqueProcess == NtCurrentProcess() ||
        Process->ClientId.UniqueProcess == UlongToHandle(LogonProcessId))
    {
        return CsrShutdownNonCsrProcess;
    }

    KillIt = NotifyGenericProcessForShutdown(Process, ProcessData, Flags);
    if (KillIt)
    {
        /* Terminate this process */
#if DBG
        WCHAR buffer[MAX_PATH];
        if (!GetProcessImageFileNameW(Process->ProcessHandle, buffer, MAX_PATH))
        {
            DPRINT1("Terminating process %x\n", Process->ClientId.UniqueProcess);
        }
        else
        {
            DPRINT1("Terminating process %x (%S)\n", Process->ClientId.UniqueProcess, buffer);
        }
#endif

        NtTerminateProcess(Process->ProcessHandle, 0);
        WaitForSingleObject(Process->ProcessHandle, ShutdownSettings.ProcessTerminateTimeout);
    }

    CsrDereferenceProcess(Process);
    return CsrShutdownCsrProcess;
}

// NOTE: See http://blogs.msdn.com/b/ntdebugging/archive/2007/06/09/how-windows-shuts-down.aspx
ULONG
NTAPI
ConsoleClientShutdown(IN PCSR_PROCESS CsrProcess,
                      IN ULONG Flags,
                      IN BOOLEAN FirstPhase)
{
    PCONSOLE_PROCESS_DATA ProcessData = ConsoleGetPerProcessData(CsrProcess);

    if ( ProcessData->ConsoleHandle != NULL ||
         ProcessData->HandleTable   != NULL )
    {
        NotifyConsoleProcessForShutdown(CsrProcess, ProcessData, Flags);

        /* We are done with the process itself */
        CsrDereferenceProcess(CsrProcess);
        return CsrShutdownCsrProcess;
    }
    else
    {
        DPRINT("ConsoleClientShutdown(0x%p, 0x%x, %s) - Non-console process [0x%x, 0x%x]\n",
                CsrProcess, Flags, FirstPhase ? "FirstPhase" : "LastPhase",
                CsrProcess->ClientId.UniqueProcess, CsrProcess->ClientId.UniqueThread);

        /* On first pass, let the gui server terminate all the processes that it owns */
        if (FirstPhase) return CsrShutdownNonCsrProcess;

        /* Use the generic handler since this isn't a gui process */
        return NonConsoleProcessShutdown(CsrProcess, ProcessData, Flags);
    }

    return CsrShutdownNonCsrProcess;
}

/* EOF */

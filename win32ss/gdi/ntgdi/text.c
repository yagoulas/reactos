/*
 * PROJECT:         ReactOS win32 kernel mode subsystem
 * LICENSE:         GPL - See COPYING in the top level directory
 * FILE:            win32ss/gdi/ntgdi/text.c
 * PURPOSE:         Text/Font
 * PROGRAMMER:
 */

/** Includes ******************************************************************/

#include <win32k.h>

#define NDEBUG
#include <debug.h>

/** Functions *****************************************************************/

BOOL FASTCALL
GreTextOutW(
    HDC  hdc,
    int  nXStart,
    int  nYStart,
    LPCWSTR  lpString,
    int  cchString)
{
    return GreExtTextOutW(hdc, nXStart, nYStart, 0, NULL, lpString, cchString, NULL, 0);
}

/*
   flOpts :
   GetTextExtentPoint32W = 0
   GetTextExtentPointW   = 1
 */
BOOL
FASTCALL
GreGetTextExtentW(
    HDC hDC,
    LPCWSTR lpwsz,
    INT cwc,
    LPSIZE psize,
    UINT flOpts)
{
    PDC pdc;
    PRFONT prfnt;
    BOOL Result;

    if (!cwc)
    {
        psize->cx = 0;
        psize->cy = 0;
        return TRUE;
    }

    pdc = DC_LockDc(hDC);
    if (!pdc)
    {
        EngSetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    prfnt = DC_prfnt(pdc);
    if ( prfnt )
    {
        Result = TextIntGetTextExtentPoint( pdc,
                                            prfnt,
                                            lpwsz,
                                            cwc,
                                            0,
                                            NULL,
                                            0,
                                            psize,
                                            flOpts);
    }
    else
        Result = FALSE;

    DC_UnlockDc(pdc);
    return Result;
}


/*
   fl :
   GetTextExtentExPointW = 0 and everything else that uses this.
   GetTextExtentExPointI = 1
 */
BOOL
FASTCALL
GreGetTextExtentExW(
    HDC hDC,
    LPCWSTR String,
    ULONG Count,
    ULONG MaxExtent,
    PULONG Fit,
    PULONG Dx,
    LPSIZE pSize,
    FLONG fl)
{
    PDC pdc;
    PRFONT prfnt;
    BOOL Result;

    if ( (!String && Count ) || !pSize )
    {
        EngSetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if ( !Count )
    {
        if ( Fit ) Fit = 0;
        return TRUE;
    }

    pdc = DC_LockDc(hDC);
    if (NULL == pdc)
    {
        EngSetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    prfnt = DC_prfnt(pdc);
    if ( prfnt )
    {
        Result = TextIntGetTextExtentPoint( pdc,
                                            prfnt,
                                            String,
                                            Count,
                                            MaxExtent,
                                            (LPINT)Fit,
                                            (LPINT)Dx,
                                            pSize,
                                            fl);
    }
    else
        Result = FALSE;

    DC_UnlockDc(pdc);
    return Result;
}

BOOL 
WINAPI
GreGetTextMetricsW(
    _In_  HDC hdc,
    _Out_ LPTEXTMETRICW lptm)
{
   TMW_INTERNAL tmwi;
   if (!ftGdiGetTextMetricsW(hdc, &tmwi)) return FALSE;
   *lptm = tmwi.TextMetric;
   return TRUE;
}

DWORD
APIENTRY
NtGdiGetCharSet(HDC hDC)
{
    PDC Dc;
    PDC_ATTR pdcattr;
    DWORD cscp;
    // If here, update everything!
    Dc = DC_LockDc(hDC);
    if (!Dc)
    {
        EngSetLastError(ERROR_INVALID_HANDLE);
        return 0;
    }
    cscp = ftGdiGetTextCharsetInfo(Dc, NULL, 0);
    pdcattr = Dc->pdcattr;
    pdcattr->iCS_CP = cscp;
    pdcattr->ulDirty_ &= ~DIRTY_CHARSET;
    DC_UnlockDc( Dc );
    return cscp;
}

BOOL
APIENTRY
NtGdiGetRasterizerCaps(
    OUT LPRASTERIZER_STATUS praststat,
    IN ULONG cjBytes)
{
    NTSTATUS Status = STATUS_SUCCESS;
    RASTERIZER_STATUS rsSafe;

    if (praststat && cjBytes)
    {
        if ( cjBytes >= sizeof(RASTERIZER_STATUS) ) cjBytes = sizeof(RASTERIZER_STATUS);
        if ( ftGdiGetRasterizerCaps(&rsSafe))
        {
            _SEH2_TRY
            {
                ProbeForWrite( praststat,
                sizeof(RASTERIZER_STATUS),
                1);
                RtlCopyMemory(praststat, &rsSafe, cjBytes );
            }
            _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
            {
                Status = _SEH2_GetExceptionCode();
            }
            _SEH2_END;

            if (!NT_SUCCESS(Status))
            {
                SetLastNtError(Status);
                return FALSE;
            }

            return TRUE;
        }
    }
    return FALSE;
}

INT
APIENTRY
NtGdiGetTextCharsetInfo(
    IN HDC hdc,
    OUT OPTIONAL LPFONTSIGNATURE lpSig,
    IN DWORD dwFlags)
{
    PDC Dc;
    INT Ret;
    FONTSIGNATURE fsSafe;
    PFONTSIGNATURE pfsSafe = &fsSafe;
    NTSTATUS Status = STATUS_SUCCESS;

    Dc = DC_LockDc(hdc);
    if (!Dc)
    {
        EngSetLastError(ERROR_INVALID_HANDLE);
        return DEFAULT_CHARSET;
    }

    if (!lpSig) pfsSafe = NULL;

    Ret = HIWORD(ftGdiGetTextCharsetInfo( Dc, pfsSafe, dwFlags));

    if (lpSig)
    {
        if (Ret == DEFAULT_CHARSET)
            RtlZeroMemory(pfsSafe, sizeof(FONTSIGNATURE));

        _SEH2_TRY
        {
            ProbeForWrite( lpSig,
            sizeof(FONTSIGNATURE),
            1);
            RtlCopyMemory(lpSig, pfsSafe, sizeof(FONTSIGNATURE));
        }
        _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
        {
            Status = _SEH2_GetExceptionCode();
        }
        _SEH2_END;

        if (!NT_SUCCESS(Status))
        {
            SetLastNtError(Status);
            return DEFAULT_CHARSET;
        }
    }
    DC_UnlockDc(Dc);
    return Ret;
}


/*
   fl :
   GetTextExtentExPointW = 0 and everything else that uses this.
   GetTextExtentExPointI = 1
 */
W32KAPI
BOOL
APIENTRY
NtGdiGetTextExtentExW(
    IN HDC hDC,
    IN OPTIONAL LPWSTR UnsafeString,
    IN ULONG Count,
    IN ULONG MaxExtent,
    OUT OPTIONAL PULONG UnsafeFit,
    OUT OPTIONAL PULONG UnsafeDx,
    OUT LPSIZE UnsafeSize,
    IN FLONG fl
)
{
    PDC dc;
    PRFONT prfnt;
    LPWSTR String;
    SIZE Size;
    NTSTATUS Status;
    BOOLEAN Result;
    INT Fit;
    LPINT Dx;

    if ((LONG)Count < 0)
    {
        EngSetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    /* FIXME: Handle fl */

    if (0 == Count)
    {
        Size.cx = 0;
        Size.cy = 0;
        Status = MmCopyToCaller(UnsafeSize, &Size, sizeof(SIZE));
        if (! NT_SUCCESS(Status))
        {
            SetLastNtError(Status);
            return FALSE;
        }
        return TRUE;
    }

    String = ExAllocatePoolWithTag(PagedPool, Count * sizeof(WCHAR), GDITAG_TEXT);
    if (NULL == String)
    {
        EngSetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    if (NULL != UnsafeDx)
    {
        Dx = ExAllocatePoolWithTag(PagedPool, Count * sizeof(INT), GDITAG_TEXT);
        if (NULL == Dx)
        {
            ExFreePoolWithTag(String, GDITAG_TEXT);
            EngSetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return FALSE;
        }
    }
    else
    {
        Dx = NULL;
    }

    Status = MmCopyFromCaller(String, UnsafeString, Count * sizeof(WCHAR));
    if (! NT_SUCCESS(Status))
    {
        if (NULL != Dx)
        {
            ExFreePoolWithTag(Dx, GDITAG_TEXT);
        }
        ExFreePoolWithTag(String, GDITAG_TEXT);
        SetLastNtError(Status);
        return FALSE;
    }

    dc = DC_LockDc(hDC);
    if (NULL == dc)
    {
        if (NULL != Dx)
        {
            ExFreePoolWithTag(Dx, GDITAG_TEXT);
        }
        ExFreePoolWithTag(String, GDITAG_TEXT);
        EngSetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    prfnt = DC_prfnt(dc);
    if ( prfnt )
    {
        Result = TextIntGetTextExtentPoint( dc,
                                            prfnt,
                                            String,
                                            Count,
                                            MaxExtent,
                                            NULL == UnsafeFit ? NULL : &Fit,
                                            Dx,
                                            &Size,
                                            fl);
    }
    else
        Result = FALSE;
    DC_UnlockDc(dc);

    ExFreePoolWithTag(String, GDITAG_TEXT);
    if (! Result)
    {
        if (NULL != Dx)
        {
            ExFreePoolWithTag(Dx, GDITAG_TEXT);
        }
        return FALSE;
    }

    if (NULL != UnsafeFit)
    {
        Status = MmCopyToCaller(UnsafeFit, &Fit, sizeof(INT));
        if (! NT_SUCCESS(Status))
        {
            if (NULL != Dx)
            {
                ExFreePoolWithTag(Dx, GDITAG_TEXT);
            }
            SetLastNtError(Status);
            return FALSE;
        }
    }

    if (NULL != UnsafeDx)
    {
        Status = MmCopyToCaller(UnsafeDx, Dx, Count * sizeof(INT));
        if (! NT_SUCCESS(Status))
        {
            if (NULL != Dx)
            {
                ExFreePoolWithTag(Dx, GDITAG_TEXT);
            }
            SetLastNtError(Status);
            return FALSE;
        }
    }
    if (NULL != Dx)
    {
        ExFreePoolWithTag(Dx, GDITAG_TEXT);
    }

    Status = MmCopyToCaller(UnsafeSize, &Size, sizeof(SIZE));
    if (! NT_SUCCESS(Status))
    {
        SetLastNtError(Status);
        return FALSE;
    }

    return TRUE;
}


/*
   flOpts :
   GetTextExtentPoint32W = 0
   GetTextExtentPointW   = 1
 */
BOOL
APIENTRY
NtGdiGetTextExtent(HDC hdc,
                   LPWSTR lpwsz,
                   INT cwc,
                   LPSIZE psize,
                   UINT flOpts)
{
    return NtGdiGetTextExtentExW(hdc, lpwsz, cwc, 0, NULL, NULL, psize, flOpts);
}

BOOL
APIENTRY
NtGdiSetTextJustification(HDC  hDC,
                          int  BreakExtra,
                          int  BreakCount)
{
    PDC pDc;
    PDC_ATTR pdcattr;

    pDc = DC_LockDc(hDC);
    if (!pDc)
    {
        EngSetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    pdcattr = pDc->pdcattr;

    pdcattr->lBreakExtra = BreakExtra;
    pdcattr->cBreak = BreakCount;

    DC_UnlockDc(pDc);
    return TRUE;
}


W32KAPI
INT
APIENTRY
NtGdiGetTextFaceW(
    IN HDC hDC,
    IN INT Count,
    OUT OPTIONAL LPWSTR FaceName,
    IN BOOL bAliasName
)
{
    PDC Dc;
    PRFONT prfnt;
    NTSTATUS Status;
    INT fLen, ret;

    /* FIXME: Handle bAliasName */

    Dc = DC_LockDc(hDC);
    if (Dc == NULL)
    {
        EngSetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    
    prfnt = DC_prfnt(Dc);
    if (prfnt == NULL)
    {
        EngSetLastError(ERROR_INVALID_HANDLE);
        DC_UnlockDc(Dc);
        return FALSE;
    }

    fLen = wcslen(prfnt->FaceName) + 1;

    if (FaceName != NULL)
    {
        Count = min(Count, fLen);
        Status = MmCopyToCaller(FaceName, prfnt->FaceName, Count * sizeof(WCHAR));
        if (!NT_SUCCESS(Status))
        {
            DC_UnlockDc(Dc);
            SetLastNtError(Status);
            return 0;
        }
        /* Terminate if we copied only part of the font name */
        if (Count > 0 && Count < fLen)
        {
            FaceName[Count - 1] = '\0';
        }
        ret = Count;
    }
    else
    {
        ret = fLen;
    }

    DC_UnlockDc(Dc);
    return ret;
}

W32KAPI
BOOL
APIENTRY
NtGdiGetTextMetricsW(
    IN HDC hDC,
    OUT TMW_INTERNAL * pUnsafeTmwi,
    IN ULONG cj)
{
    TMW_INTERNAL Tmwi;

    if ( cj <= sizeof(TMW_INTERNAL) )
    {
        if (ftGdiGetTextMetricsW(hDC, &Tmwi))
        {
            _SEH2_TRY
            {
                ProbeForWrite(pUnsafeTmwi, cj, 1);
                RtlCopyMemory(pUnsafeTmwi, &Tmwi, cj);
            }
            _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
            {
                SetLastNtError(_SEH2_GetExceptionCode());
                _SEH2_YIELD(return FALSE);
            }
            _SEH2_END

            return TRUE;
        }
    }
    return FALSE;
}

/* EOF */

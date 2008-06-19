/*
 * PROJECT:     ReactOS API Test GUI
 * LICENSE:     GPL - See COPYING in the top level directory
 * FILE:        
 * PURPOSE:     browse dialog implementation
 * COPYRIGHT:   Copyright 2008 Ged Murphy <gedmurphy@reactos.org>
 *
 */

#include <precomp.h>

#define DLL_SEARCH_DIR L"\\Debug\\testlibs\\*"
#define IL_MAIN 0
#define IL_TEST 1

typedef wchar_t *(__cdecl *DLLNAME)();

static INT
GetNumberOfDllsInFolder(LPWSTR lpFolder)
{
    HANDLE hFind;
    WIN32_FIND_DATAW findFileData;
    INT numFiles = 0;

    hFind = FindFirstFileW(lpFolder,
                           &findFileData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        DisplayError(GetLastError());
        return 0;
    }

    do
    {
        if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            numFiles++;
        }
    } while (FindNextFile(hFind, &findFileData) != 0);

    return numFiles;
}

static INT
GetListOfTestDlls(PMAIN_WND_INFO pInfo)
{
    HANDLE hFind;
    WIN32_FIND_DATAW findFileData;
    WCHAR szDllPath[MAX_PATH];
    LPWSTR ptr;
    INT numFiles = 0;
    INT len;

    len = GetCurrentDirectory(MAX_PATH, szDllPath);
    if (!len) return 0;

    wcsncat(szDllPath, DLL_SEARCH_DIR, MAX_PATH - (len + 1));

    numFiles = GetNumberOfDllsInFolder(szDllPath);
    if (!numFiles) return 0;

    pInfo->lpDllList = HeapAlloc(GetProcessHeap(), 0, numFiles * (MAX_PATH * sizeof(WCHAR)));
    if (!pInfo->lpDllList)
        return 0;

    hFind = FindFirstFileW(szDllPath,
                           &findFileData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        DisplayError(GetLastError());
        HeapFree(GetProcessHeap(), 0, pInfo->lpDllList);
        return 0;
    }

    /* remove the glob */
    ptr = wcschr(szDllPath, L'*');
    if (ptr)
        *ptr = L'\0';

    /* don't mod our base pointer */
    ptr = pInfo->lpDllList;

    do
    {
        if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            //MessageBoxW(NULL, findFileData.cFileName, NULL, 0);

            /* set the path */
            wcscpy(ptr, szDllPath);

            /* tag the file onto the path */
            len = MAX_PATH - (wcslen(ptr) + 1);
            wcsncat(ptr, findFileData.cFileName, len);

            /* move the pointer along by MAX_PATH */
            ptr += MAX_PATH;
        }
    } while (FindNextFile(hFind, &findFileData) != 0);

    return numFiles;
}

static HTREEITEM
InsertIntoTreeView(HWND hTreeView,
                   HTREEITEM hRoot,
                   LPWSTR lpLabel,
                   LPWSTR lpDllPath,
                   INT Image)
{
    TV_ITEM tvi;
    TV_INSERTSTRUCT tvins;

    ZeroMemory(&tvi, sizeof(tvi));
    ZeroMemory(&tvins, sizeof(tvins));

    tvi.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    tvi.pszText = lpLabel;
    tvi.cchTextMax = lstrlen(lpLabel);
    tvi.lParam = (LPARAM)lpDllPath;
    tvi.iImage = Image;
    tvi.iSelectedImage = Image;

    //tvi.stateMask = TVIS_OVERLAYMASK;

    tvins.item = tvi;
    tvins.hParent = hRoot;

    return TreeView_InsertItem(hTreeView, &tvins);
}

static VOID
PopulateTreeView(PMAIN_WND_INFO pInfo)
{
    HTREEITEM hRoot;
    HIMAGELIST hImgList;
    DLLNAME GetTestName;
    HMODULE hDll;
    LPWSTR lpDllPath;
    LPWSTR lpTestName;
    INT RootImage, i;

    pInfo->hBrowseTV = GetDlgItem(pInfo->hBrowseDlg, IDC_TREEVIEW);

    (void)TreeView_DeleteAllItems(pInfo->hBrowseTV);

    hImgList = InitImageList(IDI_ICON,
                             IDI_TESTS,
                             16,
                             16);
    if (!hImgList) return;

    (void)TreeView_SetImageList(pInfo->hBrowseTV,
                                hImgList,
                                TVSIL_NORMAL);

    /* insert the root item into the tree */
    hRoot = InsertIntoTreeView(pInfo->hBrowseTV,
                               NULL,
                               L"Full",
                               NULL,
                               IL_MAIN);

    for (i = 0; i < pInfo->numDlls; i++)
    {
        lpDllPath = pInfo->lpDllList + (MAX_PATH * i);

        hDll = LoadLibraryW(lpDllPath);
        if (hDll)
        {
            GetTestName = (DLLNAME)GetProcAddress(hDll, "GetTestName");
            if (GetTestName)
            {
                lpTestName = GetTestName();

                InsertIntoTreeView(pInfo->hBrowseTV,
                                   hRoot,
                                   lpTestName,
                                   lpDllPath,
                                   IL_TEST);

                // add individual tests as children

            }

            FreeLibrary(hDll);
        }
    }
}

static VOID
PopulateTestList(PMAIN_WND_INFO pInfo)
{
    pInfo->numDlls = GetListOfTestDlls(pInfo);
    if (pInfo->numDlls)
    {
        PopulateTreeView(pInfo);
    }
}


static BOOL
OnInitBrowseDialog(HWND hDlg,
                   LPARAM lParam)
{
    PMAIN_WND_INFO pInfo;

    pInfo = (PMAIN_WND_INFO)lParam;

    pInfo->hBrowseDlg = hDlg;

    SetWindowLongPtr(hDlg,
                     GWLP_USERDATA,
                     (LONG_PTR)pInfo);

    PopulateTestList(pInfo);

    return TRUE;
}


BOOL CALLBACK
BrowseDlgProc(HWND hDlg,
              UINT Message,
              WPARAM wParam,
              LPARAM lParam)
{
    PMAIN_WND_INFO pInfo;

    /* Get the window context */
    pInfo = (PMAIN_WND_INFO)GetWindowLongPtr(hDlg,
                                             GWLP_USERDATA);
    if (pInfo == NULL && Message != WM_INITDIALOG)
    {
        goto HandleDefaultMessage;
    }

    switch(Message)
    {
        case WM_INITDIALOG:
            return OnInitBrowseDialog(hDlg, lParam);

        case WM_COMMAND:
        {
            if ((LOWORD(wParam) == IDOK) || (LOWORD(wParam) == IDCANCEL))
            {
                HeapFree(GetProcessHeap(), 0, pInfo->lpDllList);
                pInfo->lpDllList = NULL;

                EndDialog(hDlg,
                          LOWORD(wParam));
                return TRUE;
            }

            break;
        }

HandleDefaultMessage:
        default:
            return FALSE;
    }

    return FALSE;
}
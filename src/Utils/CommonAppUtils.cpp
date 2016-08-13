﻿// TortoiseGit - a Windows shell extension for easy version control

// Copyright (C) 2008-2013, 2015-2016 - TortoiseGit
// Copyright (C) 2003-2008,2010 - TortoiseSVN

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
#include "stdafx.h"
#include "..\Resources\LoglistCommonResource.h"
#include "CommonAppUtils.h"
#include "PathUtils.h"
#include "StringUtils.h"
#include "FormatMessageWrapper.h"
#include "registry.h"
#include "SelectFileFilter.h"

extern CString sOrigCWD;
extern CString g_sGroupingUUID;

bool CCommonAppUtils::LaunchApplication(const CString& sCommandLine, UINT idErrMessageFormat, bool bWaitForStartup, CString *cwd, bool uac)
{
	CString theCWD = sOrigCWD;
	if (cwd)
		theCWD = *cwd;

	if (uac)
	{
		CString file, param;
		SHELLEXECUTEINFO shellinfo = { 0 };
		shellinfo.cbSize = sizeof(shellinfo);
		shellinfo.lpVerb = _T("runas");
		shellinfo.nShow = SW_SHOWNORMAL;
		shellinfo.fMask = SEE_MASK_NOCLOSEPROCESS;
		shellinfo.lpDirectory = theCWD;

		int pos = sCommandLine.Find('"');
		if (pos == 0)
		{
			pos = sCommandLine.Find('"', 2);
			if (pos > 1)
			{
				file = sCommandLine.Mid(1, pos - 1);
				param = sCommandLine.Mid(pos + 1);
			}
			else
			{
				if (idErrMessageFormat != 0)
				{
					CString temp;
					temp.Format(idErrMessageFormat, (LPCTSTR)CFormatMessageWrapper());
					MessageBox(nullptr, temp, _T("TortoiseGit"), MB_OK | MB_ICONINFORMATION);
				}
				return false;
			}
		}
		else
		{
			pos = sCommandLine.Find(' ', 1);
			if (pos > 0)
			{
				file = sCommandLine.Mid(0, pos);
				param = sCommandLine.Mid(pos + 1);
			}
			else
				file = sCommandLine;
		}

		shellinfo.lpFile = file;
		shellinfo.lpParameters = param;

		if (!ShellExecuteEx(&shellinfo))
		{
			if (idErrMessageFormat != 0)
			{
				CString temp;
				temp.Format(idErrMessageFormat, (LPCTSTR)CFormatMessageWrapper());
				MessageBox(nullptr, temp, _T("TortoiseGit"), MB_OK | MB_ICONINFORMATION);
			}
			return false;
		}

		if (bWaitForStartup)
			WaitForInputIdle(shellinfo.hProcess, 10000);

		CloseHandle(shellinfo.hProcess);

		return true;
	}

	STARTUPINFO startup = { 0 };
	PROCESS_INFORMATION process = { 0 };
	startup.cb = sizeof(startup);

	CString cleanCommandLine(sCommandLine);
	if (CreateProcess(nullptr, cleanCommandLine.GetBuffer(), nullptr, nullptr, FALSE, CREATE_UNICODE_ENVIRONMENT, nullptr, theCWD, &startup, &process) == 0)
	{
		if (idErrMessageFormat)
		{
			CString temp;
			temp.Format(idErrMessageFormat, (LPCTSTR)CFormatMessageWrapper());
			MessageBox(nullptr, temp, _T("TortoiseGit"), MB_OK | MB_ICONINFORMATION);
		}
		return false;
	}

	AllowSetForegroundWindow(process.dwProcessId);

	if (bWaitForStartup)
		WaitForInputIdle(process.hProcess, 10000);

	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);

	return true;
}

bool CCommonAppUtils::RunTortoiseGitProc(const CString& sCommandLine, bool uac, bool includeGroupingUUID)
{
	CString pathToExecutable = CPathUtils::GetAppDirectory() + _T("TortoiseGitProc.exe");
	CString sCmd;
	sCmd.Format(_T("\"%s\" %s"), (LPCTSTR)pathToExecutable, (LPCTSTR)sCommandLine);
	if (AfxGetMainWnd()->GetSafeHwnd() && (sCommandLine.Find(L"/hwnd:") < 0))
	{
		CString sCmdLine;
		sCmdLine.Format(L"%s /hwnd:%p", (LPCTSTR)sCommandLine, (void*)AfxGetMainWnd()->GetSafeHwnd());
		sCmd.Format(_T("\"%s\" %s"), (LPCTSTR)pathToExecutable, (LPCTSTR)sCmdLine);
	}
	if (!g_sGroupingUUID.IsEmpty() && includeGroupingUUID)
	{
		sCmd += L" /groupuuid:\"";
		sCmd += g_sGroupingUUID;
		sCmd += L"\"";
	}

	return LaunchApplication(sCmd, NULL, false, nullptr, uac);
}

bool CCommonAppUtils::IsAdminLogin()
{
	SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
	PSID administratorsGroup;
	// Initialize SID.
	if (!AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &administratorsGroup))
		return false;
	SCOPE_EXIT { FreeSid(administratorsGroup); };

	// Check whether the token is present in admin group.
	BOOL isInAdminGroup = FALSE;
	if (!CheckTokenMembership(nullptr, administratorsGroup, &isInAdminGroup))
		return false;

	return !!isInAdminGroup;
}

bool CCommonAppUtils::SetListCtrlBackgroundImage(HWND hListCtrl, UINT nID, int width /* = 128 */, int height /* = 128 */)
{
	if ((((DWORD)CRegStdDWORD(_T("Software\\TortoiseGit\\ShowListBackgroundImage"), TRUE)) == FALSE))
		return false;
	ListView_SetTextBkColor(hListCtrl, CLR_NONE);
	COLORREF bkColor = ListView_GetBkColor(hListCtrl);
	// create a bitmap from the icon
	HICON hIcon = (HICON)LoadImage(AfxGetResourceHandle(), MAKEINTRESOURCE(nID), IMAGE_ICON, width, height, LR_DEFAULTCOLOR);
	if (!hIcon)
		return false;
	SCOPE_EXIT { DestroyIcon(hIcon); };

	RECT rect = {0};
	rect.right = width;
	rect.bottom = height;

	HWND desktop = ::GetDesktopWindow();
	if (!desktop)
		return false;

	HDC screen_dev = ::GetDC(desktop);
	if (!screen_dev)
		return false;
	SCOPE_EXIT { ::ReleaseDC(desktop, screen_dev); };

	// Create a compatible DC
	HDC dst_hdc = ::CreateCompatibleDC(screen_dev);
	if (!dst_hdc)
		return false;
	SCOPE_EXIT { ::DeleteDC(dst_hdc); };

	// Create a new bitmap of icon size
	HBITMAP bmp = ::CreateCompatibleBitmap(screen_dev, rect.right, rect.bottom);
	if (!bmp)
		return false;

	// Select it into the compatible DC
	HBITMAP old_dst_bmp = (HBITMAP)::SelectObject(dst_hdc, bmp);
	// Fill the background of the compatible DC with the given color
	::SetBkColor(dst_hdc, bkColor);
	::ExtTextOut(dst_hdc, 0, 0, ETO_OPAQUE, &rect, nullptr, 0, nullptr);

	// Draw the icon into the compatible DC
	::DrawIconEx(dst_hdc, 0, 0, hIcon, rect.right, rect.bottom, 0, nullptr, DI_NORMAL);
	::SelectObject(dst_hdc, old_dst_bmp);

	LVBKIMAGE lv;
	lv.ulFlags = LVBKIF_TYPE_WATERMARK;
	lv.hbm = bmp;
	lv.xOffsetPercent = 100;
	lv.yOffsetPercent = 100;
	ListView_SetBkImage(hListCtrl, &lv);
	return true;
}

bool CCommonAppUtils::FileOpenSave(CString& path, int * filterindex, UINT title, UINT filter, bool bOpen, HWND hwndOwner, LPCTSTR defaultExt)
{
	OPENFILENAME ofn = {0};				// common dialog box structure
	TCHAR szFile[MAX_PATH] = {0};		// buffer for file name. Explorer can't handle paths longer than MAX_PATH.
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwndOwner;
	if (path.GetLength() >= MAX_PATH)
	{
		CString dir = path;
		while (true)
		{
			int index = dir.ReverseFind(_T('\\'));
			if (index < 0)
				break;
			dir = dir.Left(index);
			if (PathFileExists(dir))
				break;
		}
		GetShortPathName(dir, szFile, MAX_PATH);
		CString remain = path.Right(path.GetLength() - dir.GetLength());
		_tcscat_s(szFile, MAX_PATH, remain);
	}
	else
		_tcscpy_s(szFile, MAX_PATH, (LPCTSTR)path);
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = _countof(szFile);

	CSelectFileFilter fileFilter;
	if (filter)
	{
		fileFilter.Load(filter);
		ofn.lpstrFilter = fileFilter;
	}
	ofn.nFilterIndex = 1;

	ofn.lpstrFileTitle = nullptr;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = nullptr;
	ofn.lpstrDefExt = defaultExt;
	CString temp;
	if (title)
	{
		temp.LoadString(title);
		CStringUtils::RemoveAccelerators(temp);
	}
	ofn.lpstrTitle = temp;
	if (bOpen)
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER;
	else
		ofn.Flags = OFN_OVERWRITEPROMPT | OFN_EXPLORER;

	// Display the Open dialog box.
	bool bRet = false;
	if (bOpen)
		bRet = !!GetOpenFileName(&ofn);
	else
		bRet = !!GetSaveFileName(&ofn);
	SetCurrentDirectory(sOrigCWD);
	if (bRet)
	{
		path = CString(ofn.lpstrFile);
		if (filterindex)
			*filterindex = ofn.nFilterIndex;
		return true;
	}
	return false;
}

void CCommonAppUtils::SetCharFormat(CWnd* window, DWORD mask , DWORD effects, const std::vector<CHARRANGE>& positions)
{
	CHARFORMAT2 format = {};
	format.cbSize = sizeof(CHARFORMAT2);
	format.dwMask = mask;
	format.dwEffects = effects;
	format.crTextColor = effects;

	for (const auto& range : positions)
	{
		window->SendMessage(EM_EXSETSEL, NULL, (LPARAM)&range);
		window->SendMessage(EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
	}
}

void CCommonAppUtils::SetCharFormat(CWnd* window, DWORD mask, DWORD effects )
{
	CHARFORMAT2 format = {};
	format.cbSize = sizeof(CHARFORMAT2);
	format.dwMask = mask;
	format.dwEffects = effects;
	window->SendMessage(EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
}

// This file is part of ElasticTabstops.
// 
// Copyright (C)2016 Justin Dailey <dail8859@yahoo.com>
// 
// ElasticTabstops is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include <WindowsX.h>
#include "PluginDefinition.h"
#include "resource.h"
#include "Hyperlinks.h"
#include "Version.h"

INT_PTR CALLBACK abtDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch(uMsg) {
		case WM_INITDIALOG:
			ConvertStaticToHyperlink(hwndDlg, IDC_GITHUB);
			ConvertStaticToHyperlink(hwndDlg, IDC_README);
			Edit_SetText(GetDlgItem(hwndDlg, IDC_VERSION), TEXT("Elastic Tabstops v") VERSION_TEXT TEXT(" ") VERSION_STAGE);
			return true;
		case WM_COMMAND:
			switch(LOWORD(wParam)) {
				case IDOK:
					DestroyWindow(hwndDlg);
					return true;
				case IDC_GITHUB:
					ShellExecute(hwndDlg, TEXT("open"), TEXT("https://github.com/dail8859/ElasticTabstops/"), NULL, NULL, SW_SHOWNORMAL);
					return true;
			}
		case WM_DESTROY:
			DestroyWindow(hwndDlg);
			return true;
		}
	return false;
}

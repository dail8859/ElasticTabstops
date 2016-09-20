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

#include "PluginDefinition.h"
#include "Version.h"
#include "ElasticTabstops.h"
#include "AboutDialog.h"
#include "resource.h"
#include "Config.h"

static HANDLE _hModule;
static NppData nppData;
static Configuration config = { true, nullptr, 1};

// Helper functions
static HWND getCurrentScintilla();
static bool shouldProcessCurrentFile();

// Menu callbacks
static void toggleEnabled();
static void convertEtToSpaces();
static void editSettings();
static void showAbout();

FuncItem funcItem[] = {
	{ TEXT("Enable"), toggleEnabled, 0, config.enabled, nullptr },
	{ TEXT("Convert Tabstops to Spaces"), convertEtToSpaces, 0, false, nullptr },
	{ TEXT(""), nullptr, 0, false, nullptr }, // separator
	{ TEXT("Settings..."), editSettings, 0, false, nullptr },
	{ TEXT("About..."), showAbout, 0, false, nullptr }
};

static HWND getCurrentScintilla() {
	int id;
	SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&id);
	if (id == 0) return nppData._scintillaMainHandle;
	else return nppData._scintillaSecondHandle;
}

static bool shouldProcessCurrentFile() {
	// See if the file is even using tabs first of all
	if (SendMessage(getCurrentScintilla(), SCI_GETUSETABS, 0, 0) == 0) return false;

	// Check the file extension
	if (config.file_extensions != nullptr) {
		wchar_t ext[MAX_PATH] = { 0 };
		SendMessage(nppData._nppHandle, NPPM_GETEXTPART, MAX_PATH, (LPARAM)ext);

		// Make sure it has an extension
		if (ext[0] != L'\0') {
			// Search the file extensions, make sure the char after the located string is blank
			// This makes sure that searching for .c doesn't find .cpp
			wchar_t *ptr = wcsstr(config.file_extensions, ext);
			if (ptr) {
				wchar_t next = ptr[wcslen(ext)];
				return next == 0 || next == L' ';
			}
		}
	}
	else {
		return true;
	}

	return false;
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD  reasonForCall, LPVOID lpReserved) {
	switch (reasonForCall) {
		case DLL_PROCESS_ATTACH:
			_hModule = hModule;
			break;
		case DLL_PROCESS_DETACH:
			break;
		case DLL_THREAD_ATTACH:
			break;
		case DLL_THREAD_DETACH:
			break;
	}
	return TRUE;
}

extern "C" __declspec(dllexport) void setInfo(NppData notepadPlusData) {
	nppData = notepadPlusData;
}

extern "C" __declspec(dllexport) const wchar_t * getName() {
	return NPP_PLUGIN_NAME;
}

extern "C" __declspec(dllexport) FuncItem * getFuncsArray(int *nbF) {
	*nbF = sizeof(funcItem) / sizeof(funcItem[0]);
	return funcItem;
}

extern "C" __declspec(dllexport) void beNotified(const SCNotification *notify) {
	static bool isFileEnabled = true;

	// Somehow we are getting notifications from other scintilla handles at times
	if (notify->nmhdr.hwndFrom != nppData._nppHandle &&
		notify->nmhdr.hwndFrom != nppData._scintillaMainHandle &&
		notify->nmhdr.hwndFrom != nppData._scintillaSecondHandle)
		return;

	switch (notify->nmhdr.code) {
		case SCN_MODIFIED: {
			if (!config.enabled || !isFileEnabled) break;

			bool isInsert = (notify->modificationType & SC_MOD_INSERTTEXT) != 0;
			bool isDelete = (notify->modificationType & SC_MOD_DELETETEXT) != 0;

			// Make sure we only look at inserts and deletes
			if (!isInsert && !isDelete) break;

			bool isUserAction = (notify->modificationType & SC_PERFORMED_USER) != 0;
			bool isUndoRedo = (notify->modificationType & (SC_PERFORMED_REDO | SC_PERFORMED_UNDO)) != 0;
			bool isLastStep = (notify->modificationType & SC_LASTSTEPINUNDOREDO) != 0;

			// Undo/Redo can come in multiple steps. Only update tabstops on the last step.
			// This can help to reduce lag spikes on complex undo/redo actions
			if (isUserAction || (isUndoRedo && isLastStep)) {
				int start = notify->position;
				int end = (isInsert ? notify->position + notify->length : notify->position);
				ElasticTabstops_OnModify(start, end, notify->linesAdded, notify->text);
			}

			break;
		}
		case SCN_ZOOM: {
			if (!config.enabled || !isFileEnabled) break;

			// Redo the entire document since the tab sizes have changed
			ElasticTabstops_SwitchToScintilla(getCurrentScintilla(), &config);
			ElasticTabstops_ComputeEntireDoc();
			break;
		}
		case NPPN_READY:
			ConfigLoad(&nppData, &config);
			CheckMenuItem(GetMenu(nppData._nppHandle), funcItem[0]._cmdID, config.enabled ? MF_CHECKED : MF_UNCHECKED);
			ElasticTabstops_OnReady(nppData._scintillaMainHandle);
			ElasticTabstops_OnReady(nppData._scintillaSecondHandle);
			ElasticTabstops_SwitchToScintilla(getCurrentScintilla(), &config);
			ElasticTabstops_ComputeEntireDoc();
			break;
		case NPPN_SHUTDOWN:
			ConfigSave(&nppData, &config);
			break;
		case NPPN_BUFFERACTIVATED:
			if (!config.enabled) break;

			isFileEnabled = shouldProcessCurrentFile();

			if (isFileEnabled) {
				ElasticTabstops_SwitchToScintilla(getCurrentScintilla(), &config);
				ElasticTabstops_ComputeEntireDoc();
			}

			break;
		case NPPN_FILESAVED: {
			wchar_t fname[MAX_PATH] = { 0 };
			SendMessage(nppData._nppHandle, NPPM_GETFULLPATHFROMBUFFERID, notify->nmhdr.idFrom, (LPARAM)fname);
			if (wcscmp(fname, GetIniFilePath(&nppData)) == 0) {
				ConfigLoad(&nppData, &config);
				CheckMenuItem(GetMenu(nppData._nppHandle), funcItem[0]._cmdID, config.enabled ? MF_CHECKED : MF_UNCHECKED);

				// Immediately apply the new config to the config file itself
				ElasticTabstops_SwitchToScintilla(getCurrentScintilla(), &config);
				ElasticTabstops_ComputeEntireDoc();
			}
			break;
		}
	}
	return;
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT Message, WPARAM wParam, LPARAM lParam) {
	return TRUE;
}

extern "C" __declspec(dllexport) BOOL isUnicode() {
	return TRUE;
}

static void toggleEnabled() {
	config.enabled = !config.enabled;
	SendMessage(nppData._nppHandle, NPPM_SETMENUITEMCHECK, funcItem[0]._cmdID, config.enabled);

	if (config.enabled && shouldProcessCurrentFile()) {
		// Run it on the entire file
		ElasticTabstops_ComputeEntireDoc();
	}
	else {
		// Clear all tabstops on the file
		HWND sci = getCurrentScintilla();
		int lineCount = SendMessage(sci, SCI_GETLINECOUNT, 0, 0);
		for (int i = 0; i < lineCount; ++i) {
			SendMessage(sci, SCI_CLEARTABSTOPS, i, 0);
		}
	}
}

static void convertEtToSpaces() {
	if (!config.enabled || !shouldProcessCurrentFile()) return;

	// Temporarily disable elastic tabstops because replacing tabs with spaces causes
	// Scintilla to send notifications of all the changes.
	config.enabled = false;
	ElasticTabstops_ConvertToSpaces();
	config.enabled = true;
}

static void editSettings() {
	ConfigSave(&nppData, &config);
	SendMessage(nppData._nppHandle, NPPM_DOOPEN, 0, (LPARAM)GetIniFilePath(&nppData));
}

static void showAbout() {
	HWND hSelf = CreateDialogParam((HINSTANCE)_hModule, MAKEINTRESOURCE(IDD_ABOUTDLG), nppData._nppHandle, abtDlgProc, NULL);

	// Go to center
	RECT rc;
	::GetClientRect(nppData._nppHandle, &rc);
	POINT center;
	int w = rc.right - rc.left;
	int h = rc.bottom - rc.top;
	center.x = rc.left + w / 2;
	center.y = rc.top + h / 2;
	::ClientToScreen(nppData._nppHandle, &center);

	RECT dlgRect;
	::GetClientRect(hSelf, &dlgRect);
	int x = center.x - (dlgRect.right - dlgRect.left) / 2;
	int y = center.y - (dlgRect.bottom - dlgRect.top) / 2;

	::SetWindowPos(hSelf, HWND_TOP, x, y, (dlgRect.right - dlgRect.left), (dlgRect.bottom - dlgRect.top), SWP_SHOWWINDOW);
}

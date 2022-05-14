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

#include <vector>
#include <codecvt>

#include "PluginDefinition.h"
#include "Version.h"
#include "ElasticTabstops.h"
#include "AboutDialog.h"
#include "resource.h"
#include "Config.h"

static HANDLE _hModule;
static NppData nppData;
static Configuration config = { true, {"*"}, 1, false };

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
	{ TEXT(""), nullptr, 0, false, nullptr }, // separator
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

std::string ws2s(const std::wstring& wstr)
{
	using convert_typeX = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_typeX, wchar_t> converterX;

	return converterX.to_bytes(wstr);
}

static bool shouldProcessCurrentFile() {
	// Check the file extension
	wchar_t buffer[MAX_PATH] = { 0 };
	SendMessage(nppData._nppHandle, NPPM_GETEXTPART, MAX_PATH, (LPARAM)buffer);
	std::wstring wext(buffer);
	std::string ext = ws2s(wext);

	for (const auto &extension: config.file_extensions) {
		if (extension == "*") return true;
		if (extension == "!*") return false;

		bool not = extension[0] == '!';
		if (extension.compare(not ? 1 : 0, std::string::npos, ext) == 0) {
			return !not;
		}
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
	ConfigLoad(&nppData, &config);
}

extern "C" __declspec(dllexport) const wchar_t * getName() {
	return NPP_PLUGIN_NAME;
}

extern "C" __declspec(dllexport) FuncItem * getFuncsArray(int *nbF) {
	*nbF = sizeof(funcItem) / sizeof(funcItem[0]);
	return funcItem;
}

extern "C" __declspec(dllexport) void beNotified(SCNotification *notify) {
	static bool isFileEnabled = true;
	static int numEdits = 0;
	static struct {
		int start;
		int end;
		int linesAdded;
		bool hasTab;
	} edit;

	// Somehow we are getting notifications from other scintilla handles at times
	if (notify->nmhdr.hwndFrom != nppData._nppHandle &&
		notify->nmhdr.hwndFrom != nppData._scintillaMainHandle &&
		notify->nmhdr.hwndFrom != nppData._scintillaSecondHandle)
		return;

	switch (notify->nmhdr.code) {
		case SCN_UPDATEUI:
			if (!config.enabled || !isFileEnabled) break;

			if (notify->updated & SC_UPDATE_V_SCROLL || numEdits > 0) {
				// A single "edit" can be optimized to potentially update a smaller area
				// More than 1 is easiest to just update the current view
				if (numEdits == 1) {
					ElasticTabstopsOnModify(edit.start, edit.end, edit.linesAdded, edit.hasTab);
				}
				else {
					ElasticTabstopsComputeCurrentView();
				}

				numEdits = 0;
			}

			break;
		case SCN_MODIFIED: {
			if (!config.enabled || !isFileEnabled) break;

			bool isInsert = (notify->modificationType & SC_MOD_INSERTTEXT) != 0;
			bool isDelete = (notify->modificationType & SC_MOD_DELETETEXT) != 0;

			// Make sure we only look at inserts and deletes
			if (isInsert || isDelete) {
				numEdits++;
				if (numEdits == 1) {
					edit.start = static_cast<int>(notify->position);
					edit.end = static_cast<int>((isInsert ? notify->position + notify->length : notify->position));
					edit.linesAdded = static_cast<int>(notify->linesAdded);
					edit.hasTab = notify->text && strchr(notify->text, '\t') != NULL;
				}
			}

			break;
		}
		case SCN_ZOOM: {
			if (!config.enabled || !isFileEnabled) break;

			// Redo the current view since the tab sizes have changed
			ElasticTabstopsSwitchToScintilla(getCurrentScintilla(), &config);
			ElasticTabstopsComputeCurrentView();

			break;
		}
		case NPPN_READY:
			CheckMenuItem(GetMenu(nppData._nppHandle), funcItem[0]._cmdID, config.enabled ? MF_CHECKED : MF_UNCHECKED);
			ElasticTabstopsOnReady(nppData._scintillaMainHandle);
			ElasticTabstopsOnReady(nppData._scintillaSecondHandle);
			ElasticTabstopsSwitchToScintilla(getCurrentScintilla(), &config);
			if (config.enabled) ElasticTabstopsComputeCurrentView();
			break;
		case NPPN_SHUTDOWN:
			ConfigSave(&nppData, &config);
			break;
		case NPPN_BUFFERACTIVATED:
			if (!config.enabled) break;

			isFileEnabled = shouldProcessCurrentFile();

			if (isFileEnabled) {
				ElasticTabstopsSwitchToScintilla(getCurrentScintilla(), &config);
				ElasticTabstopsComputeCurrentView();
				numEdits = 0;
			}

			break;
		case NPPN_FILESAVED: {
			wchar_t fname[MAX_PATH] = { 0 };
			SendMessage(nppData._nppHandle, NPPM_GETFULLPATHFROMBUFFERID, notify->nmhdr.idFrom, (LPARAM)fname);
			if (wcscmp(fname, GetIniFilePath(&nppData)) == 0) {
				ConfigLoad(&nppData, &config);
				CheckMenuItem(GetMenu(nppData._nppHandle), funcItem[0]._cmdID, config.enabled ? MF_CHECKED : MF_UNCHECKED);

				// Immediately apply the new config to the config file itself
				ElasticTabstopsSwitchToScintilla(getCurrentScintilla(), &config);
				ElasticTabstopsComputeCurrentView();
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
		// Run it on the current file
		ElasticTabstopsComputeCurrentView();
	}
	else {
		// Clear all tabstops on the file
		HWND sci = getCurrentScintilla();
		auto lineCount = SendMessage(sci, SCI_GETLINECOUNT, 0, 0);
		for (auto i = 0; i < lineCount; ++i) {
			SendMessage(sci, SCI_CLEARTABSTOPS, i, 0);
		}
	}
}

static void convertEtToSpaces() {
	if (!config.enabled || !shouldProcessCurrentFile()) return;

	// Temporarily disable elastic tabstops because replacing tabs with spaces causes
	// Scintilla to send notifications of all the changes.
	config.enabled = false;
	ElasticTabstopsConvertToSpaces(&config);
	config.enabled = true;
}

static void editSettings() {
	ConfigSave(&nppData, &config);
	SendMessage(nppData._nppHandle, NPPM_DOOPEN, 0, (LPARAM)GetIniFilePath(&nppData));
}

static void showAbout() {
	ShowAboutDialog((HINSTANCE)_hModule, MAKEINTRESOURCE(IDD_ABOUTDLG), nppData._nppHandle);
}

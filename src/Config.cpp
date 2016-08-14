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

#include <stdio.h>
#include "Config.h"

const wchar_t *GetIniFilePath(const NppData *nppData) {
	static wchar_t iniPath[MAX_PATH];
	SendMessage(nppData->_nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, (LPARAM)iniPath);
	wcscat_s(iniPath, MAX_PATH, L"\\ElasticTabstops.ini");
	return iniPath;
}

void ConfigLoad(const NppData *nppData, Configuration *config) {
	const wchar_t *iniPath = GetIniFilePath(nppData);

	FILE *file = _wfopen(iniPath, L"r");

	if (file == nullptr) return;

	char line[256];
	while (true) {
		if (fgets(line, 256, file) == NULL) break;

		// Ignore comments and blank lines
		if (line[0] == ';' || line[0] == '\r' || line[0] == '\n') continue;

		// TODO: if this next section gets too bloated/complicated come up with a better way

		if (strncmp(line, "enabled ", 8) == 0) {
			char *c = &line[8];
			while (isspace(*c)) c++;
			config->enabled = strncmp(c, "true", 4) == 0;
		}
		else if (strncmp(line, "extensions ", 11) == 0) {
			if (config->file_extensions != nullptr) {
				free(config->file_extensions);
				config->file_extensions = nullptr;
			}

			// Strip the newline
			line[strcspn(line, "\r\n")] = 0;

			char *c = &line[11];
			while (isspace(*c)) c++;

			if (*c == '\0' || *c == '*') continue;

			wchar_t ws[256];
			mbstowcs(ws, c, 256);
			config->file_extensions = wcsdup(ws);
		}
		else if (strncmp(line, "padding ", 8) == 0) {
			char *c = &line[8];
			while (isspace(*c)) c++;

			config->min_padding = strtol(c, nullptr, 10);

			// The above could fail or the user types something crazy
			if (config->min_padding > 256) config->min_padding = 256;
			if (config->min_padding == 0) config->min_padding = 1;
		}
	}

	fclose(file);
}

void ConfigSave(const NppData *nppData, const Configuration *config) {
	char s[256];
	const wchar_t *iniPath = GetIniFilePath(nppData);

	FILE *file = _wfopen(iniPath, L"w");
	if (file == nullptr) return;

	fputs("; Configuration for ElasticTabstops.\n; Saving this file will immediately apply the settings.\n\n", file);

	// Whether or not it is enabled
	fputs("; Whether elastic tabstops are enabled or not: true or false\n", file);
	fprintf(file, "enabled %s\n\n", config->enabled == true ? "true" : "false");

	// The file extensions to appy it to
	fputs("; File extentions to apply elastic tabstops. For example...\n", file);
	fputs(";   \"extentions *\" will apply it to all files\n", file);
	fputs(";   \"extentions .c .h .cpp .hpp\" will apply it to C/C++ files\n", file);
	if (config->file_extensions != nullptr) wcstombs(s, config->file_extensions, 256);
	else strcpy(s, "*");
	fprintf(file, "extensions %s\n\n", s);

	// Minimum padding
	fputs("; Minimum padding in characters. Must be > 0\n", file);
	fprintf(file, "padding %d\n", config->min_padding);

	fclose(file);
}

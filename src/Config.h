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

#pragma once

#include "PluginDefinition.h"

#include <vector>
#include <string>

typedef struct Configuration{
	bool enabled;
	std::vector<std::string> file_extensions;
	size_t min_padding;
	bool convert_leading_tabs_to_spaces;
}Configuration;

const wchar_t *GetIniFilePath(const NppData *nppData);
void ConfigLoad(const NppData *nppData, Configuration *config);
void ConfigSave(const NppData *nppData, const Configuration *config);

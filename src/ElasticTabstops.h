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

#include "PluginInterface.h"
#include "Config.h"

void ElasticTabstopsSwitchToScintilla(HWND sci, const Configuration *config);
void ElasticTabstopsComputeCurrentView();
void ElasticTabstopsOnModify(int start, int end, int linesAdded, bool hasTab);
void ElasticTabstopsConvertToSpaces(const Configuration *config);
void ElasticTabstopsOnReady(HWND sci);

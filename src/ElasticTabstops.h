#pragma once

#include "PluginInterface.h"
#include "Config.h"

void ElasticTabstops_SwitchToScintilla(HWND sci, const Configuration *config);
void ElasticTabstops_ComputeEntireDoc();
void ElasticTabstops_OnModify(int start, int end, int linesAdded, const char *text);
void ElasticTabstops_OnReady(HWND sci);

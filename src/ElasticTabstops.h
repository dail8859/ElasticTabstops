#pragma once

#include "PluginInterface.h"
#include "Config.h"

void ElasticTabstops_ComputeEntireDoc(HWND sci, const Configuration *config);
void ElasticTabstops_OnModify(HWND sci, const Configuration *config, int start, int end, const char *text);
void ElasticTabstops_OnReady(HWND sci);

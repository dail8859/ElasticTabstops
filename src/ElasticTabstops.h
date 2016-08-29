#pragma once

#include "PluginInterface.h"
#include "Config.h"

void ElasticTabstops_OnModify(HWND sci, const Configuration *config, int start, int end);
void ElasticTabstops_OnReady(HWND sci);

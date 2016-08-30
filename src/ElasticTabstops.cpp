// This file is part of ElasticTabstops.
// 
// Original work Copyright (C)2007-2014 Nick Gravgaard, David Kinder
// Derived work Copyright (C)2016 Justin Dailey <dail8859@yahoo.com>
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
#include <string>
#include "ElasticTabstops.h"

#define MARK_UNDERLINE 20
#define SC_MARGIN_SYBOL 1
#define DBG_INDICATORS  8

static sptr_t edit;
static SciFnDirect func;
static int tab_width_minimum;
static int tab_width_padding;

static LONG_PTR inline call_edit(UINT msg, DWORD wp = 0, LONG_PTR lp = 0)
{
	return func(edit, msg, wp, lp);
}

static int get_line_start(int pos)
{
	int line = call_edit(SCI_LINEFROMPOSITION, pos);
	return call_edit(SCI_POSITIONFROMLINE, line);
}

static int get_line_end(int pos)
{
	int line = call_edit(SCI_LINEFROMPOSITION, pos);
	return call_edit(SCI_GETLINEENDPOSITION, line);
}

struct et_tabstop
{
	int text_width_pix;
	int *widest_width_pix;
};

enum direction
{
	BACKWARDS,
	FORWARDS
};

static int get_text_width(int start, int end)
{
	std::string s(end - start + 1, 0);

	TextRange range;
	range.chrg.cpMin = start;
	range.chrg.cpMax = end;
	range.lpstrText = &s[0];
	call_edit(SCI_GETTEXTRANGE, 0, (sptr_t)&range);

	LONG_PTR style = call_edit(SCI_GETSTYLEAT, start);

	// NOTE: the width is measured in case proportional fonts are used. 
	// If we assume monospaced fonts we could simplify measuring text widths eg (end-start)*char_width
	// But for now performance shouldn't be too much of an issue
	return call_edit(SCI_TEXTWIDTH, style, (LONG_PTR)range.lpstrText);
}

static int calc_tab_width(int text_width_in_tab)
{
	if (text_width_in_tab < tab_width_minimum)
	{
		text_width_in_tab = tab_width_minimum;
	}
	return text_width_in_tab + tab_width_padding;
}

static bool change_line(int& location, direction which_dir)
{
	int line = call_edit(SCI_LINEFROMPOSITION, location);
	if (which_dir == FORWARDS)
	{
		location = call_edit(SCI_POSITIONFROMLINE, line + 1);
	}
	else
	{
		if (line <= 0)
		{
			return false;
		}
		location = call_edit(SCI_POSITIONFROMLINE, line - 1);
	}
	return (location >= 0);
}

static int get_block_boundary(int location, direction which_dir)
{
	bool orig_line = true;

	location = get_line_start(location);
	do
	{
		int current_pos = location;
		unsigned char current_char = (unsigned char)call_edit(SCI_GETCHARAT, current_pos);
		int line_end = get_line_end(current_pos);
		int tabs_on_line = 0;

		while (current_char != '\0' && current_pos != line_end)
		{
			if (current_char == '\t')
				tabs_on_line++;

			current_pos = call_edit(SCI_POSITIONAFTER, current_pos);
			current_char = (unsigned char)call_edit(SCI_GETCHARAT, current_pos);
		}

		if (tabs_on_line == 0 && !orig_line) return location;

		orig_line = false;
	} while (change_line(location, which_dir));

	return location;
}

static int get_nof_tabs_between(int start, int end)
{
	int current_pos = start;
	int max_tabs = 0;

	do
	{
		unsigned char current_char = (unsigned char)call_edit(SCI_GETCHARAT, current_pos);
		int line_end = get_line_end(current_pos);
		int tabs_on_line = 0;

		while (current_char != '\0' && current_pos != line_end && current_pos < end)
		{
			if (current_char == '\t')
				tabs_on_line++;

			current_pos = call_edit(SCI_POSITIONAFTER, current_pos);
			current_char = (unsigned char)call_edit(SCI_GETCHARAT, current_pos);
		}

		if (tabs_on_line > max_tabs) max_tabs = tabs_on_line;

	} while (change_line(current_pos, FORWARDS) && current_pos < end);

	return max_tabs;
}

static void stretch_tabstops(int block_start_linenum, int block_min_end)
{
	std::vector<std::vector<et_tabstop>> grid;
	size_t max_tabs = 0;
	int block_nof_lines = 0;
	int current_line_num = 0;

	// Get width of text in cells until we reach the end of the block
	do {
		current_line_num = block_start_linenum + block_nof_lines;
		int current_pos = call_edit(SCI_POSITIONFROMLINE, current_line_num);
		int cell_start = current_pos;
		bool cell_empty = true;

		unsigned char current_char = (unsigned char)call_edit(SCI_GETCHARAT, current_pos);
		int line_end = get_line_end(current_pos);

		grid.emplace_back();
		while (current_char != '\0') {
			if (current_pos == line_end) {
				max_tabs = __max(max_tabs, grid.back().size());
				break;
			}
			else if (current_char == '\t') {
#ifdef _DEBUG
				// Highlight the cell
				call_edit(SCI_SETINDICATORCURRENT, grid.back().size() % DBG_INDICATORS);
				if (cell_empty) call_edit(SCI_INDICATORFILLRANGE, current_pos, 1);
				else call_edit(SCI_INDICATORFILLRANGE, cell_start, current_pos - cell_start + 1);
#endif

				int text_width_in_tab = 0;
				if (!cell_empty) {
					text_width_in_tab = get_text_width(cell_start, current_pos);
				}
				grid.back().push_back({ calc_tab_width(text_width_in_tab), nullptr });
				cell_empty = true;
			}
			else
			{
				if (cell_empty)
				{
					cell_start = current_pos;
					cell_empty = false;
				}
			}
			current_pos = call_edit(SCI_POSITIONAFTER, current_pos);
			current_char = (unsigned char)call_edit(SCI_GETCHARAT, current_pos);
		}

		block_nof_lines++;
		// We've hit the end of the block
	} while (grid.back().size() != 0 || current_line_num <= block_min_end);

#ifdef _DEBUG
	// Mark the start and end of the block being recomputed
	call_edit(SCI_MARKERADD, block_start_linenum - 1, MARK_UNDERLINE);
	call_edit(SCI_MARKERADD, __min(block_start_linenum + block_nof_lines - 1, call_edit(SCI_GETLINECOUNT) - 1), MARK_UNDERLINE);
#endif

	// find columns blocks and stretch to fit the widest cell
	for (size_t t = 0; t < max_tabs; t++) // for each column
	{
		bool starting_new_block = true;
		int first_line_in_block = 0;
		int max_width = 0;
		for (int l = 0; l < block_nof_lines; l++) // for each line
		{
			if (starting_new_block)
			{
				starting_new_block = false;
				first_line_in_block = l;
				max_width = 0;
			}
			if (t < grid[l].size())
			{
				grid[l][t].widest_width_pix = &(grid[first_line_in_block][t].text_width_pix); // point widestWidthPix at first 
				if (grid[l][t].text_width_pix > max_width)
				{
					max_width = grid[l][t].text_width_pix;
					grid[first_line_in_block][t].text_width_pix = max_width;
				}
			}
			else // end column block
			{
				starting_new_block = true;
			}
		}
	}

	// set tabstops
	for (int l = 0; l < block_nof_lines; l++) // for each line
	{
		int current_line_num = block_start_linenum + l;
		int acc_tabstop = 0;

		call_edit(SCI_CLEARTABSTOPS, current_line_num);

		for (size_t t = 0; t < grid[l].size(); t++)
		{
			if (grid[l][t].widest_width_pix != nullptr)
			{
				acc_tabstop += *(grid[l][t].widest_width_pix);
				call_edit(SCI_ADDTABSTOP, current_line_num, acc_tabstop);
			}
			else
			{
				break;
			}
		}
	}

	return;
}

static void setup(HWND sci, const Configuration *config) {
	// Get the direct pointer and function. Not the cleanest but it works for now
	edit = SendMessage(sci, SCI_GETDIRECTPOINTER, 0, 0);
	func = (SciFnDirect)SendMessage(sci, SCI_GETDIRECTFUNCTION, 0, 0);

	// Adjust widths based on character size
	// The width of a tab is (tab_width_minimum + tab_width_padding)
	// Since the user can adjust the padding we adjust the minimum
	const int char_width = call_edit(SCI_TEXTWIDTH, STYLE_DEFAULT, (LONG_PTR)"A");
	tab_width_padding = char_width * config->min_padding;
	tab_width_minimum = __max(char_width * call_edit(SCI_GETTABWIDTH) - tab_width_padding, 0);

#ifdef _DEBUG
	// Clear all the debugging junk, this way it only shows updates when it is actually recomputed
	call_edit(SCI_MARKERDELETEALL, MARK_UNDERLINE);
	for (int i = 0; i < DBG_INDICATORS; ++i) {
		call_edit(SCI_SETINDICATORCURRENT, i);
		call_edit(SCI_INDICATORCLEARRANGE, 0, call_edit(SCI_GETTEXTLENGTH));
	}
#endif
}

void ElasticTabstops_ComputeEntireDoc(HWND sci, const Configuration *config) {
	setup(sci, config);
	stretch_tabstops(0, call_edit(SCI_GETLINECOUNT));
}

void ElasticTabstops_OnModify(HWND sci, const Configuration *config, int start, int end, const char *text) {
	// Only stretch tabs if it is using actual tab characters
	if (SendMessage(sci, SCI_GETUSETABS, 0, 0) == 0) return;

	// Setup some stuff
	setup(sci, config);

	// If the modifications happen on a single line, we can do some heuristics to skip some computaions
	if (strchr(text, '\n') == NULL && strchr(text, '\r') == NULL && strchr(text, '\t') == NULL) {
		// See if there are any tabs after the inserted text
		if (get_nof_tabs_between(end, get_line_end(end)) == 0) return;
	}

	start = get_block_boundary(start, BACKWARDS);

	int block_start_linenum = call_edit(SCI_LINEFROMPOSITION, start);
	int block_min_end = call_edit(SCI_LINEFROMPOSITION, end);

	stretch_tabstops(block_start_linenum, block_min_end);
}

void ElasticTabstops_OnReady(HWND sci) {
#ifdef _DEBUG
	// Setup the markers for start/end of the computed block
	int mask = SendMessage(sci, SCI_GETMARGINMASKN, SC_MARGIN_SYBOL, 0);
	SendMessage(sci, SCI_SETMARGINMASKN, SC_MARGIN_SYBOL, mask | (1 << MARK_UNDERLINE));
	SendMessage(sci, SCI_MARKERDEFINE, MARK_UNDERLINE, SC_MARK_UNDERLINE);
	SendMessage(sci, SCI_MARKERSETBACK, MARK_UNDERLINE, 0x77CC77);

	// Setup indicators for column blocks
	for (int i = 0; i < DBG_INDICATORS; ++i) {
		SendMessage(sci, SCI_INDICSETSTYLE, i, INDIC_FULLBOX);
		SendMessage(sci, SCI_INDICSETALPHA, i, 200);
		SendMessage(sci, SCI_INDICSETOUTLINEALPHA, i, 255);
		SendMessage(sci, SCI_INDICSETUNDER, i, true);
	}

	// Setup indicator colors
	SendMessage(sci, SCI_INDICSETFORE, 0, 0x90EE90);
	SendMessage(sci, SCI_INDICSETFORE, 1, 0x8080F0);
	SendMessage(sci, SCI_INDICSETFORE, 2, 0xE6D8AD);
	SendMessage(sci, SCI_INDICSETFORE, 3, 0x0035DD);
	SendMessage(sci, SCI_INDICSETFORE, 4, 0x3939AA);
	SendMessage(sci, SCI_INDICSETFORE, 5, 0x396CAA);
	SendMessage(sci, SCI_INDICSETFORE, 6, 0x666622);
	SendMessage(sci, SCI_INDICSETFORE, 7, 0x2D882D);
#endif
}

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
#include "ScintillaGateway.h"

#define MARK_UNDERLINE 20
#define SC_MARGIN_SYBOL 1
#define DBG_INDICATORS  8

static ScintillaGateway editor;
static int tab_width_minimum;
static int tab_width_padding;
static int char_width;
static int(*get_text_width)(int start, int end);

static int startLine;
static int endLine;

enum direction {
	BACKWARDS,
	FORWARDS
};

static int get_line_start(int pos) {
	int line = editor.LineFromPosition(pos);
	return editor.PositionFromLine(line);
}

static int get_line_end(int pos) {
	int line = editor.LineFromPosition(pos);
	return editor.GetLineEndPosition(line);
}

static void clear_debug_marks() {
#ifdef _DEBUG
	// Clear all the debugging junk, this way it only shows updates when it is actually recomputed
	editor.MarkerDeleteAll(MARK_UNDERLINE);
	for (int i = 0; i < DBG_INDICATORS; ++i) {
		editor.SetIndicatorCurrent(i);
		editor.IndicatorClearRange(0, editor.GetTextLength());
	}
#endif
}

static int get_text_width_prop(int start, int end) {
	std::string s(end - start + 1, 0);

	TextRange range;
	range.chrg.cpMin = start;
	range.chrg.cpMax = end;
	range.lpstrText = &s[0];
	editor.GetTextRange(&range);

	int style = editor.GetStyleAt(start);

	return editor.TextWidth(style, range.lpstrText);
}

static int get_text_width_mono(int start, int end) {
	return (end - start) * char_width;
}

static int calc_tab_width(int text_width_in_tab) {
	text_width_in_tab = __max(text_width_in_tab, tab_width_minimum);
	return text_width_in_tab + tab_width_padding;
}

static bool change_line(int& location, direction which_dir) {
	int line = editor.LineFromPosition(location);
	if (which_dir == FORWARDS) {
		location = editor.PositionFromLine(line + 1);
	}
	else {
		if (line <= 0) {
			return false;
		}
		location = editor.PositionFromLine(line - 1);
	}
	return (location >= 0);
}

static int get_nof_tabs_between(int start, int end) {
	unsigned char current_char = 0;
	int tabs = 0;

	while (start < end && (current_char = (unsigned char)editor.GetCharAt(start))) {
		if (current_char == '\t') {
			tabs++;
		}
		start = editor.PositionAfter(start);
	}

	return tabs;
}

struct et_tabstop {
	int cell_width_pix; // Width of the cell
	int text_width_pix; // Width of the text within the cell
	int *widest_width_pix; // Width of the widest cell

	// Length of the tab
	int getTabLen() const {
		return *widest_width_pix - text_width_pix;
	}
};

static void measure_cells(std::vector<std::vector<et_tabstop>> &grid, int start_line, int end_line, size_t editted_cell) {
	int current_pos = editor.PositionFromLine(start_line);
	direction which_dir = (start_line <= end_line ? FORWARDS : BACKWARDS);

	do {
		unsigned char current_char = (unsigned char)editor.GetCharAt(current_pos);
		const int line_end = get_line_end(current_pos);
		int cell_start = current_pos;
		bool cell_empty = true;
		size_t cell_num = 0;
		std::vector<et_tabstop> grid_line;

		while (current_pos != line_end) {
			if (current_char == '\t') {
				if (cell_num >= editted_cell) {
#ifdef _DEBUG
					// Highlight the cell
					editor.SetIndicatorCurrent(grid_line.size() % DBG_INDICATORS);
					if (cell_empty) editor.IndicatorFillRange(current_pos, 1);
					else editor.IndicatorFillRange(cell_start, current_pos - cell_start + 1);
#endif
					int text_width_in_tab = 0;
					if (!cell_empty) {
						text_width_in_tab = get_text_width(cell_start, current_pos);
					}
					grid_line.push_back({ calc_tab_width(text_width_in_tab), text_width_in_tab, nullptr });
				}
				else {
					grid_line.push_back({ 0, 0, nullptr });
				}
				cell_empty = true;
				cell_num++;
			}
			else {
				if (cell_empty) {
					cell_start = current_pos;
					cell_empty = false;
				}
			}

			current_pos = editor.PositionAfter(current_pos);
			current_char = (unsigned char)editor.GetCharAt(current_pos);
		}

		if (grid_line.size() <= editted_cell && !(which_dir == FORWARDS && editor.LineFromPosition(current_pos) <= end_line)) {
			break;
		}

		grid.push_back(grid_line);

		if (current_char == '\0') break;

		int cur_line = editor.LineFromPosition(current_pos);
		if (cur_line < startLine || cur_line > endLine) break;
	} while (change_line(current_pos, which_dir));

	return;
}

static void stretch_cells(std::vector<std::vector<et_tabstop>> &grid, size_t start_cell, size_t max_tabs) {
	// Find columns blocks and stretch to fit the widest cell
	for (size_t t = start_cell; t < max_tabs; t++) {
		bool starting_new_block = true;
		size_t first_line_in_block = 0;
		int max_width = 0;
		for (size_t l = 0; l < grid.size(); l++) {
			if (starting_new_block) {
				starting_new_block = false;
				first_line_in_block = l;
				max_width = 0;
			}
			if (t < grid[l].size()) {
				grid[l][t].widest_width_pix = &(grid[first_line_in_block][t].cell_width_pix); // point widestWidthPix at first 
				if (grid[l][t].cell_width_pix > max_width) {
					max_width = grid[l][t].cell_width_pix;
					grid[first_line_in_block][t].cell_width_pix = max_width;
				}
			}
			else {
				// end column block
				starting_new_block = true;
			}
		}
	}
}

static void stretch_tabstops(int block_edit_linenum, int block_min_end, int editted_cell) {
	std::vector<std::vector<et_tabstop>> grid;
	size_t max_tabs = 0;
	size_t block_start_linenum;

	if (block_edit_linenum > 0) {
		measure_cells(grid, block_edit_linenum - 1, -1, editted_cell);
		std::reverse(grid.begin(), grid.end());
	}
	block_start_linenum = block_edit_linenum - grid.size();
	measure_cells(grid, block_edit_linenum, block_min_end, editted_cell);

	for (const auto &grid_line : grid) {
		max_tabs = __max(max_tabs, grid_line.size());
	}

	if (grid.size() == 0 || max_tabs == 0) return;

#ifdef _DEBUG
	// Mark the start and end of the block being recomputed
	editor.MarkerAdd((int)block_start_linenum - 1, MARK_UNDERLINE);
	editor.MarkerAdd((int)(block_start_linenum + grid.size() - 1), MARK_UNDERLINE);
#endif

	stretch_cells(grid, editted_cell, max_tabs);

	// Anything before the editted cell we can keep because we already know what it is
	std::vector<int> known_tabstops;
	int cur_tabstop = 0;
	for (int i = 0; i < editted_cell; i++) {
		cur_tabstop = editor.GetNextTabStop((int)block_start_linenum, cur_tabstop);
		known_tabstops.push_back(cur_tabstop);
	}

	// Set tabstops
	for (size_t l = 0; l < grid.size(); l++) {
		size_t current_line_num = block_start_linenum + l;
		int acc_tabstop = 0;

		editor.ClearTabStops((int)current_line_num);

		// Set any known tabstops
		for (size_t t = 0; t < known_tabstops.size(); t++) {
			acc_tabstop = known_tabstops[t];
			editor.AddTabStop((int)current_line_num, acc_tabstop);
		}

		for (size_t t = known_tabstops.size(); t < grid[l].size(); t++) {
			acc_tabstop += *(grid[l][t].widest_width_pix);
			editor.AddTabStop((int)current_line_num, acc_tabstop);
		}
	}

	return;
}

static void replace_nth_tab(int linenum, int cellnum, const char *text) {
	TextToFind ttf;

	ttf.chrg.cpMin = editor.PositionFromLine(linenum);
	ttf.chrg.cpMax = editor.GetLineEndPosition(linenum);
	ttf.lpstrText = "\t";

	int position = INVALID_POSITION;
	int curcell = -1;
	do {
		position = editor.FindText(0, &ttf);
		ttf.chrg.cpMin = position + 1; // Move start of the search forward
		curcell++;
	} while (position != INVALID_POSITION && curcell != cellnum);

	if (position == INVALID_POSITION) {
		return;
	}

	editor.SetTargetRange(position, position + 1);
	editor.ReplaceTarget(-1, text);
}

void ElasticTabstopsSwitchToScintilla(HWND sci, const Configuration *config) {
	editor.SetScintillaInstance(sci);

	// Adjust widths based on character size
	// The width of a tab is (tab_width_minimum + tab_width_padding)
	// Since the user can adjust the padding we adjust the minimum
	char_width = editor.TextWidth(STYLE_DEFAULT, " ");
	tab_width_padding = (int)(char_width * config->min_padding);
	tab_width_minimum = __max(char_width * editor.GetTabWidth() - tab_width_padding, 0);

	get_text_width = get_text_width_prop;
}

void ElasticTabstopsComputeCurrentView() {
	int linesOnScreen = editor.LinesOnScreen();
	startLine = editor.GetFirstVisibleLine();
	endLine = startLine + linesOnScreen + 1;

	// Expand up to 1 "screen" worth in both directions
	startLine -= linesOnScreen;
	endLine += linesOnScreen;

	startLine = __max(startLine, 0);
	endLine = __min(endLine, editor.GetLineCount());

	clear_debug_marks();
	stretch_tabstops(startLine, endLine, 0);
}

void ElasticTabstopsOnModify(int start, int end, int linesAdded, bool hasTab) {
	clear_debug_marks();

	int editted_cell = 0;
	// If the modifications happen on a single line and doesnt add/remove tabs, we can do some heuristics to skip some computations
	if (linesAdded == 0 && !hasTab) {
		// See if there are any tabs after the inserted/removed text
		if (get_nof_tabs_between(end, get_line_end(end)) == 0) return;

		// Find which cell was actually changed
		editted_cell = get_nof_tabs_between(get_line_start(start), start);
	}

	int block_start_linenum = editor.LineFromPosition(start);

	stretch_tabstops(block_start_linenum, block_start_linenum + (linesAdded > 0 ? linesAdded : 0), editted_cell);
}

void ElasticTabstopsConvertToSpaces(const Configuration *config) {
	std::vector<std::vector<et_tabstop>> grid;

	// Recompute the entire document
	startLine = 0;
	endLine = editor.GetLineCount();
	measure_cells(grid, 0, editor.GetLineCount(), 0);

	clear_debug_marks();

	size_t max_tabs = 0;
	for (const auto &grid_line : grid) {
		max_tabs = __max(max_tabs, grid_line.size());
	}

	if (grid.size() == 0 || max_tabs == 0) return;

	stretch_cells(grid, 0, max_tabs);

	editor.BeginUndoAction();
	for (size_t linenum = 0; linenum < grid.size(); ++linenum) {
		editor.ClearTabStops((int) linenum);

		int start_cell = 0;

		if (!config->convert_leading_tabs_to_spaces) {
			const int default_width = calc_tab_width(0);

			// Assume any leading "normal" tabs are for indentation
			while (start_cell < (int)grid[linenum].size() &&
				grid[linenum][start_cell].text_width_pix == 0 &&
				grid[linenum][start_cell].getTabLen() == default_width)
				start_cell++;
		}

		// Iterate backwards since tabs are being removed, thus it wouldn't find the correct "nth" tab
		for (int end_cell = (int)grid[linenum].size() - 1; end_cell >= start_cell; --end_cell) {
			int spaces = grid[linenum][end_cell].getTabLen() / char_width;
			replace_nth_tab((int)linenum, (int)end_cell, std::string(spaces, ' ').c_str());
		}
	}
	editor.EndUndoAction();
}

void ElasticTabstopsOnReady(HWND sci) {
#ifdef _DEBUG
	// Setup the markers for start/end of the computed block
	int mask = (int)SendMessage(sci, SCI_GETMARGINMASKN, SC_MARGIN_SYBOL, 0);
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

// Copyright (c) 2016 Martin Ridgers
// License: http://opensource.org/licenses/MIT

#include "pch.h"
#include "tab_completer.h"
#include "binder.h"
#include "editor_module.h"
#include "line_buffer.h"
#include "line_state.h"
#include "matches.h"

#include <core/base.h>
#include <core/settings.h>
#include <core/str_iter.h>
#include <terminal/printer.h>
#include <terminal/setting_colour.h>

//------------------------------------------------------------------------------
editor_module* tab_completer_create()
{
    return new tab_completer();
}

//------------------------------------------------------------------------------
void tab_completer_destroy(editor_module* completer)
{
    delete completer;
}



//------------------------------------------------------------------------------
static setting_int g_query_threshold(
    "match.query_threshold",
    "Ask if no. matches > threshold",
    "If there are more than 'threshold' matches then ask the user before\n"
    "displaying them all.",
    100);

static setting_bool g_vertical(
    "match.vertical",
    "Display matches vertically",
    "Toggles the display of ordered matches between columns or rows.",
    true);

static setting_int g_column_pad(
    "match.column_pad",
    "Space between columns",
    "Adjusts the amount of whitespace padding between columns of matches.",
    2);

setting_int g_max_width(
    "match.max_width",
    "Maximum display width",
    "The maximum number of terminal columns to use when displaying matches.",
    106);

setting_colour g_colour_interact(
    "colour.interact",
    "For user-interaction prompts",
    "Used when Clink displays text or prompts such as a pager's 'More?'. Naming\n"
    "these settings is hard. Describing them even more so.",
    setting_colour::value_light_magenta, setting_colour::value_bg_default);

setting_colour g_colour_minor(
    "colour.minor",
    "Minor colour value",
    "The colour used to display minor elements such as the lower common\n"
    "denominator of active matches in tab completion's display.",
    setting_colour::value_grey, setting_colour::value_bg_default);

setting_colour g_colour_major(
    "colour.major",
    "Major colour value",
    "The colour used to display major elements like remainder of active matches\n"
    "still to be completed.",
    setting_colour::value_white, setting_colour::value_bg_default);

setting_colour g_colour_highlight(
    "colour.highlight",
    "Colour for highlights",
    "The colour used for displaying highlighted elements such as the next\n"
    "character when invoking tab completion.",
    setting_colour::value_white, setting_colour::value_red);



//------------------------------------------------------------------------------
enum {
    bind_id_prompt      = 20,
    bind_id_prompt_yes,
    bind_id_prompt_no,
    bind_id_pager_page,
    bind_id_pager_line,
    bind_id_pager_stop,
};



//------------------------------------------------------------------------------
void tab_completer::bind_input(binder& binder)
{
    int default_group = binder.get_group();
    binder.bind(default_group, "\t", state_none);

    m_prompt_bind_group = binder.create_group("tab_complete_prompt");
    binder.bind(m_prompt_bind_group, "y", bind_id_prompt_yes);
    binder.bind(m_prompt_bind_group, "Y", bind_id_prompt_yes);
    binder.bind(m_prompt_bind_group, " ", bind_id_prompt_yes);
    binder.bind(m_prompt_bind_group, "\t", bind_id_prompt_yes);
    binder.bind(m_prompt_bind_group, "\r", bind_id_prompt_yes);
    binder.bind(m_prompt_bind_group, "n", bind_id_prompt_no);
    binder.bind(m_prompt_bind_group, "N", bind_id_prompt_no);
    binder.bind(m_prompt_bind_group, "^C", bind_id_prompt_no); // ctrl-c
    binder.bind(m_prompt_bind_group, "^D", bind_id_prompt_no); // ctrl-d
    binder.bind(m_prompt_bind_group, "^[", bind_id_prompt_no); // esc

    m_pager_bind_group = binder.create_group("tab_complete_pager");
    binder.bind(m_pager_bind_group, " ", bind_id_pager_page);
    binder.bind(m_pager_bind_group, "\t", bind_id_pager_page);
    binder.bind(m_pager_bind_group, "\r", bind_id_pager_line);
    binder.bind(m_pager_bind_group, "q", bind_id_pager_stop);
    binder.bind(m_pager_bind_group, "Q", bind_id_pager_stop);
    binder.bind(m_pager_bind_group, "^C", bind_id_pager_stop); // ctrl-c
    binder.bind(m_pager_bind_group, "^D", bind_id_pager_stop); // ctrl-d
    binder.bind(m_pager_bind_group, "^[", bind_id_pager_stop); // esc
}

//------------------------------------------------------------------------------
void tab_completer::on_begin_line(const char* prompt, const context& context)
{
}

//------------------------------------------------------------------------------
void tab_completer::on_end_line()
{
}

//------------------------------------------------------------------------------
void tab_completer::on_matches_changed(const context& context)
{
    m_waiting = false;
}

//------------------------------------------------------------------------------
void tab_completer::on_input(const input& input, result& result, const context& context)
{
    auto& matches = context.matches;
    if (matches.get_match_count() == 0)
        return;

    if (!m_waiting)
    {
        // One match? Accept it.
        if (matches.get_match_count() == 1)
        {
            result.accept_match(0);
            return;
        }

        // Append as much of the lowest common denominator of matches as we can. If
        // there is an LCD then on_matches_changed() gets called.
        m_waiting = true;
        result.append_match_lcd();
        return;
    }

    const char* keys = input.keys;
    int next_state = state_none;

    switch (input.id)
    {
    case state_none:            next_state = begin_print(context);  break;
    case bind_id_prompt_no:     next_state = state_none;            break;
    case bind_id_prompt_yes:    next_state = state_print_page;      break;
    case bind_id_pager_page:    next_state = state_print_page;      break;
    case bind_id_pager_line:    next_state = state_print_one;       break;
    case bind_id_pager_stop:    next_state = state_none;            break;
    }

    if (next_state > state_print)
        next_state = print(context, next_state == state_print_one);

    // 'm_prev_group' is >= 0 if tab completer has set a bind group. As the bind
    // groups are one-shot we restore the original back each time.
    if (m_prev_group != -1)
    {
        result.set_bind_group(m_prev_group);
        m_prev_group = -1;
    }

    switch (next_state)
    {
    case state_query:
        m_prev_group = result.set_bind_group(m_prompt_bind_group);
        return;

    case state_pager:
        m_prev_group = result.set_bind_group(m_pager_bind_group);
        return;
    }

    context.printer.print("\n");
    result.redraw();
}

//------------------------------------------------------------------------------
tab_completer::state tab_completer::begin_print(const context& context)
{
    const matches& matches = context.matches;
    int match_count = matches.get_match_count();

    m_longest = 0;
    m_row = 0;

    // Get the longest match length.
    for (int i = 0, n = matches.get_match_count(); i < n; ++i)
        m_longest = max<int>(matches.get_cell_count(i), m_longest);

    if (!m_longest)
        return state_none;

    context.printer.print("\n");

    int query_threshold = g_query_threshold.get();
    if (query_threshold > 0 && query_threshold <= match_count)
    {
        str<40> prompt;
        prompt.format("Show %d matches? [Yn]", match_count);
        context.printer.print(g_colour_interact.get(), prompt.c_str(), prompt.length());

        return state_query;
    }

    return state_print_page;
}

//------------------------------------------------------------------------------
tab_completer::state tab_completer::print(const context& context, bool single_row)
{
    auto& printer = context.printer;

    const matches& matches = context.matches;

    attributes minor_attr = g_colour_minor.get();
    attributes major_attr = g_colour_major.get();
    attributes highlight_attr = g_colour_highlight.get();

    printer.print("\r");

    int match_count = matches.get_match_count();

    str<288> lcd;
    matches.get_match_lcd(lcd);
    int lcd_length = lcd.length();

    // Calculate the number of columns of matches per row.
    int column_pad = g_column_pad.get();
    int cell_columns = min<int>(g_max_width.get(), printer.get_columns());
    int columns = max(1, (cell_columns + column_pad) / (m_longest + column_pad));
    int total_rows = (match_count + columns - 1) / columns;

    bool vertical = g_vertical.get();
    int index_step = vertical ? total_rows : 1;

    int max_rows = single_row ? 1 : (total_rows - m_row - 1);
    max_rows = min<int>(printer.get_rows() - 2 - (m_row != 0), max_rows);
    for (; max_rows >= 0; --max_rows, ++m_row)
    {
        int index = vertical ? m_row : (m_row * columns);
        for (int x = columns - 1; x >= 0; --x)
        {
            if (index >= match_count)
                continue;

            // Print the match.
            const char* match = matches.get_displayable(index);
            const char* post_lcd = match + lcd_length;

            str_iter iter(post_lcd);
            iter.next();
            const char* match_tail = iter.get_pointer();

            printer.print(minor_attr, match, lcd_length);
            printer.print(highlight_attr, post_lcd, int(match_tail - post_lcd));
            printer.print(major_attr, match_tail, int(strlen(match_tail)));

            // Move the cursor to the next column
            if (x)
            {
                int visible_chars = matches.get_cell_count(index);
                for (int i = m_longest - visible_chars + column_pad; i >= 0;)
                {
                    const char spaces[] = "                ";
                    printer.print(spaces, min<int>(sizeof_array(spaces) - 1, i));
                    i -= sizeof_array(spaces) - 1;
                }
            }

            index += index_step;
        }

        printer.print("\n");
    }

    if (m_row == total_rows)
        return state_none;

    printer.print(g_colour_interact.get(), "-- More --");
    return state_pager;
}

//------------------------------------------------------------------------------
void tab_completer::on_terminal_resize(int columns, int rows, const context& context)
{
}

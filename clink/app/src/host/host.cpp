// Copyright (c) 2016 Martin Ridgers
// License: http://opensource.org/licenses/MIT

#include "pch.h"
#include "host.h"
#include "host_lua.h"
#include "host_module.h"
#include "prompt.h"
#include "utils/app_context.h"
#include "utils/scroller.h"

#include <core/globber.h>
#include <core/os.h>
#include <core/path.h>
#include <core/settings.h>
#include <core/str.h>
#include <core/str_compare.h>
#include <core/str_tokeniser.h>
#include <lib/match_generator.h>
#include <lib/line_editor.h>
#include <lua/lua_script_loader.h>
#include <lua/lua_state.h>
#include <lua/lua_match_generator.h>
#include <terminal/terminal.h>

extern "C" {
#include <lua.h>
}

//------------------------------------------------------------------------------
static setting_enum g_ignore_case(
    "match.ignore_case",
    "Case insensitive matching",
    "Toggles whether case is ignored when selecting matches. The 'relaxed'\n"
    "option will also consider -/_ as equal.",
    "off,on,relaxed",
    2);

static setting_bool g_add_history_cmd(
    "history.add_history_cmd",
    "Add 'history' commands",
    "Toggles the adding of 'history' commands to the history.",
    true);



//------------------------------------------------------------------------------
host::host(const char* name)
: m_name(name)
{
}

//------------------------------------------------------------------------------
host::~host()
{
}

//------------------------------------------------------------------------------
bool host::edit_line(const char* prompt, str_base& out)
{
    const app_context* app = app_context::get();
    app->update_env();

    struct cwd_restorer
    {
        cwd_restorer()  { os::get_current_dir(m_path); }
        ~cwd_restorer() { os::set_current_dir(m_path.c_str()); }
        str<288>        m_path;
    } cwd;

    // Load Clink's settings.
    str<288> settings_file;
    app_context::get()->get_settings_path(settings_file);
    settings::load(settings_file.c_str());

    // Set up the string comparison mode.
    int cmp_mode;
    switch (g_ignore_case.get())
    {
    case 1:     cmp_mode = str_compare_scope::caseless; break;
    case 2:     cmp_mode = str_compare_scope::relaxed;  break;
    default:    cmp_mode = str_compare_scope::exact;    break;
    }
    str_compare_scope compare(cmp_mode);

    // Set up Lua and load scripts into it.
    host_lua lua;
    prompt_filter prompt_filter(lua);
    initialise_lua(lua);
    lua.load_scripts();

    // Unfortunately we need to load settings again because some settings don't
    // exist until after Lua's up and running. But.. we can't load Lua scripts
    // without loading settings first. [TODO: find a better way]
    settings::load(settings_file.c_str());

    line_editor::desc desc = {};
    initialise_editor_desc(desc);

    // Filter the prompt.
    str<256> filtered_prompt;
    prompt_filter.filter(prompt, filtered_prompt);
    desc.prompt = filtered_prompt.c_str();

    // Set the terminal that will handle all IO while editing.
    terminal terminal = terminal_create();
    desc.input = terminal.in;
    desc.output = terminal.out;

    // Create the editor and add components to it.
    line_editor* editor = line_editor_create(desc);

    editor_module* completer = tab_completer_create();
    editor->add_module(*completer);

    scroller_module scroller;
    editor->add_module(scroller);

    host_module host_module(m_name);
    editor->add_module(host_module);

    editor->add_generator(lua);
    editor->add_generator(file_match_generator());

    m_history.initialise();
    m_history.load_rl_history();

    bool ret = false;
    while (1)
    {
        if (ret = editor->edit(out.data(), out.size()))
        {
            // Handle history event expansion.
            if (m_history.expand(out.c_str(), out) == history_db::expand_print)
            {
                puts(out.c_str());
                continue;
            }

            // Should we skip adding lines begining with 'history'?
            if (!g_add_history_cmd.get())
            {
                const char* c = out.c_str();
                while (*c == ' ' || *c == '\t')
                    ++c;

                if (_strnicmp(c, "history", 7) == 0)
                    break;
            }

            // Add the line to the history.
            m_history.add(out.c_str());
        }
        break;
    }

    line_editor_destroy(editor);
    tab_completer_destroy(completer);
    terminal_destroy(terminal);
    return ret;
}

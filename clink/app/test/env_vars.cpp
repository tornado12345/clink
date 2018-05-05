// Copyright (c) 2012 Martin Ridgers
// License: http://opensource.org/licenses/MIT

#include "pch.h"
#include "env_fixture.h"
#include "fs_fixture.h"
#include "line_editor_tester.h"

#include <core/str_compare.h>
#include <lua/lua_match_generator.h>
#include <lua/lua_script_loader.h>
#include <lua/lua_state.h>

//------------------------------------------------------------------------------
TEST_CASE("Env. vars")
{
    static const char* env_desc[] = {
        "simple",   "0",
        "case_map", "0",
        "dash-1",   "0",
        "dash_2",   "0",
        nullptr
    };

    fs_fixture fs;
    env_fixture env(env_desc);

    lua_state lua;
    lua_match_generator lua_generator(lua);
    lua_load_script(lua, app, env);

    line_editor::desc desc;
    desc.word_delims = " =";
    line_editor_tester tester(desc);
    tester.get_editor()->add_generator(lua_generator);

    SECTION("Basic")
    {
        tester.set_input("nullcmd %simp");
        tester.set_expected_matches("simple");
        tester.run();
    }

    SECTION("Second %var%")
    {
        tester.set_input("nullcmd %simple% %sim" DO_COMPLETE);
        tester.set_expected_output("nullcmd %simple% %simple%");
        tester.run();
    }

    SECTION("Case mapped 1")
    {
        str_compare_scope _(str_compare_scope::relaxed);

        tester.set_input("nullcmd %case_m");
        tester.set_expected_matches("case_map");
        tester.run();
    }

    SECTION("Case mapped 2")
    {
        str_compare_scope _(str_compare_scope::relaxed);

        tester.set_input("nullcmd %dash-");
        tester.set_expected_matches("dash-1", "dash_2");
        tester.run();
    }

    SECTION("Mid-word 1")
    {
        tester.set_input("nullcmd One%Two%Three%dash");
        tester.set_expected_matches("dash-1", "dash_2");
        tester.run();
    }

    SECTION("Mid-word 2")
    {
        tester.set_input("nullcmd One%Two%");
        tester.set_expected_matches();
        tester.run();
    }

    SECTION("Not in quotes")
    {
        tester.set_input("nullcmd \"arg\" %simp" DO_COMPLETE);
        tester.set_expected_output("nullcmd \"arg\" %simple%");
        tester.run();
    }

    SECTION("In quotes")
    {
        tester.set_input("nullcmd \"arg %sim" DO_COMPLETE);
        tester.set_expected_output("nullcmd \"arg %simple%");
        tester.run();
    }
}

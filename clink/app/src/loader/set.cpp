// Copyright (c) 2013 Martin Ridgers
// License: http://opensource.org/licenses/MIT

#include "pch.h"
#include "host/host_lua.h"
#include "utils/app_context.h"

#include <core/base.h>
#include <core/path.h>
#include <core/settings.h>
#include <core/str.h>
#include <core/str_tokeniser.h>
#include <lua/lua_script_loader.h>

#include <getopt.h>

//------------------------------------------------------------------------------
void puts_help(const char**, int);

//------------------------------------------------------------------------------
static void list_keys()
{
    for (auto* next = settings::first(); next != nullptr; next = next->next())
        puts(next->get_name());
}

//------------------------------------------------------------------------------
static void list_options(const char* key)
{
    const setting* setting = settings::find(key);
    if (setting == nullptr)
        return;

    switch (setting->get_type())
    {
    case setting::type_int:
    case setting::type_string:
        break;

    case setting::type_bool:
        puts("true");
        puts("false");
        break;

    case setting::type_enum:
        {
            const char* options = ((const setting_enum*)setting)->get_options();
            str_tokeniser tokens(options, ",");
            const char* start;
            int length;
            while (tokens.next(start, length))
                printf("%.*s\n", length, start);
        }
        break;
    }

    puts("clear");
}

//------------------------------------------------------------------------------
static bool print_keys()
{
    int longest = 0;
    for (auto* next = settings::first(); next != nullptr; next = next->next())
        longest = max(longest, int(strlen(next->get_name())));

    for (auto* next = settings::first(); next != nullptr; next = next->next())
    {
        str<> value;
        next->get(value);
        const char* name = next->get_name();
        printf("%-*s  %s\n", longest, name, value.c_str());
    }

    return true;
}

//------------------------------------------------------------------------------
static bool print_value(const char* key)
{
    const setting* setting = settings::find(key);
    if (setting == nullptr)
    {
        printf("ERROR: Setting '%s' not found.\n", key);
        return false;
    }

    printf("        Name: %s\n", setting->get_name());
    printf(" Description: %s\n", setting->get_short_desc());

    // Output an enum-type setting's options.
    if (setting->get_type() == setting::type_enum)
        printf("     Options: %s\n", ((setting_enum*)setting)->get_options());

    str<> value;
    setting->get(value);
    printf("       Value: %s\n", value.c_str());

    const char* long_desc = setting->get_long_desc();
    if (long_desc != nullptr && *long_desc)
        printf("\n%s\n", setting->get_long_desc());

    return true;
}

//------------------------------------------------------------------------------
static bool set_value(const char* key, const char* value)
{
    setting* setting = settings::find(key);
    if (setting == nullptr)
    {
        printf("ERROR: Setting '%s' not found.\n", key);
        return false;
    }

    if (value == nullptr)
    {
        setting->set();
    }
    else if (!setting->set(value))
    {
        printf("ERROR: Failed to set value '%s'.\n", key);
        return false;
    }

    str<> result;
    setting->get(result);
    printf("Setting '%s' %sset to '%s'\n", key, value ? "" : "re", result.c_str());
    return true;
}

//------------------------------------------------------------------------------
static void print_help()
{
    extern const char* g_clink_header;

    const char* help[] = {
        "setting_name", "Name of the setting who's value is to be set.",
        "value",        "Value to set the setting to."
    };

    puts(g_clink_header);
    puts("Usage: set [<setting_name> [clear|<value>]]\n");

    puts_help(help, sizeof_array(help));

    puts("If 'settings_name' is omitted then all settings are listed. Omit 'value'\n"
        "for more detailed info about a setting and use a value of 'clear' to reset\n"
        "the setting to its default value.\n");
}

//------------------------------------------------------------------------------
int set(int argc, char** argv)
{
    // Parse command line arguments.
    struct option options[] = {
        { "help", no_argument, nullptr, 'h' },
        { "list", no_argument, nullptr, 'l' },
        {}
    };

    bool complete = false;
    int i;
    while ((i = getopt_long(argc, argv, "+hl", options, nullptr)) != -1)
    {
        switch (i)
        {
        default:
        case 'h': print_help();     return 0;
        case 'l': complete = true;  break;
        }
    }

    // Load the settings from disk.
    str<280> settings_file;
    app_context::get()->get_settings_path(settings_file);
    settings::load(settings_file.c_str());

    // Load all lua state too as there is settings declared in scripts.
    host_lua lua;
    lua_load_script(lua, app, exec);
    lua.load_scripts();

    // Loading settings _again_ now Lua's initialised ... :(
    settings::load(settings_file.c_str());

    // List or set Clink's settings.
    if (complete)
    {
        (optind < argc) ? list_options(argv[optind]) : list_keys();
        return 0;
    }

    bool clear = false;
    switch (argc - optind)
    {
    case 0:
        return (print_keys() != true);

    case 1:
        if (!clear)
            return (print_value(argv[1]) != true);
        return print_help(), 0;

    default:
        if (_stricmp(argv[2], "clear") == 0)
        {
            if (set_value(argv[1], nullptr))
                return settings::save(settings_file.c_str()), 0;
        }
        else if (set_value(argv[1], argv[2]))
            return settings::save(settings_file.c_str()), 0;
    }

    return 1;
}

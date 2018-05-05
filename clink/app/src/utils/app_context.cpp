// Copyright (c) 2013 Martin Ridgers
// License: http://opensource.org/licenses/MIT

#include "pch.h"
#include "app_context.h"

#include <core/base.h>
#include <core/os.h>
#include <core/path.h>
#include <core/str.h>
#include <process/process.h>
#include <process/vm.h>

//------------------------------------------------------------------------------
app_context::desc::desc()
{
    state_dir[0] = '\0';
}



//-----------------------------------------------------------------------------
app_context::app_context(const desc& desc)
: m_desc(desc)
{
    str_base state_dir(m_desc.state_dir);

    // The environment variable 'clink_profile' overrides all other state
    // path mechanisms.
    if (state_dir.empty())
        os::get_env("clink_profile", state_dir);

    // Look for a state directory that's been inherited in our environment.
    if (state_dir.empty())
        os::get_env("=clink.profile", state_dir);

    // Still no state directory set? Derive one.
    if (state_dir.empty())
    {
        wstr<280> wstate_dir;
        if (SHGetFolderPathW(0, CSIDL_LOCAL_APPDATA, nullptr, 0, wstate_dir.data()) == S_OK)
            state_dir = wstate_dir.c_str();
        else if (!os::get_env("userprofile", state_dir))
            os::get_temp_dir(state_dir);

        if (!state_dir.empty())
            path::append(state_dir, "clink");
    }

    {
        str<280> cwd;
        os::get_current_dir(cwd);
        path::append(cwd, state_dir.c_str());
        path::normalise(state_dir);

        os::make_dir(state_dir.c_str());
    }

    m_id = process().get_pid();
    if (desc.inherit_id)
    {
        str<16, false> env_id;
        if (os::get_env("=clink.id", env_id))
            m_id = atoi(env_id.c_str());
    }

    update_env();
}

//-----------------------------------------------------------------------------
int app_context::get_id() const
{
    return m_id;
}

//------------------------------------------------------------------------------
bool app_context::is_logging_enabled() const
{
    return m_desc.log;
}

//------------------------------------------------------------------------------
bool app_context::is_quiet() const
{
    return m_desc.quiet;
}

//------------------------------------------------------------------------------
void app_context::get_binaries_dir(str_base& out) const
{
    out.clear();

    void* base = vm().get_alloc_base("");
    if (base == nullptr)
        return;

    wstr<280> wout;
    GetModuleFileNameW(HMODULE(base), wout.data(), wout.size());
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        return;

    // Check for a .origin suffix indicating that we're using a copied DLL.
    int out_length = wout.length();
    wout << L".origin";
    HANDLE origin = CreateFileW(wout.c_str(), GENERIC_READ, 0, nullptr,
        OPEN_EXISTING, 0, nullptr);
    if (origin != INVALID_HANDLE_VALUE)
    {
        DWORD read;
        int size = GetFileSize(origin, nullptr);
        out.reserve(size);
        ReadFile(origin, out.data(), size, &read, nullptr);
        CloseHandle(origin);
    }
    else
    {
        wout.truncate(out_length);
        out = wout.c_str();
    }

    path::get_directory(out);
}

//------------------------------------------------------------------------------
void app_context::get_state_dir(str_base& out) const
{
    out.copy(m_desc.state_dir);
}

//------------------------------------------------------------------------------
void app_context::get_log_path(str_base& out) const
{
    get_state_dir(out);
    path::append(out, "clink.log");
}

//------------------------------------------------------------------------------
void app_context::get_settings_path(str_base& out) const
{
    get_state_dir(out);
    path::append(out, "clink_settings");
}

//------------------------------------------------------------------------------
void app_context::get_history_path(str_base& out) const
{
    get_state_dir(out);
    path::append(out, "clink_history");
}

//-----------------------------------------------------------------------------
void app_context::update_env() const
{
    str<48> id_str;
    id_str.format("%d", m_id);
    os::set_env("=clink.id", id_str.c_str());
    os::set_env("=clink.profile", m_desc.state_dir);
}

// Copyright (c) 2015 Martin Ridgers
// License: http://opensource.org/licenses/MIT

#include "pch.h"
#include "base.h"
#include "path.h"
#include "str.h"

#if defined(PLATFORM_WINDOWS)
#   define PATH_SEP "\\"
#else
#   define PATH_SEP "/"
#endif

//------------------------------------------------------------------------------
static const char* get_last_separator(const char* in)
{
    for (int i = int(strlen(in)) - 1; i >= 0; --i)
        if (path::is_separator(in[i]))
            return in + i;

    return nullptr;
}

//------------------------------------------------------------------------------
static int get_directory_end(const char* path)
{
    if (const char* slash = get_last_separator(path))
    {
        // Trim consecutive slashes unless they're leading ones.
        const char* first_slash = slash;
        while (first_slash >= path)
        {
            if (!path::is_separator(*first_slash))
                break;

            --first_slash;
        }
        ++first_slash;

        if (first_slash != path)
            slash = first_slash;

        // Don't strip '/' if it's the first char.
        if (slash == path)
            ++slash;

#if defined(PLATFORM_WINDOWS)
        // Same for Windows and it's drive prefixes.
        if (path[0] && path[1] == ':' && slash == path + 2)
            ++slash;
#endif

        return int(slash - path);
    }

#if defined(PLATFORM_WINDOWS)
    if (path[0] && path[1] == ':')
        return 2;
#endif

    return 0;
}



namespace path
{

//------------------------------------------------------------------------------
void normalise(str_base& in_out, int sep)
{
    normalise(in_out.data(), sep);
}

//------------------------------------------------------------------------------
void normalise(char* in_out, int sep)
{
    if (!sep)
        sep = PATH_SEP[0];

#if defined(PLATFORM_WINDOWS)
    if (in_out[0] && in_out[1] == ':')
        in_out += 2;
#endif

    unsigned int piece_count = 0;

    char* __restrict write = in_out;
    if (is_separator(*write))
    {
        *write++ = char(sep);
        piece_count = INT_MAX;
    }

    const char* __restrict start = write;
    const char* __restrict read = write;
    for (; const char* __restrict next = next_element(read); read = next)
    {
        while (is_separator(*read))
            ++read;

        if (read[0] == '.')
        {
            bool two_dot = (read[1] == '.');

            char c = *(read + 1 + two_dot);
            if (is_separator(c) || !c)
            {
                if (!two_dot)
                    continue;

                if (piece_count)
                {
                    while (write > start)
                    {
                        --write;
                        if (is_separator(write[-1]))
                            break;
                    }

                    piece_count -= !!piece_count;
                    continue;
                }
            }
        }
        else
            ++piece_count;

        for (; read < next; ++read)
            *write++ = is_separator(*read) ? char(sep) : *read;
    }

    *write = '\0';
}

//------------------------------------------------------------------------------
bool is_separator(int c)
{
#if defined(PLATFORM_WINDOWS)
    return (c == '/' || c == '\\');
#else
    return (c == '/');
#endif
}

//------------------------------------------------------------------------------
const char* next_element(const char* in)
{
    if (*in == '\0')
        return nullptr;

    for (; is_separator(*in); ++in);
    for (; *in && !is_separator(*in); ++in);
    return in + !!*in;
}

//------------------------------------------------------------------------------
bool get_base_name(const char* in, str_base& out)
{
    if (!get_name(in, out))
        return false;

    int dot = out.last_of('.');
    if (dot >= 0)
        out.truncate(dot);

    return true;
}

//------------------------------------------------------------------------------
bool get_directory(const char* in, str_base& out)
{
    int end = get_directory_end(in);
    return out.concat(in, end);
}

//------------------------------------------------------------------------------
bool get_directory(str_base& in_out)
{
    int end = get_directory_end(in_out.c_str());
    in_out.truncate(end);
    return true;
}

//------------------------------------------------------------------------------
bool get_drive(const char* in, str_base& out)
{
#if defined(PLATFORM_WINDOWS)
    if ((in[1] != ':') || (unsigned(tolower(in[0]) - 'a') > ('z' - 'a')))
        return false;

    return out.concat(in, 2);
#else
    return false;
#endif
}

//------------------------------------------------------------------------------
bool get_drive(str_base& in_out)
{
#if defined(PLATFORM_WINDOWS)
    if ((in_out[1] != ':') || (unsigned(tolower(in_out[0]) - 'a') > ('z' - 'a')))
        return false;

    in_out.truncate(2);
    return (in_out.size() > 2);
#else
    return false;
#endif
}

//------------------------------------------------------------------------------
bool get_extension(const char* in, str_base& out)
{
    const char* dot = strrchr(in, '.');
    if (dot == nullptr)
        return false;

    if (dot[1] == '\0')
        return false;

    int end = get_directory_end(in);
    if (in + end > dot)
        return false;

    return out.concat(dot);
}

//------------------------------------------------------------------------------
bool get_name(const char* in, str_base& out)
{
    return out.concat(get_name(in));
}

//------------------------------------------------------------------------------
const char* get_name(const char* in)
{
    if (const char* slash = get_last_separator(in))
        return slash + 1;

#if defined(PLATFORM_WINDOWS)
    if (in[0] && in[1] == ':')
        in += 2;
#endif

    return in;
}

//------------------------------------------------------------------------------
bool is_rooted(const char* path)
{
#if defined(PLATFORM_WINDOWS)
    if (path[0] && path[1] == ':')
        path += 2;
#endif

    return is_separator(*path);
}

//------------------------------------------------------------------------------
bool is_root(const char* path)
{
#if defined(PLATFORM_WINDOWS)
    // Windows' drives prefixes.
    // "X:" ?
    if (path[0] && path[1] == ':')
    {
        if (path[2] == '\0')
            return true;

        // "X:\" or "X://" ?
        if (path[3] == '\0' && is_separator(path[2]))
            return true;
    }
#endif

    // "[/ or /]+" ?
    while (is_separator(*path))
        ++path;

    return (*path == '\0');
}

//------------------------------------------------------------------------------
bool join(const char* lhs, const char* rhs, str_base& out)
{
    out << lhs;
    return append(out, rhs);
}

//------------------------------------------------------------------------------
bool append(str_base& out, const char* rhs)
{
    if (is_rooted(rhs))
        return out.copy(rhs);

    bool add_seperator = true;

    int last = int(out.length() - 1);
    if (last >= 0)
    {
        add_seperator &= !is_separator(out[last]);

#if defined(PLATFORM_WINDOWS)
        add_seperator &= !(out[1] == ':' && out[2] == '\0');
#endif
    }
    else
        add_seperator = false;

    if (add_seperator && !is_separator(rhs[0]))
        out << PATH_SEP;

    return out.concat(rhs);
}

}; // namespace path

// Copyright (c) 2016 Martin Ridgers
// License: http://opensource.org/licenses/MIT

#pragma once

#include "attributes.h"

#include <core/base.h>
#include <core/settings.h>

#include <new>

//------------------------------------------------------------------------------
class setting_colour
{
public:
    #define COLOUR_X(x) value_##x,
    enum : unsigned char
    {
        COLOUR_XS

        value_fg_normal,
        value_fg_bright,
        value_fg_default,
        value_fg_count,

        value_bg_default = value_fg_normal,
        value_bg_count,
    };
    #undef COLOUR_X
                        setting_colour(const char* name, const char* short_desc, int default_fg, int default_bg);
                        setting_colour(const char* name, const char* short_desc, const char* long_desc, int default_fg, int default_bg);
    attributes          get() const;

private:
    template <class T, bool AUTO_DTOR=true>
    struct late
    {
        template <typename... ARGS>
        void            construct(ARGS... args) { new (buffer) T(args...); }
        void            destruct()              { (*this)->~T(); }
                        ~late()                 { if (AUTO_DTOR) destruct(); }
        T*              operator -> ()          { return (T*)buffer; }
        const T*        operator -> () const    { return (T*)buffer; }
        align_to(8)     char buffer[sizeof(T)];
    };

    late<setting_enum>  m_fg;
    late<setting_enum>  m_bg;
};

// Copyright (c) 2016 Martin Ridgers
// License: http://opensource.org/licenses/MIT

#include "pch.h"
#include "attributes.h"

static_assert(sizeof(attributes) == sizeof(long long), "sizeof(attr) != 64bits");

//------------------------------------------------------------------------------
enum
{
    default_code = 231, // because xterm256's 231 == old-school colour 15 (white)
};



//------------------------------------------------------------------------------
void attributes::colour::as_888(unsigned char (&out)[3]) const
{
    out[0] = (r << 3) | (r & 7);
    out[1] = (g << 3) | (g & 7);
    out[2] = (b << 3) | (b & 7);
}



//------------------------------------------------------------------------------
attributes::attributes()
: m_state(0)
{
}

//------------------------------------------------------------------------------
attributes::attributes(default_e)
: attributes()
{
    reset_fg();
    reset_bg();
    set_bold(false);
    set_underline(false);
}

//------------------------------------------------------------------------------
bool attributes::operator == (const attributes rhs)
{
    int cmp = 1;
    #define CMP_IMPL(x) (m_flags.x & rhs.m_flags.x) ? (m_##x == rhs.m_##x) : 1;
    cmp &= CMP_IMPL(fg);
    cmp &= CMP_IMPL(bg);
    cmp &= CMP_IMPL(bold);
    cmp &= CMP_IMPL(underline);
    #undef CMP_IMPL
    return (cmp != 0);
}

//------------------------------------------------------------------------------
attributes attributes::merge(const attributes first, const attributes second)
{
    attributes mask;
    mask.m_flags.all = ~0;
    mask.m_fg.value = second.m_flags.fg ? ~0 : 0;
    mask.m_bg.value = second.m_flags.bg ? ~0 : 0;
    mask.m_bold = second.m_flags.bold;
    mask.m_underline = second.m_flags.underline;

    attributes out;
    out.m_state = first.m_state & ~mask.m_state;
    out.m_state |= second.m_state & mask.m_state;
    out.m_flags.all |= first.m_flags.all;

    return out;
}

//------------------------------------------------------------------------------
attributes attributes::diff(const attributes from, const attributes to)
{
    flags changed;
    changed.fg = !(to.m_fg == from.m_fg);
    changed.bg = !(to.m_bg == from.m_bg);
    changed.bold = (to.m_bold != from.m_bold);
    changed.underline = (to.m_underline != from.m_underline);

    attributes out = to;
    out.m_flags.all &= changed.all;
    return out;
}

//------------------------------------------------------------------------------
void attributes::reset_fg()
{
    m_flags.fg = 1;
    m_fg.value = default_code;
}

//------------------------------------------------------------------------------
void attributes::reset_bg()
{
    m_flags.bg = 1;
    m_bg.value = default_code;
}

//------------------------------------------------------------------------------
void attributes::set_fg(unsigned char value)
{
    if (value == default_code)
        value = 15;

    m_flags.fg = 1;
    m_fg.value = value;
}

//------------------------------------------------------------------------------
void attributes::set_bg(unsigned char value)
{
    if (value == default_code)
        value = 15;

    m_flags.bg = 1;
    m_bg.value = value;
}

//------------------------------------------------------------------------------
void attributes::set_fg(unsigned char r, unsigned char g, unsigned char b)
{
    m_flags.fg = 1;
    m_fg.r = r >> 3;
    m_fg.g = g >> 3;
    m_fg.b = b >> 3;
    m_fg.is_rgb = 1;
}

//------------------------------------------------------------------------------
void attributes::set_bg(unsigned char r, unsigned char g, unsigned char b)
{
    m_flags.bg = 1;
    m_bg.r = r >> 3;
    m_bg.g = g >> 3;
    m_bg.b = b >> 3;
    m_bg.is_rgb = 1;
}

//------------------------------------------------------------------------------
void attributes::set_bold(bool state)
{
    m_flags.bold = 1;
    m_bold = !!state;
}

//------------------------------------------------------------------------------
void attributes::set_underline(bool state)
{
    m_flags.underline = 1;
    m_underline = !!state;
}

//------------------------------------------------------------------------------
attributes::attribute<attributes::colour> attributes::get_fg() const
{
    return { m_fg, m_flags.fg, (m_fg.value == default_code) };
}

//------------------------------------------------------------------------------
attributes::attribute<attributes::colour> attributes::get_bg() const
{
    return { m_bg, m_flags.bg, (m_bg.value == default_code) };
}

//------------------------------------------------------------------------------
attributes::attribute<bool> attributes::get_bold() const
{
    return { bool(m_bold), bool(m_flags.bold) };
}

//------------------------------------------------------------------------------
attributes::attribute<bool> attributes::get_underline() const
{
    return { bool(m_underline), bool(m_flags.underline) };
}

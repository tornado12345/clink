// Copyright (c) 2015 Martin Ridgers
// License: http://opensource.org/licenses/MIT

#pragma once

#include <core/str_iter.h>

template <typename T> class array;

//------------------------------------------------------------------------------
struct word
{
    unsigned int        offset : 14;
    unsigned int        length : 10;
    unsigned int        quoted : 1;
    unsigned int        delim  : 7;
};

//------------------------------------------------------------------------------
class line_state
{
public:
                        line_state(const char* line, unsigned int cursor, unsigned int command_offset, const array<word>& words);
    const char*         get_line() const;
    unsigned int        get_cursor() const;
    unsigned int        get_command_offset() const;
    const array<word>&  get_words() const;
    unsigned int        get_word_count() const;
    bool                get_word(unsigned int index, str_base& out) const;
    str_iter            get_word(unsigned int index) const;
    bool                get_end_word(str_base& out) const;
    str_iter            get_end_word() const;

private:
    const array<word>&  m_words;
    const char*         m_line;
    unsigned int        m_cursor;
    unsigned int        m_command_offset;
};

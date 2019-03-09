// Copyright (c) 2015 Martin Ridgers
// License: http://opensource.org/licenses/MIT

#pragma once

class str_base;

//------------------------------------------------------------------------------
class matches
{
public:
    virtual unsigned int    get_match_count() const = 0;
    virtual const char*     get_match(unsigned int index) const = 0;
    virtual const char*     get_displayable(unsigned int index) const = 0;
    virtual const char*     get_aux(unsigned int index) const = 0;
    virtual char            get_suffix(unsigned int index) const = 0;
    virtual unsigned int    get_cell_count(unsigned int index) const = 0;
    virtual bool            has_aux() const = 0;
    virtual void            get_match_lcd(str_base& out) const = 0;
};



//------------------------------------------------------------------------------
struct match_desc
{
    const char*             match;
    const char*             displayable;
    const char*             aux;
    char                    suffix;
};

//------------------------------------------------------------------------------
class match_builder
{
public:
                            match_builder(matches& matches);
    bool                    add_match(const char* match);
    bool                    add_match(const match_desc& desc);
    void                    set_prefix_included(bool included=true);

private:
    matches&                m_matches;
};

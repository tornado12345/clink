// Copyright (c) 2016 Martin Ridgers
// License: http://opensource.org/licenses/MIT

#include "pch.h"
#include "bind_resolver.h"
#include "binder.h"

#include <core/base.h>
#include <core/str.h>

#include <new>

//------------------------------------------------------------------------------
bind_resolver::binding::binding(bind_resolver* resolver, int node_index)
: m_outer(resolver)
, m_node_index(node_index)
{
    const binder& binder = m_outer->m_binder;
    const auto& node = binder.get_node(m_node_index);

    m_module = node.module;
    m_depth = max<unsigned char>(1, node.depth);
    m_id = node.id;
}

//------------------------------------------------------------------------------
bind_resolver::binding::operator bool () const
{
    return (m_outer != nullptr);
}

//------------------------------------------------------------------------------
editor_module* bind_resolver::binding::get_module() const
{
    if (m_outer == nullptr)
        return nullptr;

    const binder& binder = m_outer->m_binder;
    return binder.get_module(m_module);
}

//------------------------------------------------------------------------------
unsigned char bind_resolver::binding::get_id() const
{
    if (m_outer == nullptr)
        return 0xff;

    return m_id;
}

//------------------------------------------------------------------------------
void bind_resolver::binding::get_chord(str_base& chord) const
{
    if (m_outer == nullptr)
        return;

    chord.clear();
    chord.concat(m_outer->m_keys + m_outer->m_tail, m_depth);
}

//------------------------------------------------------------------------------
void bind_resolver::binding::claim()
{
    if (m_outer != nullptr)
        m_outer->claim(*this);
}



//------------------------------------------------------------------------------
bind_resolver::bind_resolver(const binder& binder)
: m_binder(binder)
{
}

//------------------------------------------------------------------------------
void bind_resolver::set_group(int group)
{
    if (unsigned(group) - 1 >= sizeof_array(m_binder.m_nodes) - 1)
        return;

    if (m_group == group || !m_binder.get_node(group - 1).is_group)
        return;

    m_group = group;
    m_node_index = group;
    m_pending_input = true;
}

//------------------------------------------------------------------------------
int bind_resolver::get_group() const
{
    return m_group;
}

//------------------------------------------------------------------------------
void bind_resolver::reset()
{
    int group = m_group;

    new (this) bind_resolver(m_binder);

    m_group = group;
    m_node_index = m_group;
}

//------------------------------------------------------------------------------
bool bind_resolver::step(unsigned char key)
{
    if (m_key_count >= sizeof_array(m_keys))
    {
        reset();
        return false;
    }

    m_keys[m_key_count] = key;
    ++m_key_count;

    return step_impl(key);
}

//------------------------------------------------------------------------------
bool bind_resolver::step_impl(unsigned char key)
{
    int next = m_binder.find_child(m_node_index, key);
    if (!next)
        return true;

    m_node_index = next;
    return (m_binder.get_node(next).child == 0);
}

//------------------------------------------------------------------------------
bind_resolver::binding bind_resolver::next()
{
    // Push remaining input through the tree.
    if (m_pending_input)
    {
        m_pending_input = false;

        unsigned int keys_remaining = m_key_count - m_tail;
        if (!keys_remaining || keys_remaining >= sizeof_array(m_keys))
        {
            reset();
            return binding();
        }

        for (int i = m_tail, n = m_key_count; i < n; ++i)
            if (step_impl(m_keys[i]))
                break;
    }

    // Go along this depth's nodes looking for valid binds.
    while (m_node_index)
    {
        const binder::node& node = m_binder.get_node(m_node_index);

        // Move iteration along to the next node.
        int node_index = m_node_index;
        m_node_index = node.next;

        // Check to see if where we're currently at a node in the tree that is
        // a valid bind (at the point of call).
        int key_index = m_tail + node.depth - 1;
        if (node.bound && (!node.key || node.key == m_keys[key_index]))
            return binding(this, node_index);
    }

    // We can't get any further traversing the tree with the input provided.
    reset();
    return binding();
}

//------------------------------------------------------------------------------
void bind_resolver::claim(binding& binding)
{
    m_tail += binding.m_depth;
    m_node_index = m_group;
    m_pending_input = true;
}

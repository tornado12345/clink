// Copyright (c) 2012 Martin Ridgers
// License: http://opensource.org/licenses/MIT

#include "pch.h"

#include <Windows.h>

#define TERMCAP_DEBUG 0
#if TERMCAP_DEBUG
#   include <core/base.h>
#   include <stdio.h>
#endif

#define AS_INT(x)       (*(int*)(x))
#define MAKE_CAP(x)     ((AS_INT(x) >> 8)|(AS_INT(x) << 8) & 0xffff)
#define CAP(a, b)       ((b) | ((a) << 8))

/*
    "@7" End key
    "ce" String to clear from the cursor to the end of the line.
    "cl" String to clear the entire screen and put cursor at upper left corner.
    "cr" String to move cursor sideways to left margin.
    "DC" String to delete n characters starting at the cursor.
    "dc" String to delete one character position at the cursor.
    "ei" String to leave insert mode.
    "IC" String to insert n character positions at the cursor.
    "ic" String to insert one character position at the cursor.
    "im" String to enter insert mode.
    "kD" String of input sent by the "delete character" key.
    "kd" String of input sent by typing the down-arrow key.
    "ke" String to make the function keys work locally.
    "kH" String of input sent by the "home down" key.
    "kh" String of input sent by typing the "home-position" key.
    "kI" String of input sent by the "insert character" or "enter insert mode" key.
    "kl" String of input sent by typing the left-arrow key.
    "kr" String of input sent by typing the right-arrow key.
    "ks" String to make the function keys transmit.
    "ku" String of input sent by typing the up-arrow key.
    "le" String to move the cursor left one column.
    "mm" String to enable the functioning of the Meta key.
    "mo" String to disable the functioning of the Meta key.
    "nd" String to move the cursor right one column.
    "pc" String containing character for padding.
    "up" String to move the cursor vertically up one line.
    "vb" String to make the screen flash.
    "ve" String to return the cursor to normal.
    "vs" String to enhance the cursor.
    "co" Number: width of the screen.
    "li" Number: height of the screen.
    "am" Flag: output to last column wraps cursor to next line.
    "km" Flag: the terminal has a Meta key.
    "xn" Flag: cursor wraps in a strange way.
*/

#if defined(__cplusplus)
extern "C" {
#endif

//------------------------------------------------------------------------------
extern int      _rl_last_v_pos;
static int      g_default_cursor_size   = -1;
static int      g_enhanced_cursor       = 0;

//------------------------------------------------------------------------------
struct tgoto_data
{
    short   base;
    int     x;
    int     y;
};
typedef struct tgoto_data tgoto_data_t;

//------------------------------------------------------------------------------
int clamp(int v, int mi, int mx)
{
    v = (v > mi) ? v : mi;
    v = (v < mx) ? v : mx;
    return v;
}

//------------------------------------------------------------------------------
void move_cursor(int dx, int dy)
{
    CONSOLE_SCREEN_BUFFER_INFO i;
    COORD o;

    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &i);
    o.X = clamp(i.dwCursorPosition.X + dx, 0, i.dwSize.X - 1);
    o.Y = clamp(i.dwCursorPosition.Y + dy, 0, i.dwSize.Y - 1);
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), o);
}

//------------------------------------------------------------------------------
static void set_cursor(int x, int y)
{
    CONSOLE_SCREEN_BUFFER_INFO i;
    COORD o;
    HANDLE handle;

    handle = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(handle, &i);

    if (x < 0)  x = i.dwCursorPosition.X;
    if (y < 0)  y = i.dwCursorPosition.Y;

    o.X = clamp(x, 0, i.dwSize.X - 1);
    o.Y = clamp(y, 0, i.dwSize.Y - 1);
    SetConsoleCursorPosition(handle, o);
}

//------------------------------------------------------------------------------
static void delete_chars(int count)
{
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    SMALL_RECT rect;
    CHAR_INFO fill;
    int chars_moved;

    if (count < 0)
        return;

    GetConsoleScreenBufferInfo(handle, &csbi);

    rect.Left = csbi.dwCursorPosition.X + count;
    rect.Right = csbi.dwSize.X - 1; // -1 as dwSize.X isn't a visible column.
    rect.Top = rect.Bottom = csbi.dwCursorPosition.Y;

    fill.Char.AsciiChar = ' ';
    fill.Attributes = csbi.wAttributes;

    ScrollConsoleScreenBuffer(handle, &rect, NULL, csbi.dwCursorPosition, &fill);

    chars_moved = rect.Right - rect.Left + 1;
    if (chars_moved < count)
    {
        DWORD written;
        COORD xy;

        xy = csbi.dwCursorPosition;
        xy.X += chars_moved;

        count -= chars_moved;
        FillConsoleOutputCharacterW(handle, ' ', count, xy, &written);
        FillConsoleOutputAttribute(handle, csbi.wAttributes, count, xy, &written);
    }
}

//------------------------------------------------------------------------------
static void insert_chars(int count)
{
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    SMALL_RECT rect;
    CHAR_INFO fill;

    if (count < 0)
    {
        return;
    }

    GetConsoleScreenBufferInfo(handle, &csbi);

    rect.Left = csbi.dwCursorPosition.X;
    rect.Right = csbi.dwSize.X;
    rect.Top = rect.Bottom = csbi.dwCursorPosition.Y;

    fill.Char.AsciiChar = ' ';
    fill.Attributes = csbi.wAttributes;

    csbi.dwCursorPosition.X += count;

    ScrollConsoleScreenBuffer(
        handle,
        &rect,
        NULL,
        csbi.dwCursorPosition,
        &fill
    );
}

//------------------------------------------------------------------------------
void clear_to_eol()
{
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    GetConsoleScreenBufferInfo(handle, &csbi);
    insert_chars(csbi.dwSize.X - csbi.dwCursorPosition.X);
}

//------------------------------------------------------------------------------
void clear_screen()
{
    // Moves the visible window up so the cursor's at the top. If there's not
    // enough buffer available then the it's scroll up enough to create space.

    HANDLE handle;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    int dy;
    int window_rows;
    SMALL_RECT window;
    COORD cursor_pos;

    handle = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(handle, &csbi);
    window = csbi.srWindow;

    // Adjust for Readline's offset.
    csbi.dwCursorPosition.Y -= _rl_last_v_pos;

    // Is there not enough buffer space to put the cursor at the top?
    dy = csbi.dwCursorPosition.Y - window.Top;
    window_rows = window.Bottom - window.Top;
    if (window_rows > csbi.dwSize.Y - csbi.dwCursorPosition.Y)
    {
        COORD dest_origin;
        CHAR_INFO fill;

        // Set up the rectangle to be scrolled upwards.
        window.Left = 0;
        window.Top = 0;
        window.Right = csbi.dwSize.X;
        window.Bottom = csbi.dwCursorPosition.Y;

        // Coordinates of where to move the rectangle to.
        dest_origin.X = 0;
        dest_origin.Y = -dy;

        // How the new space should be filled.
        fill.Char.UnicodeChar = L' ';
        fill.Attributes = csbi.wAttributes;

        ScrollConsoleScreenBuffer(handle, &window, NULL, dest_origin, &fill);

        window.Top = csbi.dwCursorPosition.Y - dy;
    }
    else if (dy != 0)
    {
        // Move the visible window.
        SMALL_RECT delta_window = { 0, short(dy), 0, short(dy) };
        SetConsoleWindowInfo(handle, FALSE, &delta_window);

        window.Top += dy;
        window.Bottom += dy;
    }

    // Move the cursor to the top of the visible window.
    cursor_pos.X = 0;
    cursor_pos.Y = window.Top;
    SetConsoleCursorPosition(handle, cursor_pos);
}

//------------------------------------------------------------------------------
static void get_screen_size(int* width, int* height)
{
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle != INVALID_HANDLE_VALUE)
    {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(handle, &csbi))
        {
            *width = csbi.dwSize.X;
            *height = (csbi.srWindow.Bottom - csbi.srWindow.Top) + 1;
            return;
        }
    }

    *width = 80;
    *height = 25;
}

//------------------------------------------------------------------------------
static void cursor_style(int style)
{
    CONSOLE_CURSOR_INFO ci;
    HANDLE handle;

    handle = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleCursorInfo(handle, &ci);

    // Assume first encounter of cursor size is the default size.
    if (g_default_cursor_size < 0)
    {
        g_default_cursor_size = ci.dwSize;
    }

    if (g_default_cursor_size > 75)
    {
        style = !style;
    }

    ci.dwSize = style ? 100 : g_default_cursor_size;
    SetConsoleCursorInfo(handle, &ci);
}

//------------------------------------------------------------------------------
static void visible_bell()
{
    cursor_style(!g_enhanced_cursor);
    move_cursor(0, 0);
    Sleep(20);
    cursor_style(g_enhanced_cursor);
}

//------------------------------------------------------------------------------
static void termcap_debug(const char* str)
{
#if TERMCAP_DEBUG
    static struct {
        char buffer[64];
        int index;
        int count;
        int param;
    } buf[16];
    static unsigned int index = 0;

    unsigned int i;
    HANDLE handle;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    COORD cpos;

    handle = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(handle, &csbi);

    i = (index - 1) % sizeof_array(buf);
    if (!strcmp(buf[i].buffer, str) && buf[i].index == index - 1)
    {
        ++buf[i].count;
    }
    else
    {
        i = index % sizeof_array(buf);
        strcpy(buf[i].buffer, str);
        buf[i].index = index;
        buf[i].count = 1;
        buf[i].param = 0;
        ++index;

        if ((str[0] == 'D' || str[0] == 'I') && str[1] == 'C')
        {
            const tgoto_data_t* data = (const tgoto_data_t*)str;
            buf[i].param = data->x << 16 | (data->y & 0xffff);
        }
    }

    cpos.X = csbi.srWindow.Right - 32;
    cpos.Y = csbi.srWindow.Bottom - 2;
    for (i = 0; i < sizeof_array(buf); ++i)
    {
        int j;

        SetConsoleCursorPosition(handle, cpos);
        cpos.Y -= 1;

        j = (index + i) % sizeof_array(buf);
        printf("%02d (%03d) %s", index + i, buf[j].count, buf[j].buffer);

        if (buf[j].param != 0)
        {
            printf(" %08x", buf[j].param);
        }
        else
        {
            printf(" % 8s", "");
        }
    }

    SetConsoleCursorPosition(handle, csbi.dwCursorPosition);
#endif
}

//------------------------------------------------------------------------------
int tputs(const char* str, int count, int (*putc_func)(int))
{
    unsigned short cap = MAKE_CAP(str);

    if (str == NULL || *str == '\0')
    {
        return 0;
    }

    termcap_debug(str);

    switch (cap)
    {
    case CAP('c', 'r'): set_cursor(0, -1);   return 0;  // cr ^M
    case CAP('u', 'p'): move_cursor(0, -1);  return 0;  // up \E[A
    case CAP('l', 'e'): move_cursor(-1, 0);  return 0;  // le ^H
    case CAP('n', 'd'): move_cursor(1, 0);   return 0;  // nd \E[C
    case CAP('c', 'e'): clear_to_eol();      return 0;  // ce \E[K
    case CAP('c', 'l'): clear_screen();      return 0;  // cl \E[H\E[2J
    case CAP('v', 'b'): visible_bell();      return 0;  // vb \E[?5h\E[?5l

    case CAP('v', 'e'): cursor_style(0);                // ve \E[?12l\E[?25h
                        g_enhanced_cursor = 0;
                        return 0;

    case CAP('v', 's'): cursor_style(1);                // vs \E[?12;25h
                        g_enhanced_cursor = 1;
                        return 0;

    case CAP('I', 'C'):                                 // IC \E[%d@
    case CAP('D', 'C'):                                 // DC \E[%dP
        {
            const tgoto_data_t* data = (const tgoto_data_t*)str;
            if (cap == CAP('D', 'C'))
            {
                delete_chars(data->x);
            }
            else
            {
                insert_chars(data->y);
            }
        }
        return 0;

    case CAP('i', 'c'):  return 0;
    case CAP('d', 'c'):  delete_chars(1); return 0;     // dc \E[P
    }

    // Default to simply printing the string.
    while (*str)
    {
        putc_func(*str++);
    }

    return 0;
}

//------------------------------------------------------------------------------
int tgetent(char* bp, const char* name)
{
    *bp = '\0';
    return 1;
}

//------------------------------------------------------------------------------
int tgetnum(char* cap_name)
{
    int width, height;
    unsigned short cap = MAKE_CAP(cap_name);

    get_screen_size(&width, &height);

    switch (cap)
    {
    // Readline will take care of wrapping the cursor at the right-hand edge
    // of the terminal (the Windows console has been told not to do it). This
    // is done to support thirdparty terminals that support resizing. Readline
    // however will will subtract one from the width when it is to handle
    // wrapping preventing use of the buffer's full width. Hence the '+1' here.
    case CAP('c', 'o'): return width;
    case CAP('l', 'i'): return height;
    }

    return 0;
}

//------------------------------------------------------------------------------
int tgetflag(char* cap_name)
{
    unsigned short cap = MAKE_CAP(cap_name);

    switch (cap)
    {
    case CAP('a', 'm'):  return 1;
    case CAP('k', 'm'):  return 1;
    case CAP('x', 'n'):  return 1;
    }

    return 0;
}

//------------------------------------------------------------------------------
char* tgetstr(char* cap_name, char** out)
{
    size_t i;
    char* ret;
    const char* str;
    unsigned short cap = MAKE_CAP(cap_name);

    str = cap_name;
    switch (cap)
    {
    // Insert and delete N and single characters.
    case CAP('D', 'C'): str = "DC";  break;
    case CAP('d', 'c'): str = "dc";  break;

    case CAP('I', 'C'): str = "IC";  break;
    case CAP('i', 'c'): str = "ic";  break;

    // Clear to EOL and the screen.
    case CAP('c', 'e'): str = "ce";  break;
    case CAP('c', 'l'): str = "cl";  break;

    // Enter and leave insert mode. Used in insert_some_chars(), called when
    // drawing the line. Windows' console is always in an "insert" mode.
    case CAP('i', 'm'): str = "";    break;
    case CAP('e', 'i'): str = "";    break;

    // Echo/Send movement keys modes. Called in rl_(de)prep_terminal functions
    // which are never called.
    case CAP('k', 'e'): str = "";    break;
    case CAP('k', 's'): str = "";    break;

    // Movement key bindings.
    case CAP('k', 'H'): str = "[kh]";        break; // Home "down"?! Unused.
    case CAP('k', 'h'): str = "\x1b[H";      break; // Home
    case CAP('@', '7'): str = "\x1b[F";      break; // End
    case CAP('k', 'D'): str = "\x1b[3";      break; // Del
    case CAP('k', 'I'): str = "\x1b[2";      break; // Ins
    case CAP('k', 'u'): str = "\x1b[A";      break; // Up
    case CAP('k', 'd'): str = "\x1b[B";      break; // Down
    case CAP('k', 'r'): str = "\x1b[C";      break; // Right
    case CAP('k', 'l'): str = "\x1b[D";      break; // Left

    // Cursor movement.
    case CAP('c', 'r'): str = "cr";  break;
    case CAP('l', 'e'): str = "le";  break;
    case CAP('n', 'd'): str = "nd";  break;
    case CAP('u', 'p'): str = "up";  break;

    // meta-mode on (m) and off (o)
    case CAP('m', 'm'): str = "";    break;
    case CAP('m', 'o'): str = "";    break;

    // Cursor style
    case CAP('v', 'e'): str = "ve";  break;
    case CAP('v', 's'): str = "vs";  break;

    // Misc.
    case CAP('v', 'b'): str = "vb";  break;
    case CAP('p', 'c'): str = "";    break; // The "padding char". A relic left over from
                                            // when terminals took 1.3ms to pad a line
                                            // and 1200baud ruled the roost.
    }

    i = strlen(str) + 1;
    if (out != NULL)
    {
        ret = *out;
        *out += i;
        strcpy(ret, str);
    }
    else
    {
        ret = (char*)malloc(i);
        strcpy(ret, str);
    }

    return ret;
}

//------------------------------------------------------------------------------
char* tgoto(char* base, int x, int y)
{
    static tgoto_data_t data;

    data.base = *(short*)base;
    data.x = x;
    data.y = y;

    return (char*)&data;
}

#if defined(__cplusplus)
} // extern "C"
#endif

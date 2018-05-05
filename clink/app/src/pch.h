// Copyright (c) 2012 Martin Ridgers
// License: http://opensource.org/licenses/MIT

#pragma once

#define NOMINMAX
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN

#include <stdio.h>
#include <conio.h>
#include <io.h>
#include <locale.h>
#include <stdlib.h>
#ifdef __MINGW32__
#   include <stdint.h>
#endif

#include <Windows.h>
#include <Shellapi.h>
#include <Shlobj.h>
#include <TlHelp32.h>
// Some code
#ifndef __MINGW32__
#   ifdef _MSC_VER
#       pragma warning(push)
#       pragma warning(disable : 4091)
#   endif
#   include <DbgHelp.h>
#   ifdef _MSC_VER
#       pragma warning(pop)
#   endif
#endif

#include <core/base.h>

#define CLINK_MODULE    "clink_x" AS_STR(ARCHITECTURE)
#define CLINK_DLL       CLINK_MODULE ".dll"
#define CLINK_EXE       CLINK_MODULE ".exe"

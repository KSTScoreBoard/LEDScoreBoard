/* Target definitions for GCC for Beyond Architecture using ELF
   Copyright (C) 1996, 1997, 2005, 2006, 2008, 2009, 2010, 2011
   Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#undef  STARTFILE_SPEC
#define STARTFILE_SPEC	"crti%O%s crtbegin%O%s crt0%O%s"

#undef  ENDFILE_SPEC
#define ENDFILE_SPEC	"crtend%O%s crtn%O%s"

#undef LINK_GCC_C_SEQUENCE_SPEC
#define LINK_GCC_C_SEQUENCE_SPEC "%L %G %L"

#undef DWARF2_DEBUGGING_INFO
#define DWARF2_DEBUGGING_INFO 1

#undef DWARF2_UNWIND_INFO
#define DWARF2_UNWIND_INFO 1

#undef  PREFERRED_DEBUGGING_TYPE
#define PREFERRED_DEBUGGING_TYPE DWARF2_DEBUG

#undef  PUT_SDB_DEF
#define PUT_SDB_DEF

#undef  USER_LABEL_PREFIX
#define USER_LABEL_PREFIX ""

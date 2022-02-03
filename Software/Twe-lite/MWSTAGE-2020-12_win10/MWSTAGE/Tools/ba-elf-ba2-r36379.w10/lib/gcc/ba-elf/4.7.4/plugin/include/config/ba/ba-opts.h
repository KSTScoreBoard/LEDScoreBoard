/* Definitions for option handling for Beyond Architecture.
   Copyright (C) 1987, 1988, 1992, 1995, 1996, 1999, 2000, 2001, 2002,
   2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013
   Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#ifndef BA_OPTS_H_
#define BA_OPTS_H_

/* An enumeration of all supported target devices.  */
enum target_device
{
#define BA_DEVICE(NAME,ENUM_VALUE,MULTILIB,ARCH,MTUNE,FLAGS) \
  ENUM_VALUE,
#include "ba-devices.def"
#undef BA_DEVICE
  unk_device
};

#endif /* BA_OPTS_H_ */

/* 
   Unix SMB/CIFS implementation.
   Samba system utilities
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Jeremy Allison  1998-2005
   Copyright (C) Timur Bakeyev        2005
   Copyright (C) Bjoern Jacke    2006-2007
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
   
   sys_copyxattr modified from LGPL2.1 libattr copyright
   Copyright (C) 2001-2002 Silicon Graphics, Inc.  All Rights Reserved.
   Copyright (C) 2001 Andreas Gruenbacher.
      
   Samba 3.0.28, modified for netatalk.
   $Id: sys_ea.c,v 1.6 2009-12-04 10:26:10 franklahm Exp $
   
*/

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>

#if HAVE_ATTR_XATTR_H
#include <attr/xattr.h>
#elif HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif



#ifdef HAVE_SYS_EXTATTR_H
#include <sys/extattr.h>
#endif

#include <atalk/adouble.h>
#include <atalk/util.h>
#include <atalk/logger.h>
#include <atalk/ea.h>

#ifndef ENOATTR
#define ENOATTR ENODATA
#endif

/******** Solaris EA helper function prototypes ********/

/**************************************************************************
 Wrappers for extented attribute calls. Based on the Linux package with
 support for IRIX and (Net|Free)BSD also. Expand as other systems have them.
****************************************************************************/
static char attr_name[256 +5] = "user.";

static const char *prefix(const char *uname)
{
	strlcpy(attr_name +5, uname, 256);
	return attr_name;
}

ssize_t sys_getxattr (const char *path, const char *uname, void *value, size_t size)
{
	const char *name = prefix(uname);

	return getxattr(path, name, value, size);
}

ssize_t sys_lgetxattr (const char *path, const char *uname, void *value, size_t size)
{
	const char *name = prefix(uname);

	return lgetxattr(path, name, value, size);
}



static ssize_t remove_user(ssize_t ret, char *list, size_t size)
{
	size_t len;
	char *ptr;
	char *ptr1;
	ssize_t ptrsize;
	
	if (ret <= 0 || size == 0)
		return ret;
	ptrsize = ret;
	ptr = ptr1 = list;
	while (ptrsize > 0) {
		len = strlen(ptr1) +1;
		ptrsize -= len;
		if (strncmp(ptr1, "user.",5)) {
			ptr1 += len;
			continue;
		}
		memmove(ptr, ptr1 +5, len -5);
		ptr += len -5;
		ptr1 += len;
	}
	return ptr -list;
}

ssize_t sys_listxattr (const char *path, char *list, size_t size)
{
	ssize_t ret;

	ret = listxattr(path, list, size);
	return remove_user(ret, list, size);

}

ssize_t sys_llistxattr (const char *path, char *list, size_t size)
{
	ssize_t ret;

	ret = llistxattr(path, list, size);
	return remove_user(ret, list, size);
}

int sys_removexattr (const char *path, const char *uname)
{
	const char *name = prefix(uname);
	return removexattr(path, name);
}

int sys_lremovexattr (const char *path, const char *uname)
{
	const char *name = prefix(uname);
	return lremovexattr(path, name);
}

int sys_setxattr (const char *path, const char *uname, const void *value, size_t size, int flags)
{
	const char *name = prefix(uname);
	return setxattr(path, name, value, size, flags);
}

int sys_lsetxattr (const char *path, const char *uname, const void *value, size_t size, int flags)
{
	const char *name = prefix(uname);
	return lsetxattr(path, name, value, size, flags);
}

/**************************************************************************
 helper functions for Solaris' EA support
****************************************************************************/


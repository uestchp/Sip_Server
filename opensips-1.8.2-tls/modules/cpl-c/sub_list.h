/*
 * $Id: sub_list.h 5901 2009-07-21 07:45:05Z bogdan_iancu $
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of opensips, a free SIP server.
 *
 * opensips is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * opensips is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _CPL_SUB_LIST_H
#define _CPL_SUB_LIST_H

struct node {
	char         *offset;
	char         *name;
	struct node  *next;
};


struct node*  append_to_list(struct node *head, char *offdet, char *name);
char*         search_the_list(struct node *head, char *name);
void          delete_list(struct node *head );

#endif

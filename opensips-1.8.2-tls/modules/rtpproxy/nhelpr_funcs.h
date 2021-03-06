/*
 * $Id: nhelpr_funcs.h 7796 2011-03-09 22:23:25Z osas $
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


#ifndef _NHLPR_FUNCS_H
#define  _NHLPR_FUNCS_H

#include "../../str.h"
#include "../../parser/msg_parser.h"
#include "../../parser/contact/contact.h"

int extract_body(struct sip_msg * , str *);
int check_content_type(struct sip_msg * );
void *ser_memmem(const void *, const void *, size_t, size_t);
int get_callid(struct sip_msg *, str *);
int get_to_tag(struct sip_msg *, str *);
int get_from_tag(struct sip_msg *, str *);
int get_contact_uri(struct sip_msg *, struct sip_uri *,
		contact_t **,struct hdr_field **);

#endif

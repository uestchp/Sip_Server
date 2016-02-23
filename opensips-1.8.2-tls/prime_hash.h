/*
 * $Id: prime_hash.h 5902 2009-07-21 07:50:39Z bogdan_iancu $
 *
 * Copyright (C) 2007 1&1 Internet AG
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
 *
 */

/*!
 * \file
 * \brief
 * Functions for determinung a pseudo random number over a message's
 * header field, based on CRC32 or a prime number algorithm.
 */


#ifndef PRIME_HASH_H
#define PRIME_HASH_H 1

#include "parser/msg_parser.h"


/*!
 * \brief
 * Determines from which part of a message the hash shall be calculated.
 * Possible values are:
 * 
 * - \b shs_call_id     the content of the Call-ID header field
 * - \b shs_from_uri    the entire URI in the From header field
 * - \b shs_from_user   the username part of the URI in the From header field
 * - \b shs_to_uri      the entire URI in the To header field
 * - \b shs_to_user     the username part of the URI in the To header field
 * - \b shs_error       no hash specified
*/
enum hash_source {
	shs_call_id = 1,
	shs_from_uri,
	shs_from_user,
	shs_to_uri,
	shs_to_user,
	shs_error
};

/*! generic interface for hash functions */
typedef int (*hash_func_t)(struct sip_msg * msg,
	enum hash_source source, int denominator);


/*!
 * \brief CRC32 hash function
 * Returns an integer number between 0 and denominator - 1 based on
 * the hash source from the msg. The hash algorith is CRC32.
*/
int hash_func (struct sip_msg * msg,
                         enum hash_source source, int denominator);

/*!
 * \brief prime hash function
 * Returns an integer number between 0 and denominator - 1 based on
 * the hash source from the msg. Use the prime number algorithm.
*/
int prime_hash_func (struct sip_msg * msg,
                               enum hash_source source, int denominator);

#endif

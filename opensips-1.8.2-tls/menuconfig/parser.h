/*
 * $Id: parser.h 8676 2012-01-19 17:05:24Z bogdan_iancu $
 *
 * Copyright (C) 2012 OpenSIPS Solutions
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
 * History:
 * -------
 *  2012-01-19  created (vlad)
 */

#ifndef _parser_h_
#define _parser_h_

#include "menus.h"
#include "cfg.h"

int parse_dep_line(char *line,select_menu *parent);
int parse_include_line(char *line,select_menu *parent);
int parse_defs_line(char *line,select_menu *parent);
int parse_prefix_line(char *line,select_menu *menu);
int parse_defs_m4_line(char *line,select_menu *menu);
int parse_defs_m4(select_menu *curr_menu,cfg_gen_t *curr_cfg);
int parse_make_conf();
#endif

/* 
 * $Id: perlvdb_conv.c 5901 2009-07-21 07:45:05Z bogdan_iancu $
 *
 * Perl virtual database module interface
 *
 * Copyright (C) 2007 Collax GmbH
 *                    (Bastian Friedrich <bastian.friedrich@collax.com>)
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

#include "perlvdb_conv.h"
#include "perlvdb_oohelpers.h"

#include "../../dprint.h"
#include "../../mem/mem.h"

/* Converts a set of pairs to perl SVs.
 * For insert, and update (second half)
 */
AV *pairs2perlarray(db_key_t* keys, db_val_t* vals, int n) {

	AV *array = newAV();
	SV *element;
	int i;

	for (i = 0; i < n; i++) {
		element = pair2perlpair(*(keys + i), vals + i);
		av_push(array, element);
	}

	return array;
}

/* Converts a set of cond's to perl SVs.
 * For delete, update (first half), query
 */
AV *conds2perlarray(db_key_t* keys, db_op_t* ops, db_val_t* vals, int n) {
	AV *array = newAV();
	SV *element = NULL;
	int i = 0;

	for (i = 0; i < n; i++) {
		if (ops) {
			if (ops + i)
				if (*(ops + i))
					element = cond2perlcond(*(keys + i),
							*(ops + i), vals + i);
		} else {
/* OP_EQ is defined in OpenSIPS _and_ perl. Includes collide :( */
#ifdef OP_EQ
			element = cond2perlcond(*(keys + i), OP_EQ, vals + i);
#else
			element = cond2perlcond(*(keys + i), "=", vals + i);
#endif
		}

		av_push(array, element);
	}

	return array;
}


/* Converts a set of key names to a perl array.
 * Needed in query.
 */
AV *keys2perlarray(db_key_t* keys, int n) {
	AV *array = newAV();
	SV *element;
	int i;
	for (i = 0; i < n; i++) {
		element = newSVpv((keys[i])->s, (keys[i])->len); 
		av_push(array, element);
	}

	return array;
}

inline SV *valdata(db_val_t* val) {
	SV *data = &PL_sv_undef;
	const char* stringval;

	switch(VAL_TYPE(val)) {
		case DB_INT:
			data = newSViv(VAL_INT(val));
			break;

		case DB_DOUBLE:
			data = newSVnv(VAL_DOUBLE(val));
			break;

		case DB_STRING:
			stringval = VAL_STRING(val);
			if (strlen(stringval) > 0)
				data = newSVpv(stringval, strlen(stringval));
			else
				data = &PL_sv_undef;
			break;

		case DB_STR:
			if (VAL_STR(val).len > 0)
				data = newSVpv(VAL_STR(val).s, VAL_STR(val).len);
			else
				data = &PL_sv_undef;
			break;

		case DB_DATETIME:
			data = newSViv((unsigned int)VAL_TIME(val));
			break;

		case DB_BLOB:
			if (VAL_BLOB(val).len > 0)
				data = newSVpv(VAL_BLOB(val).s,
						VAL_BLOB(val).len);
			else
				data = &PL_sv_undef;
			break;

		case DB_BITMAP:
			data = newSViv(VAL_BITMAP(val));
			break;
	}

	return data;
}

SV *val2perlval(db_val_t* val) {
	SV* retval;
	SV *class;

	SV *p_data;
	SV *p_type;

	class = newSVpv(PERL_CLASS_VALUE, 0);

	p_data = valdata(val);
	p_type = newSViv(val->type);
	
	retval = perlvdb_perlmethod(class, PERL_CONSTRUCTOR_NAME,
			p_type, p_data, NULL, NULL);

	return retval;

}

SV *pair2perlpair(db_key_t key, db_val_t* val) {
	SV* retval;
	SV *class;

	SV *p_key;
	SV *p_type;
	SV *p_data;

	class = newSVpv(PERL_CLASS_PAIR, 0);

	p_key  = newSVpv(key->s, key->len);
	p_type = newSViv(val->type);
	p_data = valdata(val);
	
	retval = perlvdb_perlmethod(class, PERL_CONSTRUCTOR_NAME,
			p_key, p_type, p_data, NULL);

	SvREFCNT_dec(class);

	return retval;
	
}

SV *cond2perlcond(db_key_t key, db_op_t op, db_val_t* val) {
	SV* retval;
	SV *class;
	
	SV *p_key;
	SV *p_op;
	SV *p_type;
	SV *p_data;

	class = newSVpv(PERL_CLASS_REQCOND, 0);

	p_key  = newSVpv(key->s, key->len);
	p_op   = newSVpv(op, strlen(op));
	p_type = newSViv(val->type);
	p_data = valdata(val);
	
	retval = perlvdb_perlmethod(class, PERL_CONSTRUCTOR_NAME,
			p_key, p_op, p_type, p_data);

	return retval;
}



int perlresult2dbres(SV *perlres, db_res_t **r) {

	SV *colarrayref = NULL;
	AV *colarray = NULL;
	SV *acol = NULL;
	int colcount = 0;


	SV *rowarrayref = NULL;
	AV *rowarray = NULL;
	int rowcount = 0;

	SV *arowref = NULL;
	AV *arow = NULL;
	int arowlen = 0;

	SV *aelement = NULL;
	SV *atypesv = 0;
	int atype = 0;
	SV *aval = NULL;

	char *charbuf;
	char *currentstring;

	int i, j;
	
	int retval = 0;
	STRLEN len;

	SV *d1; /* helper variables */

	/*db_val_t cur_val;*/ /* Abbreviation in "switch" below. The currently
			     modified db result value. */

	if (!(SvROK(perlres) &&
		(sv_derived_from(perlres, "OpenSIPS::VDB::Result")))) {
		goto error;
	}
	/* Memory allocation for C side result structure */
	*r = (db_res_t *)pkg_malloc(sizeof(db_res_t));
	if (!(*r)) {
		LM_ERR("no pkg memory left\n");
		return -1;
	}
	memset(*r, 0, sizeof(db_res_t));
	
	/* Fetch column definitions */
	colarrayref = perlvdb_perlmethod(perlres, PERL_VDB_COLDEFSMETHOD,
			NULL, NULL, NULL, NULL);
	if (!(SvROK(colarrayref))) goto error;
	colarray = (AV *)SvRV(colarrayref);
	if (!(SvTYPE(colarray) == SVt_PVAV)) goto error;

	colcount = av_len(colarray) + 1;

	/* Allocate col def memory */
	(*r)->col.n = colcount;
	(*r)->col.types = (db_type_t*)pkg_malloc(colcount*sizeof(db_type_t));
	(*r)->col.names = (db_key_t*)pkg_malloc(colcount*sizeof(db_key_t));
	
	 /* reverse direction, as elements are removed by "SvREFCNT_dec" */
	for (i = colcount-1; i >= 0; i--) {
		acol = *av_fetch(colarray, i, 0);
		d1 = perlvdb_perlmethod(acol, PERL_VDB_TYPEMETHOD,
				NULL, NULL, NULL, NULL);
		if (!SvIOK(d1)) goto error;
		(*r)->col.types[i] = SvIV(d1);

		SvREFCNT_dec(d1);
		
		d1 = perlvdb_perlmethod(acol, PERL_VDB_NAMEMETHOD,
				NULL, NULL, NULL, NULL);
		if (!SvPOK(d1)) goto error;
		currentstring = SvPV(d1, len);
		charbuf = pkg_malloc(len+1);
		strncpy(charbuf, currentstring, len+1);
		(*r)->col.names[i]->s = charbuf;
		(*r)->col.names[i]->len = strlen(charbuf);

		SvREFCNT_dec(d1);

	}

	rowarrayref = perlvdb_perlmethod(perlres, PERL_VDB_ROWSMETHOD,
			NULL, NULL, NULL, NULL);
	if (!(SvROK(rowarrayref))) { /* Empty result set */
		(*r)->n = 0;
		(*r)->res_rows = 0;
		(*r)->last_row = 0;
		goto end;
	}

	rowarray = (AV *)SvRV(rowarrayref);
	if (!(SvTYPE(rowarray) == SVt_PVAV)) goto error;

	rowcount = av_len(rowarray) + 1;

	(*r)->n = rowcount;
	(*r)->res_rows = rowcount;
	(*r)->last_row = rowcount;
	
	(*r)->rows = (db_row_t *)pkg_malloc(rowcount*sizeof(db_row_t));

	for (i = 0; i < rowcount; i++) {
		arowref = *av_fetch(rowarray, 0, 0);
		if (!SvROK(arowref)) goto error;
		arow = (AV *)SvRV(arowref);
		if (!(SvTYPE(colarray) == SVt_PVAV)) goto error;
		arowlen = av_len(arow) + 1;

		(*r)->rows[i].n = arowlen;
		(*r)->rows[i].values =
			(db_val_t *)pkg_malloc(arowlen*sizeof(db_val_t));


		for (j = 0; j < arowlen; j++) {
			aelement = *av_fetch(arow, j, 0);
#define cur_val (((*r)->rows)[i].values)[j]
			/*cur_val = (((*r)->rows)[i].values)[j];*/
			  /* cur_val is just an "abbreviation" */
			if (!(sv_isobject(aelement) && 
				sv_derived_from(aelement, PERL_CLASS_VALUE))) {
				cur_val.nul = 1;
				continue;
			}
			atype = SvIV(atypesv = perlvdb_perlmethod(aelement,
						PERL_VDB_TYPEMETHOD,
						NULL, NULL, NULL, NULL));
			aval = perlvdb_perlmethod(aelement, PERL_VDB_DATAMETHOD,
					NULL, NULL, NULL, NULL);

			(*r)->rows[i].values[j].type = atype;
			if (!SvOK(aval)) {
				cur_val.nul = 1;
			} else {
				switch (atype) {
					case DB_INT:
						cur_val.val.int_val = 
							SvIV(aval);
						cur_val.nul = 0;
						break;
					case DB_DOUBLE:
						cur_val.val.double_val = 
							SvNV(aval);
						cur_val.nul = 0;
						break;
					case DB_STRING:
					case DB_STR:
				/* We dont support DB_STR for now.
				 * Set DB_STRING instead */
						cur_val.type = DB_STRING;
						currentstring = SvPV(aval, len);
						charbuf = pkg_malloc(len+1);
						strncpy(charbuf, currentstring,
								len+1);
						cur_val.val.string_val =
							charbuf;
						cur_val.nul = 0;
						break;
					case DB_DATETIME:
						cur_val.val.time_val =
							(time_t)SvIV(aval);
						cur_val.nul = 0;
						break;
					case DB_BLOB:
						currentstring = SvPV(aval, len);
						charbuf = pkg_malloc(len+1);
						strncpy(charbuf, currentstring,
								len+1);
						cur_val.val.blob_val.s =
							charbuf;
						cur_val.val.blob_val.len = len;
						cur_val.nul = 0;
						break;
					case DB_BITMAP:
						cur_val.val.bitmap_val =
							SvIV(aval);
						cur_val.nul = 0;
						break;
					default:
						LM_CRIT("cannot handle this data type.\n");
						return -1;
						break;
				}
			}
			SvREFCNT_dec(atypesv);
			SvREFCNT_dec(aval);
		}
	}

end:
	av_undef(colarray);
	av_undef(rowarray);
	return retval;
error:
	LM_CRIT("broken result set. Exiting, leaving OpenSIPS in unknown state.\n");
	return -1;
}




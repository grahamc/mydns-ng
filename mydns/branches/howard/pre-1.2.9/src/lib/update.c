/**************************************************************************************************
	$Id: update.c,v 1.10 2005/12/18 19:16:41 bboy Exp $
	update.c: Code to implement RFC 2136 (DNS UPDATE)

	Copyright (C) 2005  Don Moore <bboy@bboy.net>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at Your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**************************************************************************************************/

#include "named.h"

/* Make this nonzero to enable debugging for this source file */
#define	DEBUG_LIB_UPDATE	1


/**************************************************************************************************
	TEXT_RETRIEVE
	Retrieve a name from the source without end-dot encoding.
**************************************************************************************************/
static char *
text_retrieve(unsigned char **srcp, unsigned char *end, size_t *datalen, int one_word_only) {
  int		n = 0;
  int		x = 0;							/* Offset in 'data' */
  char 		*data = NULL;
  unsigned char	*src = *srcp;

  *datalen = 0;

  for (n = 0; src < end; ) {
    int len = *src++;

    if (n >= *datalen) {
      *datalen += len;
      data = REALLOCATE(data, (*datalen)+1, char[]);
    }
    if (n)
      data[n++] = '\0';
    for (x = 0; x < len && src < end; x++)
      data[n++] = *src++;
    if (one_word_only)
      break;
  }
  data[n] = '\0';
  *srcp = src;
  return data;
}
/*--- text_retrieve() ---------------------------------------------------------------------------*/

taskexec_t __mydns_update_get_rr_data_unknown_type(TASK *t,
						   UQRR *rr,
						   char **data,
						   size_t *datalen,
						   char **edata,
						   size_t *edatalen,
						   uint32_t *aux,
						   dns_qtype_map *map) {
  *datalen = ASPRINTF(data, "Unknown type %s", map->rr_type_name);
  return (TASK_FAILED);
}

taskexec_t __mydns_update_get_rr_data_a(TASK *t,
					UQRR *rr,
					char **data,
					size_t *datalen,
					char **edata,
					size_t *edatalen,
					uint32_t *aux,
					dns_qtype_map *map) {
  if (UQRR_DATA_LENGTH(rr) != 4)
    return (TASK_ABANDONED);
  *datalen = ASPRINTF(data, "%d.%d.%d.%d", UQRR_DATA_VALUE(rr)[0], UQRR_DATA_VALUE(rr)[1],
		      UQRR_DATA_VALUE(rr)[2], UQRR_DATA_VALUE(rr)[3]);
  return TASK_EXECUTED;
}

taskexec_t __mydns_update_get_rr_data_ns(TASK *t,
					 UQRR *rr,
					 char **data,
					 size_t *datalen,
					 char **edata,
					 size_t *edatalen,
					 uint32_t *aux,
					 dns_qtype_map *map) {
  unsigned char	*src = (unsigned char*)UQRR_DATA_VALUE(rr);
  task_error_t	errcode = 0;

  if (!(*data = name_unencode2(t->query, t->len, (char**)&src, &errcode)))
    return formerr(t, DNS_RCODE_FORMERR, errcode, NULL);
  *datalen = strlen(*data);
  return TASK_EXECUTED;
}

taskexec_t __mydns_update_get_rr_data_cname(TASK *t,
					    UQRR *rr,
					    char **data,
					    size_t *datalen,
					    char **edata,
					    size_t *edatalen,
					    uint32_t *aux,
					    dns_qtype_map *map) {
  unsigned char	*src = (unsigned char*)UQRR_DATA_VALUE(rr);
  task_error_t	errcode = 0;

  if (!(*data = name_unencode2(t->query, t->len, (char**)&src, &errcode)))
    return formerr(t, DNS_RCODE_FORMERR, errcode, NULL);
  *datalen = strlen(*data);
  return TASK_EXECUTED;
}

taskexec_t __mydns_update_get_rr_data_ptr(TASK *t,
					  UQRR *rr,
					  char **data,
					  size_t *datalen,
					  char **edata,
					  size_t *edatalen,
					  uint32_t *aux,
					  dns_qtype_map *map) {
  unsigned char	*src = (unsigned char*)UQRR_DATA_VALUE(rr);
  task_error_t	errcode = 0;

  if (!(*data = name_unencode2(t->query, t->len, (char**)&src, &errcode)))
    return formerr(t, DNS_RCODE_FORMERR, errcode, NULL);
  *datalen = strlen(*data);
  return TASK_EXECUTED;
}

taskexec_t __mydns_update_get_rr_data_hinfo(TASK *t,
					    UQRR *rr,
					    char **data,
					    size_t *datalen,
					    char **edata,
					    size_t *edatalen,
					    uint32_t *aux,
					    dns_qtype_map *map) {
  char	*data1 = NULL;
  char	*data2 = NULL;
  char	*c = NULL;
  size_t	datalen1= 0;
  size_t	datalen2 = 0;
  int	data1sp = 0;
  int	data2sp = 0;
  unsigned char	*src = (unsigned char*)UQRR_DATA_VALUE(rr);
  unsigned char	*end = (unsigned char*)(UQRR_DATA_VALUE(rr) + UQRR_DATA_LENGTH(rr));

  data1 = text_retrieve(&src, end, &datalen1, 1);
  data2 = text_retrieve(&src, end, &datalen2, 1);

  /* See if either value contains spaces, so we can enclose it with quotes */
  for (c = data1, data1sp = 0; *c && !data1sp; c++)
    if (isspace(*c)) data1sp = 1;
  for (c = data2, data2sp = 0; *c && !data2sp; c++)
    if (isspace(*c)) data2sp = 1;

  *datalen = ASPRINTF(data, "%s%s%s %s%s%s",
		      data1sp ? "\"" : "", data1, data1sp ? "\"" : "",
		      data2sp ? "\"" : "", data2, data2sp ? "\"" : "");
  RELEASE(data1);
  RELEASE(data2);
  return TASK_EXECUTED;
}

taskexec_t __mydns_update_get_rr_data_mx(TASK *t,
					 UQRR *rr,
					 char **data,
					 size_t *datalen,
					 char **edata,
					 size_t *edatalen,
					 uint32_t *aux,
					 dns_qtype_map *map) {
  unsigned char	*src = (unsigned char*)UQRR_DATA_VALUE(rr);
  task_error_t	errcode = 0;

  DNS_GET16(*aux, src);
  if (!(*data = name_unencode2(t->query, t->len, (char**)&src, &errcode)))
    return formerr(t, DNS_RCODE_FORMERR, errcode, NULL);
  *datalen = strlen(*data);
  return TASK_EXECUTED;
}

taskexec_t __mydns_update_get_rr_data_txt(TASK *t,
					  UQRR *rr,
					  char **data,
					  size_t *datalen,
					  char **edata,
					  size_t *edatalen,
					  uint32_t *aux,
					  dns_qtype_map *map) {
  unsigned char	*src = (unsigned char*)UQRR_DATA_VALUE(rr);
  unsigned char	*end = (unsigned char*)(UQRR_DATA_VALUE(rr) + UQRR_DATA_LENGTH(rr));

  *data = text_retrieve(&src, end, datalen, 0);
  if (*datalen > DNS_MAXTXTLEN) {
    RELEASE(*data);
    return dnserror(t, DNS_RCODE_FORMERR, ERR_INVALID_DATA);
  }
  return TASK_EXECUTED;
}

taskexec_t __mydns_update_get_rr_data_rp(TASK *t,
					 UQRR *rr,
					 char **data,
					 size_t *datalen,
					 char **edata,
					 size_t *edatalen,
					 uint32_t *aux,
					 dns_qtype_map *map) {
  unsigned char	*src = (unsigned char*)UQRR_DATA_VALUE(rr);
  task_error_t	errcode = 0;
  char *data1, *data2;

  if (!(data1 = name_unencode2(t->query, t->len, (char**)&src, &errcode)))
    return formerr(t, DNS_RCODE_FORMERR, errcode, NULL);
  if (!(data2 = name_unencode2(t->query, t->len, (char**)&src, &errcode))) {
    RELEASE(data1);
    return formerr(t, DNS_RCODE_FORMERR, errcode, NULL);
  }

  *datalen = ASPRINTF(data, "%s %s", data1, data2);
  RELEASE(data1);
  RELEASE(data2);
  return TASK_EXECUTED;
}

taskexec_t __mydns_update_get_rr_data_aaaa(TASK *t,
					   UQRR *rr,
					   char **data,
					   size_t *datalen,
					   char **edata,
					   size_t *edatalen,
					   uint32_t *aux,
					   dns_qtype_map *map) {
  if (UQRR_DATA_LENGTH(rr) != 16)
    return (TASK_ABANDONED);
  /* Need to allocate a dynamic buffer */
  if (!(*data = (char*)ipaddr(AF_INET6, UQRR_DATA_VALUE(rr)))) {
    *datalen = 0;
    return dnserror(t, DNS_RCODE_FORMERR, ERR_INVALID_ADDRESS);
  }
  *datalen = strlen(*data);
  *data = STRDUP(*data);
  return TASK_EXECUTED;
}

taskexec_t __mydns_update_get_rr_data_srv(TASK *t,
					  UQRR *rr,
					  char **data,
					  size_t *datalen,
					  char **edata,
					  size_t *edatalen,
					  uint32_t *aux,
					  dns_qtype_map *map) {
  unsigned char	*src = (unsigned char*)UQRR_DATA_VALUE(rr);
  task_error_t	errcode = 0;
  uint16_t weight, port;
  char *data1;

  DNS_GET16(*aux, src);
  DNS_GET16(weight, src);
  DNS_GET16(port, src);

  if (!(data1 = name_unencode2(t->query, t->len, (char**)&src, &errcode)))
    return formerr(t, DNS_RCODE_FORMERR, errcode, NULL);

  *datalen = ASPRINTF(data, "%u %u %s", weight, port, data1);
  RELEASE(data1);
  return TASK_EXECUTED;
}


/**************************************************************************************************
	Copyright (C) 2009-  Howard Wilkinson <howard@cohtech.com>

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

#ifndef _MYDNS_LIB_UPDATE_H
#define _MYDNS_LIB_UPDATE_H

extern taskexec_t __mydns_update_get_rr_data_unknown_type(TASK *t,
							  UQRR *rr,
							  char **data,
							  size_t *datalen,
							  char **edata,
							  size_t *edatalen,
							  uint32_t *aux,
							  dns_qtype_map *map);
extern taskexec_t __mydns_update_get_rr_data_a(TASK *t,
					       UQRR *rr,
					       char **data,
					       size_t *datalen,
					       char **edata,
					       size_t *edatalen,
					       uint32_t *aux,
					       dns_qtype_map *map);
extern taskexec_t __mydns_update_get_rr_data_aaaa(TASK *t,
						  UQRR *rr,
						  char **data,
						  size_t *datalen,
						  char **edata,
						  size_t *edatalen,
						  uint32_t *aux,
						  dns_qtype_map *map);
extern taskexec_t __mydns_update_get_rr_data_cname(TASK *t,
						   UQRR *rr,
						   char **data,
						   size_t *datalen,
						   char **edata,
						   size_t *edatalen,
						   uint32_t *aux,
						   dns_qtype_map *map);
extern taskexec_t __mydns_update_get_rr_data_hinfo(TASK *t,
						   UQRR *rr,
						   char **data,
						   size_t *datalen,
						   char **edata,
						   size_t *edatalen,
						   uint32_t *aux,
						   dns_qtype_map *map);
extern taskexec_t __mydns_update_get_rr_data_mx(TASK *t,
						UQRR *rr,
						char **data,
						size_t *datalen,
						char **edata,
						size_t *edatalen,
						uint32_t *aux,
						dns_qtype_map *map);
extern taskexec_t __mydns_update_get_rr_data_ns(TASK *t,
						UQRR *rr,
						char **data,
						size_t *datalen,
						char **edata,
						size_t *edatalen,
						uint32_t *aux,
						dns_qtype_map *map);
extern taskexec_t __mydns_update_get_rr_data_ptr(TASK *t,
						 UQRR *rr,
						 char **data,
						 size_t *datalen,
						 char **edata,
						 size_t *edatalen,
						 uint32_t *aux,
						 dns_qtype_map *map);
extern taskexec_t __mydns_update_get_rr_data_rp(TASK *t,
						UQRR *rr,
						char **data,
						size_t *datalen,
						char **edata,
						size_t *edatalen,
						uint32_t *aux,
						dns_qtype_map *map);
extern taskexec_t __mydns_update_get_rr_data_srv(TASK *t,
						 UQRR *rr,
						 char **data,
						 size_t *datalen,
						 char **edata,
						 size_t *edatalen,
						 uint32_t *aux,
						 dns_qtype_map *map);
extern taskexec_t __mydns_update_get_rr_data_txt(TASK *t,
						 UQRR *rr,
						 char **data,
						 size_t *datalen,
						 char **edata,
						 size_t *edatalen,
						 uint32_t *aux,
						 dns_qtype_map *map);

#endif

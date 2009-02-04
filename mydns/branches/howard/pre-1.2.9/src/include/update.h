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

extern taskexec_t __mydns_update_get_rr_data_unknown_type(TASKP t,
							  UQRRP rr,
							  char **data,
							  size_t *datalen,
							  char **edata,
							  size_t *edatalen,
							  uint32_t *aux,
							  dns_qtype_mapp map);
extern taskexec_t __mydns_update_get_rr_data_a(TASKP t,
					       UQRRP rr,
					       char **data,
					       size_t *datalen,
					       char **edata,
					       size_t *edatalen,
					       uint32_t *aux,
					       dns_qtype_mapp map);
extern taskexec_t __mydns_update_get_rr_data_aaaa(TASKP t,
						  UQRRP rr,
						  char **data,
						  size_t *datalen,
						  char **edata,
						  size_t *edatalen,
						  uint32_t *aux,
						  dns_qtype_mapp map);
extern taskexec_t __mydns_update_get_rr_data_cname(TASKP t,
						   UQRRP rr,
						   char **data,
						   size_t *datalen,
						   char **edata,
						   size_t *edatalen,
						   uint32_t *aux,
						   dns_qtype_mapp map);
extern taskexec_t __mydns_update_get_rr_data_hinfo(TASKP t,
						   UQRRP rr,
						   char **data,
						   size_t *datalen,
						   char **edata,
						   size_t *edatalen,
						   uint32_t *aux,
						   dns_qtype_mapp map);
extern taskexec_t __mydns_update_get_rr_data_mx(TASKP t,
						UQRRP rr,
						char **data,
						size_t *datalen,
						char **edata,
						size_t *edatalen,
						uint32_t *aux,
						dns_qtype_mapp map);
extern taskexec_t __mydns_update_get_rr_data_ns(TASKP t,
						UQRRP rr,
						char **data,
						size_t *datalen,
						char **edata,
						size_t *edatalen,
						uint32_t *aux,
						dns_qtype_mapp map);
extern taskexec_t __mydns_update_get_rr_data_ptr(TASKP t,
						 UQRRP rr,
						 char **data,
						 size_t *datalen,
						 char **edata,
						 size_t *edatalen,
						 uint32_t *aux,
						 dns_qtype_mapp map);
extern taskexec_t __mydns_update_get_rr_data_rp(TASKP t,
						UQRRP rr,
						char **data,
						size_t *datalen,
						char **edata,
						size_t *edatalen,
						uint32_t *aux,
						dns_qtype_mapp map);
extern taskexec_t __mydns_update_get_rr_data_srv(TASKP t,
						 UQRRP rr,
						 char **data,
						 size_t *datalen,
						 char **edata,
						 size_t *edatalen,
						 uint32_t *aux,
						 dns_qtype_mapp map);
extern taskexec_t __mydns_update_get_rr_data_txt(TASKP t,
						 UQRRP rr,
						 char **data,
						 size_t *datalen,
						 char **edata,
						 size_t *edatalen,
						 uint32_t *aux,
						 dns_qtype_mapp map);

#endif

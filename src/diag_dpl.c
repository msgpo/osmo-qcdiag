/* Utility code for Diag Packet Logging */
/*
 * (C) 2016 by Harald Welte <laforge@gnumonks.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <errno.h>
#include <string.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/msgb.h>

#include "diag_log.h"
#include "diag_io.h"
#include "diag_dpl.h"
#include "protocol/diagcmd.h"
#include "protocol/diag_log_1x.h"
#include "protocol/dpl.h"

/* iface-id 0..99 */
/* every interface has multiple physical links */
/* iid contains flag info: 16bits LDFx IFNA PROT LINS, see struct
 * dpl_iid
 */

int diag_dpl_reset_logging(struct diag_instance *di)
{
	struct msgb *msg = msgb_alloc_diag();
	struct msgb *rx;
	diag_push_subsys_hdr(msg, DIAG_SUBSYS_PS_DATA_LOGGING,
			     DIAG_DPL_RESET_LOGGING);
	rx = diag_transceive_msg(di, msg);
	/* FIXME */
	msgb_free(rx);
	return 0;
}

int diag_dpl_get_sup_if(struct diag_instance *di)
{
	struct msgb *msg = msgb_alloc_diag();
	struct msgb *rx;
	struct dpl_get_sup_if_resp *gsir;
	rx = diag_subsys_transceive_msg(di, msg, DIAG_SUBSYS_PS_DATA_LOGGING,
					DIAG_DPL_GET_SUPPORTED_IFACES);
	if (!rx)
		return -EINVAL;
	gsir = (struct dpl_get_sup_if_resp *) msg->l3h;
	printf("DPL Supported interfaces: %s\n",
		osmo_hexdump(gsir->iface_id, gsir->num_ifaces));
	msgb_free(rx);
	return 0;
}

int diag_dpl_get_if_desc(struct diag_instance *di, uint8_t iface_id)
{
	struct msgb *msg = msgb_alloc_diag();
	struct msgb *rx;
	struct dpl_get_if_desc_req *gidr;
	uint8_t if_num, num_links;
	char *if_name, *link_name;
	unsigned int i;

	gidr = (struct dpl_get_if_desc_req *) msgb_put(msg, sizeof(*gidr));
	gidr->iface_id = iface_id;
	rx = diag_subsys_transceive_msg(di, msg, DIAG_SUBSYS_PS_DATA_LOGGING,
					DIAG_DPL_GET_IFACE_DESC);
	if (!rx)
		return -EINVAL;
	if_num = rx->l3h[0];
	if_name = (char *)rx->l3h+1;
	num_links = *(rx->l3h+1+strlen(if_name)+1);
	printf("DPL Interface %u \"%s\" num_links=%u\n", if_num, if_name, num_links);
	link_name = (char *) rx->l3h+1+strlen(if_name)+1+1;
	for (i = 0; i < num_links; i++) {
		printf("\tLink %u: \"%s\"\n", i, link_name);
		link_name += strlen(link_name) + 1;
	}
	msgb_free(rx);
	return 0;
}

int diag_dpl_set_if_log(struct diag_instance *di, uint8_t iface_id,
			struct dpl_iid iid, uint32_t link_type)
{
	struct msgb *msg = msgb_alloc_diag();
	struct dpl_set_if_log_req *silr;
	struct msgb *rx;
	int i;

	silr = (struct dpl_set_if_log_req *) msgb_put(msg, sizeof(*silr));
	silr->iface_id = iface_id;
	silr->num_log_flags = 0;
	for (i = 0; i < 1; i++) {
		msgb_put(msg, sizeof(silr->log_flags[0]));
		silr->num_log_flags++;
		iid.link_instance = i;
		silr->log_flags[i].iid = iid;
		silr->log_flags[i].link_type = link_type;
	}
	rx = diag_subsys_transceive_msg(di, msg, DIAG_SUBSYS_PS_DATA_LOGGING,
					DIAG_DPL_SET_IFACE_LOGGING);
	/* FIXME */
	msgb_free(rx);

	return 0;
}

/* LOG_DATA_PROTOCOL_LOGGING_C must be enabled */
/* dpli_log_full_packet() used to log packet; maximum of 256 bytes per
 * diag message; DPLI_BUILD_LOG_PACKET */
static void handle_pcap_msg(struct diag_instance *di, struct log_hdr *lh, struct msgb *msg)
{
	struct dpl_hdr *dh = (struct dpl_hdr *) msgb_data(msg);
	printf("PCAP(fl=0x%02x, ifn=0x%02x, prot=0x%02x, inst=%u, seq=%u, seg=%u): %s\n",
		dh->iid.flags, dh->iid.if_name, dh->iid.protocol,
		dh->iid.link_instance, dh->seq_nr, dh->seg_num,
		osmo_hexdump(dh->data, msgb_length(msg)-sizeof(*dh)));
}

static const struct diag_log_dispatch_tbl log_tbl[] = {
	{ L1X(LOG_DATA_PROTOCOL_LOGGING_C), handle_pcap_msg },
};

#if 0
static __attribute__((constructor)) void on_dso_load_gsm(void)
{
	diag_log_reg_dispatch(log_tbl, ARRAY_SIZE(log_tbl));
}
#endif

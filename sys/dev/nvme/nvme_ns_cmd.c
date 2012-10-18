/*-
 * Copyright (C) 2012 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "nvme_private.h"

int
nvme_ns_cmd_read(struct nvme_namespace *ns, void *payload, uint64_t lba,
    uint32_t lba_count, nvme_cb_fn_t cb_fn, void *cb_arg)
{
	struct nvme_request	*req;
	struct nvme_tracker	*tr;
	struct nvme_command	*cmd;
	int			err;

	req = nvme_allocate_request(payload, lba_count*512, cb_fn, cb_arg);

	tr = nvme_allocate_tracker(ns->ctrlr, FALSE, req);

	if (tr == NULL)
		return (ENOMEM);

	cmd = &req->cmd;
	cmd->opc = NVME_OPC_READ;
	cmd->nsid = ns->id;

	/* TODO: create a read command data structure */
	*(uint64_t *)&cmd->cdw10 = lba;
	cmd->cdw12 = lba_count-1;

	err = bus_dmamap_load(tr->qpair->dma_tag, tr->payload_dma_map, payload,
	    req->payload_size, nvme_payload_map, tr, 0);

	KASSERT(err == 0, ("bus_dmamap_load returned non-zero!\n"));

	return (0);
}

int
nvme_ns_cmd_write(struct nvme_namespace *ns, void *payload, uint64_t lba,
    uint32_t lba_count, nvme_cb_fn_t cb_fn, void *cb_arg)
{
	struct nvme_request	*req;
	struct nvme_tracker	*tr;
	struct nvme_command	*cmd;
	int 			err;

	req = nvme_allocate_request(payload, lba_count*512, cb_fn, cb_arg);

	tr = nvme_allocate_tracker(ns->ctrlr, FALSE, req);

	if (tr == NULL)
		return (ENOMEM);

	cmd = &req->cmd;
	cmd->opc = NVME_OPC_WRITE;
	cmd->nsid = ns->id;

	/* TODO: create a write command data structure */
	*(uint64_t *)&cmd->cdw10 = lba;
	cmd->cdw12 = lba_count-1;

	err = bus_dmamap_load(tr->qpair->dma_tag, tr->payload_dma_map, payload,
	    req->payload_size, nvme_payload_map, tr, 0);

	KASSERT(err == 0, ("bus_dmamap_load returned non-zero!\n"));

	return (0);
}

int
nvme_ns_cmd_deallocate(struct nvme_namespace *ns, void *payload,
    uint8_t num_ranges, nvme_cb_fn_t cb_fn, void *cb_arg)
{
	struct nvme_request	*req;
	struct nvme_tracker	*tr;
	struct nvme_command	*cmd;
	int 			err;

	req = nvme_allocate_request(payload,
	    num_ranges * sizeof(struct nvme_dsm_range), cb_fn, cb_arg);

	tr = nvme_allocate_tracker(ns->ctrlr, FALSE, req);

	if (tr == NULL)
		return (ENOMEM);

	cmd = &req->cmd;
	cmd->opc = NVME_OPC_DATASET_MANAGEMENT;
	cmd->nsid = ns->id;

	/* TODO: create a delete command data structure */
	cmd->cdw10 = num_ranges;
	cmd->cdw11 = NVME_DSM_ATTR_DEALLOCATE;

	err = bus_dmamap_load(tr->qpair->dma_tag, tr->payload_dma_map, payload,
	    req->payload_size, nvme_payload_map, tr, 0);

	KASSERT(err == 0, ("bus_dmamap_load returned non-zero!\n"));

	return (0);
}

int
nvme_ns_cmd_flush(struct nvme_namespace *ns, nvme_cb_fn_t cb_fn, void *cb_arg)
{
	struct nvme_request	*req;
	struct nvme_tracker	*tr;
	struct nvme_command	*cmd;

	req = nvme_allocate_request(NULL, 0, cb_fn, cb_arg);

	tr = nvme_allocate_tracker(ns->ctrlr, FALSE, req);

	if (tr == NULL)
		return (ENOMEM);

	cmd = &req->cmd;
	cmd->opc = NVME_OPC_FLUSH;
	cmd->nsid = ns->id;

	nvme_qpair_submit_cmd(tr->qpair, tr);

	return (0);
}

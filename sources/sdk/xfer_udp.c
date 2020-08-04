/******************************************************************************
 *
 * Copyright (C) 2017 Xilinx, Inc.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Use of the Software is limited solely to applications:
 * (a) running on a Xilinx device, or
 * (b) that interact with a Xilinx device through a bus or interconnect.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of the Xilinx shall not be used
 * in advertising or otherwise to promote the sale, use or other dealings in
 * this Software without prior written authorization from Xilinx.
 *
 ******************************************************************************/

/** Connection handle for a UDP Server session */

#include "xfer_udp.h"
#include "math.h"
#include "sleep.h"

#define MAX_SEGMENTS 65536
#define SEGMENT_SIZE 8188
//#define MAX_BURST 128

extern struct netif server_netif;
extern uint8_t rx_lock;
uint8_t xfer_state=XFER_STATE_INIT;

struct udp_pcb *pcb;

static uint8_t received_segment_bitmap[MAX_SEGMENTS / 8];
static uint16_t *req_segs_ptr; // array of SEG_NUMS to be transmitted

static uint32_t tx_dummy[SEGMENT_SIZE / 4];

static uint8_t xfer_req_flag=0,xfer_start_flag=0,xfer_bar_flag=0,isr_flag=0;
uint32_t *payload_ptr;



static void reset_rcv_bitmap(uint16_t num_seg ) {
  uint16_t i;
  for (i = 0; i < MAX_SEGMENTS / 8; i++) {
    received_segment_bitmap[i] = 0;
  }
}

static void update_tx_dummy(uint32_t inc) {
  uint32_t i;
  for (i = 0; i < SEGMENT_SIZE / 4; i++) {
    tx_dummy[i] = i + inc * SEGMENT_SIZE / 4;
  }
}

static uint16_t set_req_segs_ptr(uint32_t *req_seg_bitmap,
                                 uint16_t *req_segs_ptr, uint16_t num_seg) {
  uint32_t i, j = 0;
  uint32_t index, offset;
  uint8_t data;
  for (i = 0; i < num_seg; i++) {
    index = i / 32;
    offset = i % 32;
    data = (req_seg_bitmap[index] >> (offset)) & 0x1;
    if (data) {
      req_segs_ptr[j] = i;
      j++;
    }
  }
  return j;
}

void udp_xfer_send(uint32_t *payload, uint32_t xfer_size) {
  struct pbuf *p;
  p = pbuf_alloc(PBUF_TRANSPORT, xfer_size, PBUF_RAM);
  memcpy(p->payload, payload, xfer_size);
  udp_send(pcb, p);

  pbuf_free(p);
}


void transfer_data(void ) {
	  uint16_t seg_num = 0,num_seg = 0;
	  uint32_t header;
	  struct pbuf *pH;
	  struct pbuf *pD;
	  uint16_t burst_length;
	  uint8_t delay;
	if (xfer_req_flag)
	{

	    num_seg = (payload_ptr[0] >> 16) & 0x0000FFFF;
	    delay = ((payload_ptr[0] >> 8) & 0xFF);

	    req_segs_ptr = malloc(sizeof(uint16_t) * num_seg);
	    burst_length = set_req_segs_ptr(&payload_ptr[1], req_segs_ptr, num_seg);

    for (uint32_t i = 0; i < burst_length;i++ ) {
      seg_num = req_segs_ptr[i];
      update_tx_dummy(req_segs_ptr[i]);
      header = (XFER_DATA & 0x0000FFFF) | ((seg_num << 16) & 0xFFFF0000);
      pH = pbuf_alloc(PBUF_TRANSPORT, sizeof(uint32_t), PBUF_RAM);
      pD = pbuf_alloc(PBUF_TRANSPORT, sizeof(uint32_t) * SEGMENT_SIZE / 4,
                      PBUF_RAM);
      memcpy(pH->payload, &header, sizeof(header));
      memcpy(pD->payload, tx_dummy, sizeof(tx_dummy));
      pbuf_cat(pH, pD);
      udp_send(pcb, pH);
      pbuf_free(pH);
      usleep(delay*100);
    }
    xfer_req_flag=0;
	}

	if (xfer_start_flag)
	{
	    num_seg = (payload_ptr[0] >> 16) & 0x0000FFFF;
	    reset_rcv_bitmap(num_seg);
		xfer_start_flag=0;
	}

	if (xfer_bar_flag)
	{
		    pH = pbuf_alloc(PBUF_TRANSPORT, sizeof(uint32_t), PBUF_RAM);
		    pD = pbuf_alloc(PBUF_TRANSPORT, sizeof(uint8_t) * 8192, PBUF_RAM);
		    header = XFER_BA;
		    memcpy(pH->payload, &header, sizeof(uint32_t));
		    pD->payload = received_segment_bitmap;
		    pbuf_cat(pH, pD);
		    udp_send(pcb, pH); // SEND BA
		    pbuf_free(pH);
		    xfer_bar_flag=0;
	}
	isr_flag=xfer_req_flag|xfer_start_flag|xfer_bar_flag;

}

/** Receive data on a udp session */
static void udp_recv_cb(void *arg, struct udp_pcb *tpcb, struct pbuf *p,
                        const ip_addr_t *addr, u16_t port) {
  uint8_t cmd = 0;
  uint16_t seg_num = 0;

  uint8_t mask;
  uint16_t index;


if (!isr_flag)
{ payload_ptr = p->payload;
  cmd = payload_ptr[0];

  switch (cmd) {
  case XFER_REQ:
	  xfer_req_flag=1;
    break;
  case XFER_START: // XFER_START
	  if (p->tot_len==sizeof(uint32_t)){
    udp_send(tpcb, p); // SEND ACK
    xfer_start_flag=1;
	  }
    break;
  case XFER_DATA: // DATA
    seg_num = (payload_ptr[0] >> 16) & 0x0000FFFF;
    mask = 1 << (seg_num % 8);
    index = seg_num / 8;
    received_segment_bitmap[index] = received_segment_bitmap[index] + (mask);
    break;
  case TX_ST:
    udp_send(tpcb, p); // SEND ACK
    break;

  case XFER_BAR: // BAR
	  xfer_bar_flag=1;
    break;
  case XFER_STATE: // XFER_STATE
	  xfer_state = (payload_ptr[0] >> 24) & 0x000000FF;
	      udp_sendto(tpcb, p, addr, port); // SEND ACK
	    pcb=tpcb;
	    udp_connect(pcb,addr,port);
	    rx_lock=0;
    break;
  default:
	    break;

  }
  isr_flag=1;
}

  pbuf_free(p);

  return;
}


void start_application(void)
{
	err_t err;
uint32_t i;
	/* Create Server PCB */
	pcb = udp_new();
	if (!pcb) {
		xil_printf("UDP server: Error creating PCB. Out of Memory\r\n");
		return;
	}

	err = udp_bind(pcb, IP_ADDR_ANY, UDP_CONN_PORT);
	if (err != ERR_OK) {
		xil_printf("UDP server: Unable to bind to port");
		xil_printf(" %d: err = %d\r\n", UDP_CONN_PORT, err);
		udp_remove(pcb);
		return;
	}

	/* specify callback to use for incoming connections */
	udp_recv(pcb, udp_recv_cb, NULL);

	  for (i = 0; i < SEGMENT_SIZE / 4; i++) {
	    tx_dummy[i] = i;
	  }

	return;
}

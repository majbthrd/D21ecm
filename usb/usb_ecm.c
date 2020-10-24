/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Peter Lawrence
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdbool.h>
#include <stdalign.h>
#include <string.h>
#include <stddef.h>
#include "utils.h"
#include "usb.h"
#include "usb_std.h"
#include "usb_ecm.h"

static alignas(4) uint8_t received[ECM_MAX_SEGMENT_SIZE];
static alignas(4) uint8_t transmitted[ECM_MAX_SEGMENT_SIZE];
static alignas(4) usb_request_t notify_nc = 
{
  .bmRequestType = 0xA1,
  .bRequest = 0 /* NETWORK_CONNECTION */,
  .wValue = 1 /* Connected */,
  .wIndex = USB_ECM_NOTIFY_ITF,
  .wLength = 0,
};
static alignas(4) struct
{
  usb_request_t header;
  uint32_t downlink, uplink;
} notify_csc = 
{
  .header =
  {
    .bmRequestType = 0xA1,
    .bRequest = 0x2A /* CONNECTION_SPEED_CHANGE */,
    .wIndex = USB_ECM_NOTIFY_ITF,
    .wLength = 0,
  },
  .downlink = 9728000,
  .uplink = 9728000,
};

static bool can_xmit;

static void usb_ecm_ep_send_callback(int size);
static void usb_ecm_ep_recv_callback(int size);
static void usb_ecm_ep_comm_callback(int size);

void usb_ecm_init(void)
{
  usb_set_callback(USB_ECM_EP_SEND, usb_ecm_ep_send_callback);
  usb_set_callback(USB_ECM_EP_RECV, usb_ecm_ep_recv_callback);
  usb_set_callback(USB_ECM_EP_COMM, usb_ecm_ep_comm_callback);
}

static void usb_ecm_send(uint8_t *data, int size)
{
  usb_send(USB_ECM_EP_SEND, data, size);
}

void usb_ecm_recv_renew(void)
{
  usb_recv(USB_ECM_EP_RECV, received, sizeof(received));
}

void usb_configuration_callback(int config)
{
  (void)config;

  usb_ecm_recv_renew();
  can_xmit = true;
}

static void ecm_report(bool nc)
{
  if (nc)
  {
    /* provide a Management Element Notification with a NETWORK_CONNECTION status */
    usb_send(USB_ECM_EP_COMM, (uint8_t *)&notify_nc, sizeof(notify_nc));
  }
  else
  {
    /* provide a Management Element Notification with a CONNECTION_SPEED_CHANGE status */
    usb_send(USB_ECM_EP_COMM, (uint8_t *)&notify_csc, sizeof(notify_csc));
  }
}

void usb_interface_callback(int interface)
{
  if (interface)
    ecm_report(true);
}

static void usb_ecm_ep_send_callback(int size)
{
  (void)size;

  can_xmit = true;
}

static void usb_ecm_ep_recv_callback(int size)
{
  usb_ecm_recv_callback(received, size);
}

static void usb_ecm_ep_comm_callback(int size)
{
  if (sizeof(notify_nc) == size)
    ecm_report(false);
}

bool usb_class_handle_request(usb_request_t *request)
{
  int length = request->wLength;

  if (0x20 /* CLASS */ != (request->bmRequestType & 0x60))
    return false;

  /* the only required CDC-ECM Management Element Request is SetEthernetPacketFilter */

  if (0x43 /* SET_ETHERNET_PACKET_FILTER */ == request->bRequest)
  {
    usb_control_send_zlp();
    ecm_report(true);
    return true;
  }

  return false;
}

bool usb_ecm_can_xmit(void)
{
  return can_xmit;
}

void usb_ecm_xmit_packet(struct pbuf *p)
{
  struct pbuf *q;
  uint8_t *data;
  int packet_size;

  if (!can_xmit)
    return;

  data = transmitted;
  packet_size = 0;
  for(q = p; q != NULL; q = q->next)
  {
    memcpy(data, (char *)q->payload, q->len);
    data += q->len;
    packet_size += q->len;
    if (q->tot_len == q->len) break;
  }

  can_xmit = false;
  usb_ecm_send(transmitted, packet_size);
}

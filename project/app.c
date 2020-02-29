/*
 SAMD21-based USB CDC-ECM implementation with embedded web server
  ingredients includes:
   vcp : https://github.com/ataradov/vcp by Alex Taradov 
   lrndis : https://github.com/fetisov/lrndis by Sergey Fetisov
    lwIP : https://savannah.nongnu.org/projects/lwip/ by many
    usbstick : https://colinoflynn.com/tag/usb/ by Colin O'Flynn
   stm32ecm: https://github.com/majbthrd/stm32ecm by Peter Lawrence
*/

/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Peter Lawrence
 * Copyright (c) 2015 by Sergey Fetisov <fsenok@gmail.com>
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

#include <sam.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdalign.h>
#include <string.h>
#include "hal_gpio.h"
#include "nvm_data.h"
#include "usb.h"
#include "dhserver.h"
#include "dnserver.h"
#include "netif/etharp.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/icmp.h"
#include "lwip/udp.h"
#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"
#include "lwip/inet.h"
#include "lwip/dns.h"
#include "lwip/tcp.h"
#include "lwip/sys.h"
#include "time.h"
#include "httpd.h"
#include "ecmhelper.h"

static struct netif netif_data;
static const uint8_t hwaddr[6]  = {0x20,0x89,0x84,0x6A,0x96,0x00};
static const uint8_t ipaddr[4]  = {192, 168, 7, 1};
static const uint8_t netmask[4] = {255, 255, 255, 0};
static const uint8_t gateway[4] = {0, 0, 0, 0};
static struct pbuf *received_frame;

static dhcp_entry_t entries[] =
{
    /* mac    ip address        subnet mask        lease time */
    { {0}, {192, 168, 7, 2}, {255, 255, 255, 0}, 24 * 60 * 60 },
    { {0}, {192, 168, 7, 3}, {255, 255, 255, 0}, 24 * 60 * 60 },
    { {0}, {192, 168, 7, 4}, {255, 255, 255, 0}, 24 * 60 * 60 }
};

static const dhcp_config_t dhcp_config =
{
    {192, 168, 7, 1}, 67, /* server address, port */
    {192, 168, 7, 1},     /* dns server */
    "sam",                /* dns suffix */
    ARRAY_SIZE(entries),  /* num entry */
    entries               /* entries */
};

static void device_init(void)
{
  uint32_t sn = 0;

#if 1
  /*
  configure oscillator for crystal-free USB operation (USBCRM / USB Clock Recovery Mode)
  */
  uint32_t coarse, fine;

  NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CACHEDIS | NVMCTRL_CTRLB_RWS(2);

  SYSCTRL->INTFLAG.reg = SYSCTRL_INTFLAG_BOD33RDY | SYSCTRL_INTFLAG_BOD33DET | SYSCTRL_INTFLAG_DFLLRDY;

  coarse = NVM_READ_CAL(NVM_DFLL48M_COARSE_CAL);
  fine = NVM_READ_CAL(NVM_DFLL48M_FINE_CAL);

  SYSCTRL->DFLLCTRL.reg = 0; // See Errata 9905
  while (0 == (SYSCTRL->PCLKSR.reg & SYSCTRL_PCLKSR_DFLLRDY));

  SYSCTRL->DFLLMUL.reg = SYSCTRL_DFLLMUL_MUL(48000);
  SYSCTRL->DFLLVAL.reg = SYSCTRL_DFLLVAL_COARSE(coarse) | SYSCTRL_DFLLVAL_FINE(fine);

  SYSCTRL->DFLLCTRL.reg = SYSCTRL_DFLLCTRL_ENABLE | SYSCTRL_DFLLCTRL_USBCRM | SYSCTRL_DFLLCTRL_MODE | SYSCTRL_DFLLCTRL_CCDIS;

  while (0 == (SYSCTRL->PCLKSR.reg & SYSCTRL_PCLKSR_DFLLRDY));

  GCLK->GENCTRL.reg = GCLK_GENCTRL_ID(0) | GCLK_GENCTRL_SRC(GCLK_SOURCE_DFLL48M) | GCLK_GENCTRL_RUNSTDBY | GCLK_GENCTRL_GENEN;
  while (GCLK->STATUS.reg & GCLK_STATUS_SYNCBUSY);
#else
  /*
  configure oscillator for operation disciplined by external 32k crystal

  This can only be used on PCBs (such as Arduino Zero derived designs) that have these extra components populated.
  It *should* be wholly unnecessary to use this instead of the above USBCRM code.
  However, some problem (Sparkfun?) PCBs experience unreliable USB operation in USBCRM mode.
  */

  NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_RWS_DUAL;

  SYSCTRL->XOSC32K.reg = SYSCTRL_XOSC32K_STARTUP( 0x6u ) | SYSCTRL_XOSC32K_XTALEN | SYSCTRL_XOSC32K_EN32K;
  SYSCTRL->XOSC32K.reg |= SYSCTRL_XOSC32K_ENABLE;

  while (!(SYSCTRL->PCLKSR.reg & SYSCTRL_PCLKSR_XOSC32KRDY));

  GCLK->GENDIV.reg = GCLK_GENDIV_ID( 1u /* XOSC32K */ );

  GCLK->GENCTRL.reg = GCLK_GENCTRL_ID( 1u /* XOSC32K */ ) | GCLK_GENCTRL_SRC_XOSC32K | GCLK_GENCTRL_GENEN;

  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID( 0u /* DFLL48M */ ) | GCLK_CLKCTRL_GEN_GCLK1 | GCLK_CLKCTRL_CLKEN;

  while (GCLK->STATUS.reg & GCLK_STATUS_SYNCBUSY);

  SYSCTRL->DFLLCTRL.reg = 0; // See Errata 9905
  while (!SYSCTRL->PCLKSR.bit.DFLLRDY);

  SYSCTRL->DFLLMUL.reg = SYSCTRL_DFLLMUL_CSTEP( 31 ) | SYSCTRL_DFLLMUL_FSTEP( 511 ) | SYSCTRL_DFLLMUL_MUL(48000000ul / 32768ul);

  SYSCTRL->DFLLCTRL.reg = SYSCTRL_DFLLCTRL_ENABLE | SYSCTRL_DFLLCTRL_MODE | SYSCTRL_DFLLCTRL_WAITLOCK | SYSCTRL_DFLLCTRL_QLDIS;

  while ( !(SYSCTRL->PCLKSR.reg & SYSCTRL_PCLKSR_DFLLLCKC) || !(SYSCTRL->PCLKSR.reg & SYSCTRL_PCLKSR_DFLLLCKF) || !(SYSCTRL->PCLKSR.reg & SYSCTRL_PCLKSR_DFLLRDY) );

  GCLK->GENDIV.reg = GCLK_GENDIV_ID( 0u /* MAIN */ );

  GCLK->GENCTRL.reg = GCLK_GENCTRL_ID( 0u /* MAIN */ ) | GCLK_GENCTRL_SRC_DFLL48M | GCLK_GENCTRL_IDC | GCLK_GENCTRL_GENEN;

  while (GCLK->STATUS.reg & GCLK_STATUS_SYNCBUSY);
#endif

  /* update global var to reflect clock change made */
  SystemCoreClock = 48000000UL;

  sn ^= *(volatile uint32_t *)0x0080a00c;
  sn ^= *(volatile uint32_t *)0x0080a040;
  sn ^= *(volatile uint32_t *)0x0080a044;
  sn ^= *(volatile uint32_t *)0x0080a048;

  /* the SN doubles as the MAC address; using this prefix indicates a locally-administered MAC address */
  usb_serial_number[0] = '0';
  usb_serial_number[1] = '2';
  usb_serial_number[2] = '0';
  usb_serial_number[3] = '2';

  for (int i = 0; i < 8; i++)
    usb_serial_number[4 + i] = "0123456789ABCDEF"[(sn >> (i * 4)) & 0xf];

  usb_serial_number[12] = 0;

  time_init();
}

void usb_ecm_recv_callback(const uint8_t *data, int size)
{
  if (received_frame)
    return;

  received_frame = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);
  if (!received_frame) 
    return;

  memcpy(received_frame->payload, data, size);
  received_frame->len = size;
}

err_t output_fn(struct netif *netif, struct pbuf *p, const ip_addr_t *ipaddr)
{
    return etharp_output(netif, p, ipaddr);
}

err_t linkoutput_fn(struct netif *netif, struct pbuf *p)
{
    for (;;)
    {
        if (usb_ecm_can_xmit()) goto ok_to_xmit;
        usb_task();
    }

    return ERR_USE;

ok_to_xmit:
    usb_ecm_xmit_packet(p);

    return ERR_OK;
}

err_t netif_init_cb(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));
    netif->mtu = ECM_MTU;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_UP;
    netif->state = NULL;
    netif->name[0] = 'E';
    netif->name[1] = 'X';
    netif->linkoutput = linkoutput_fn;
    netif->output = output_fn;
    return ERR_OK;
}

#define PADDR(ptr) ((ip_addr_t *)ptr)

void init_lwip()
{
    struct netif  *netif = &netif_data;

    lwip_init();
    netif->hwaddr_len = 6;
    memcpy(netif->hwaddr, hwaddr, 6);

    netif = netif_add(netif, PADDR(ipaddr), PADDR(netmask), PADDR(gateway), NULL, netif_init_cb, ip_input);
    netif_set_default(netif);
}

static void init_periph(void)
{
  device_init();
  usb_init();
  usb_ecm_init();
}

bool dns_query_proc(const char *name, ip_addr_t *addr)
{
    if (strcmp(name, "run.sam") == 0 || strcmp(name, "www.run.sam") == 0)
    {
        addr->addr = *(uint32_t *)ipaddr;
        return true;
    }
    return false;
}

const char *state_cgi_handler(int index, int n_params, char *params[], char *values[])
{
    return "/state.shtml";
}

bool alpha = false;
bool bravo = false;
bool charlie = false;

const char *ctl_cgi_handler(int index, int n_params, char *params[], char *values[])
{
    int i;
    for (i = 0; i < n_params; i++)
    {
        if (strcmp(params[i], "a") == 0) alpha = *values[i] == '1';
        if (strcmp(params[i], "b") == 0) bravo = *values[i] == '1';
        if (strcmp(params[i], "c") == 0) charlie = *values[i] == '1';
    }

    return "/state.shtml";
}

static const char *ssi_tags_table[] =
{
    "systick", /* 0 */
    "alpha",   /* 1 */
    "bravo",   /* 2 */
    "charlie"  /* 3 */
};

static const tCGI cgi_uri_table[] =
{
    { "/state.cgi", state_cgi_handler },
    { "/ctl.cgi",   ctl_cgi_handler },
};

static u16_t ssi_handler(int index, char *insert, int ins_len)
{
    int res;

    if (ins_len < 32) return 0;

    switch (index)
    {
    case 0: /* systick */
        res = snprintf(insert, ins_len, "%u", sys_now());
        break;
    case 1: /* alpha */
        *insert = '0' + (alpha & 1);
        res = 1;
        break;
    case 2: /* bravo */
        *insert = '0' + (bravo & 1);
        res = 1;
        break;
    case 3: /* charlie */
        *insert = '0' + (charlie & 1);
        res = 1;
        break;
    }

    return res;
}

static void service_traffic(void)
{
  if (received_frame)
  {
    ethernet_input(received_frame, &netif_data);
    pbuf_free(received_frame);
    received_frame = NULL;
    usb_ecm_recv_renew();
  }
}

int main(void)
{
  init_periph();

  init_lwip();

  while (!netif_is_up(&netif_data));

  while (dhserv_init(&dhcp_config) != ERR_OK);

  while (dnserv_init(PADDR(ipaddr), 53, dns_query_proc) != ERR_OK);

  http_set_cgi_handlers(cgi_uri_table, sizeof(cgi_uri_table) / sizeof(tCGI));
  http_set_ssi_handler(ssi_handler, ssi_tags_table, sizeof(ssi_tags_table) / sizeof(char *));
  httpd_init();

  while (1)
  {
    usb_task();
    service_traffic();
  }

  return 0;
}

sys_prot_t sys_arch_protect(void) {}
void sys_arch_unprotect(sys_prot_t pval) {}

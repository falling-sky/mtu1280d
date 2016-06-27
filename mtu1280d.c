// Thanks to Austin Marton
// https://austinmarton.wordpress.com/2011/09/14/sending-raw-ethernet-packets-from-a-specific-interface-in-c-on-linux/
// csum() is borrowed from Austin; and csum_3() is derived from csum().

// Code not otherwise borrowed is 
// (C) 2015 by Jason Fesler <jfesler@gigo.com>
// Principally: anything to do with ICMPv6 responses
// The uglier it looks, the more likely it is mine.

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/ether.h>
#include <linux/if_packet.h>
#include <linux/sockios.h>

#include <linux/netfilter.h>	/* for NF_ACCEPT */
#include <libnetfilter_queue/libnetfilter_queue.h>

#define MTU 1280
#define ETHER_SIZE (6+6+2)
#define IPV6HDR_SIZE 40
#define ICMP6_SIZE 8
#define PAYLOAD_SIZE (MTU-(IPV6HDR_SIZE+ICMP6_SIZE))
#define ETHER_CRC_SIZE 4
#define ETHER_TOTAL_SIZE (MTU + ETHER_SIZE)


unsigned int queue = 1280;	// -q 
unsigned int do_fork = 0;	// -d
unsigned int do_debug = 0;	// -g 


void
must (char *s, int i)
{
  if (do_debug || (i == 0))
    printf ("must: %s (value %s)\n", s, i ? "true" : "false");
  if (!i)
    exit (1);
}




typedef struct fullframe
{
  u_int8_t ether_frame[ETHER_SIZE];
  u_int8_t ipv6_header[IPV6HDR_SIZE];
  u_int8_t icmp6_header[ICMP6_SIZE];
  u_int8_t payload[PAYLOAD_SIZE];
} fullframe;

int
sockfd (void)
{
  static sock = 0;
  if (!sock)
    {
      sock = socket (AF_PACKET, SOCK_RAW, IPPROTO_RAW);
    }
  if (sock == -1)
    {
      perror ("socket");
    }
}

uint8_t *
macaddr_for_interface (int i)
{
  static int last_i = 0xfffff;
  static uint8_t buffer[6];
  static uint8_t devname[IF_NAMESIZE];

  if (i != last_i)
    {
      int s = sockfd ();	// * Need a random socket FD to do ioctl against
      char *interface = NULL;
      memset (buffer, 0, sizeof (buffer));
      interface = if_indextoname (i, devname);
      struct ifreq ifr;

      if (interface)
	{
	  if (do_debug)
	    {
	      printf ("Looked up %d, found %s ", i, interface);
	    }

	  // Use ioctl() to look up interface name and get its MAC address.   
	  memset (&ifr, 0, sizeof (ifr));
	  snprintf (ifr.ifr_name, sizeof (ifr.ifr_name), "%s", interface);
	  must("ioctl() for source MAC address",ioctl (s, SIOCGIFHWADDR, &ifr) >= 0);
	  memcpy (buffer, ifr.ifr_hwaddr.sa_data, 6);
	  last_i = i;		// Save a lookup later.
	}

    }
  if (do_debug)
    {
      printf ("interface %d mac %02x:%02x:%02x:%02x:%02x:%02x",
	      i, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
    }

  return buffer;
}


void
hexdump (char *s, uint8_t * p, int n)
{
return;
  int i;

  if (!do_debug)
    {
      return;
    }

  printf ("\nHEXDUMP: %s\n", s);
  for (i = 0; i < n; i++)
    {
      if (i % 16 == 0)
	{
	  printf ("%04x:  ", i);
	}
      printf ("%02x", p[i]);
      if (i % 2 == 1)
	{
	  printf (" ");
	}
      if (i % 4 == 3)
	{
	  printf (" ");
	}
      if (i % 16 == 15)
	{
	  printf ("\n");
	}
    }
  printf ("\n");
}


uint16_t
csum (uint16_t * buf, int count)
{
  uint32_t sum;
  for (sum = 0; count > 0; count -= 2)
    sum += *buf++;
  sum = (sum >> 16) + (sum & 0xffff);
  sum += (sum >> 16);
  return (uint16_t) (~sum);
}

uint16_t
csum_3 (uint16_t * buf1, int count1, uint16_t * buf2, int count2, uint16_t * buf3, int count3)
{
  uint32_t sum;
  for (sum = 0; count1 > 0; count1 -= 2)
    sum += *buf1++;
  for (; count2 > 0; count2 -= 2)
    sum += *buf2++;
  for (; count3 > 0; count3 -= 2)
    sum += *buf3++;
  sum = (sum >> 16) + (sum & 0xffff);
  sum += (sum >> 16);
  return (uint16_t) (~sum);
}




/* returns packet id */
static u_int32_t
block_pkt (struct nfq_data *tb)
{
  int id = 0;
  struct nfqnl_msg_packet_hw *hwph;
  u_int32_t mark, ifi;
  int ret;
  int data_len;
  int copy_len;
  unsigned char *data;
  uint16_t c;
  static u_int8_t previous_macaddr[6] = { 0, 0, 0, 0, 0, 0 };

  // Where to address the raw packets - fill this in 
  // as we go
  struct sockaddr_ll socket_address;
  memset (&socket_address, 0, sizeof (socket_address));


  // I'm tired of fighting all the crud,
  // so I'm goingto just use a big block.
  fullframe buffer;
  memset (&buffer, 0, sizeof (buffer));
  assert (sizeof (buffer) == ETHER_TOTAL_SIZE);

  // We need
  // Ethernet header
  // IPv6 header
  // IPCMPv6 header
  // As much of the original payload as possible
  // Final ether CRC


  // Get the data payload from netfilter_queue
  ret = nfq_get_payload (tb, &data);
  data_len = ret;
  copy_len = (data_len > PAYLOAD_SIZE) ? PAYLOAD_SIZE : data_len;

  if (do_debug)
    {
      printf ("payload_len=%d copy_len=%d ", data_len, copy_len);
    }



  // What MAC address sent us this packet?
  // We intend to send the outbound packet
  // back to the same place.
  hwph = nfq_get_packet_hw (tb);
  if (hwph)
    {
      int i, hlen = ntohs (hwph->hw_addrlen);

      if (do_debug)
	{
	  printf ("hw_src_addr=");
	  for (i = 0; i < hlen - 1; i++)
	    printf ("%02x:", hwph->hw_addr[i]);
	  printf ("%02x ", hwph->hw_addr[hlen - 1]);
	}

      // Ethernet frame destination
      memcpy (&buffer.ether_frame[0], hwph->hw_addr, 6);
      memcpy (socket_address.sll_addr, hwph->hw_addr, 6);
      memcpy (previous_macaddr, hwph->hw_addr, 6);
      socket_address.sll_halen = ETH_ALEN;
    }
  else
    {
      if (do_debug)
	{
	  printf ("hw_src_addr=missing ");
	}
      memcpy (&buffer.ether_frame[0], previous_macaddr, 6);
      memcpy (socket_address.sll_addr, previous_macaddr, 6);
      socket_address.sll_halen = ETH_ALEN;
    }

  // Early-ish accept if the packets are small
  if ((data_len > 0) && (data_len <= 1280))
    {
      if (do_debug)
	{
	  printf ("Accepting!\n");
	}
      return NF_ACCEPT;		// iptables mark to keep the packet
    }


  // TODO: Ethernet frame source

  // Ethernet frame type
  buffer.ether_frame[12] = ETH_P_IPV6 / 256;
  buffer.ether_frame[13] = ETH_P_IPV6 % 256;

  // Show the ethernet frame
  hexdump ("DUMP: ether_frame", buffer.ether_frame, sizeof (buffer.ether_frame));


  // Start creating the IPv6 header
  buffer.ipv6_header[0] = 0x60;	// IPv6 "version=6"

  // What is the payload length?
  int plength = copy_len + ICMP6_SIZE;
  buffer.ipv6_header[4] = plength / 256;
  buffer.ipv6_header[5] = plength % 256;

  // What is the next header?
  buffer.ipv6_header[6] = 0x3a;	// ICMPv6
  buffer.ipv6_header[7] = 0xff;	// Hop limit

  // Source address, Destination Address
  // Just swap from what we saw in our input packet
  memcpy (&buffer.ipv6_header[8], &data[24], 16);
  memcpy (&buffer.ipv6_header[24], &data[8], 16);

  hexdump ("IP6 HEADER:", buffer.ipv6_header, sizeof (buffer.ipv6_header));

  // ICMPv6 header
  buffer.icmp6_header[0] = 2;	// Type 2 Packet Too Big
  buffer.icmp6_header[1] = 0;	// Code (not used)

  // TODO Checksum
  buffer.icmp6_header[2] = 0;	// TODO Checksum
  buffer.icmp6_header[3] = 0;	// TODO checksum

  // MTU expressed as 32 bits
  buffer.icmp6_header[4] = (MTU >> 24) & 0xff;
  buffer.icmp6_header[5] = (MTU >> 16) & 0xff;
  buffer.icmp6_header[6] = (MTU >> 8) & 0xff;
  buffer.icmp6_header[7] = MTU & 0xff;

  memcpy (buffer.payload, data, copy_len);
  hexdump ("ICMP6", buffer.icmp6_header, sizeof (buffer.icmp6_header) + copy_len);

  u_int8_t pseudoheader[40];
  memcpy (pseudoheader, &buffer.ipv6_header[8], 32);
  pseudoheader[32] = 0;		// length never more than 0xffff
  pseudoheader[33] = 0;		// length never more than 0xffff
  pseudoheader[34] = (ICMP6_SIZE + copy_len) / 256;
  pseudoheader[35] = (ICMP6_SIZE + copy_len) % 256;
  pseudoheader[36] = 0;		// zero
  pseudoheader[37] = 0;		// zero
  pseudoheader[38] = 0;		// zero 
  pseudoheader[39] = 58;	// ICMPv6 header code

  c = csum_3 ((uint16_t *) pseudoheader, sizeof (pseudoheader),
	      (uint16_t *) buffer.icmp6_header, sizeof (buffer.icmp6_header), (uint16_t *) buffer.payload, copy_len);
  buffer.icmp6_header[2] = c % 256;
  buffer.icmp6_header[3] = c / 256;

  hexdump ("PseudoHeader", pseudoheader, sizeof (pseudoheader));


  hexdump ("ICMP6", buffer.icmp6_header, sizeof (buffer.icmp6_header) + copy_len);






  // Device ID that the packet came from
  ifi = nfq_get_indev (tb);
  if (ifi)
    {
      if (do_debug)
	{
	  printf ("indev=%u ", ifi);
	}
      socket_address.sll_ifindex = ifi;
      memcpy (&buffer.ether_frame[6], macaddr_for_interface (ifi), 6);
    }

  if (do_debug)
    {
      fputc ('\n', stdout);
    }

  int tx_len = ETHER_SIZE + IPV6HDR_SIZE + ICMP6_SIZE + copy_len;
  if (sendto (sockfd (), &buffer, tx_len, 0, (struct sockaddr *) &socket_address, sizeof (struct sockaddr_ll)) < 0)
    printf ("Send failed\n");


  return NF_DROP;		// iptables will drop this later as being too big
}



static int
cb (struct nfq_q_handle *qh, struct nfgenmsg *nfmsg, struct nfq_data *nfa, void *data)
{
  struct nfqnl_msg_packet_hdr *ph;
  u_int32_t id = 0;

  if (do_debug)
    printf ("entering callback\n");

  must ("nfq_get_msg_packet_hdr", (ph = nfq_get_msg_packet_hdr (nfa)) != 0);
  id = ntohl (ph->packet_id);
  if (do_debug)
    printf ("hw_protocol=0x%04x hook=%u id=%u ", ntohs (ph->hw_protocol), ph->hook, id);
  int v = block_pkt (nfa);
  if (do_debug)
    printf ("\nnfq_set_verdict(qh, id=%d, v=%d, 0, NULL)\n", id, v);
  return nfq_set_verdict (qh, id, v, 0, NULL);
}


int
main (int argc, char **argv)
{
  struct nfq_handle *h;
  struct nfq_q_handle *qh;
  struct nfnl_handle *nh;
  int fd;
  int rv;
  char *interface;
  char buf[4096] __attribute__ ((aligned));

// Getopt        
  int c;
  int opterr = 0;
  while ((c = getopt (argc, argv, "dgq:")) != -1)
    switch (c)
      {
      case 'd':
	do_fork = 1;
	break;
      case 'g':
	do_debug = 1;
	break;
      case 'q':
	queue = strtol (optarg, NULL, 10);
	break;
      case '?':
	if (optopt == 'q')
	  fprintf (stderr, "Option -%c requires an argument.\n", optopt);
	else if (isprint (optopt))
	  fprintf (stderr, "Unknown option `-%c'.\n", optopt);
	else
	  fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
	return 1;
      default:
	abort ();
      }


if (getuid () != 0) {
   fprintf(stdout,"WARNING: Not running as root.  If this fails, either run as root, or grant CAP_NET_ADMIN.\n");
}

  must ("nfq_open", (h = nfq_open ()) != 0);
  must ("nfq_unbind_pf", nfq_unbind_pf (h, AF_INET6) >= 0);
  must ("nfq_bind_pf", nfq_bind_pf (h, AF_INET6) >= 0);
  must ("nfq_create_queue", (qh = nfq_create_queue (h, queue, &cb, NULL)) != 0);
  must ("nfq_set_mode", nfq_set_mode (qh, NFQNL_COPY_PACKET, 0xffff) >= 0);

  if (do_fork)
    {
      fprintf (stdout, "forking to background\n");
      daemon (0, 0);
    }


  must ("nfq_fd", (fd = nfq_fd (h)) > 0);

  while (1)
    {
      rv = recv (fd, buf, sizeof (buf), 0);
      if (rv > 0)
	nfq_handle_packet (h, buf, rv);
      if ((rv < 0) && (errno != ENOBUFS))
	{
	  perror ("recv:");
	  exit (1);
	}

      if (rv == 0)
	break;
    }

  /* At the end, close the queue. */
  /* We should not hit this code unless our earlier socket is closed. */
  nfq_destroy_queue (qh);
  must ("nfq_close", nfq_close (h) == 0);

  exit (0);
}

/* Demonstration program of reading packet trace files recorded by pcap
 * (used by tshark and tcpdump) and dumping out some corresponding information
 * in a human-readable form.
 *
 * Note, this program is limited to processing trace files that contains
 * UDP packets.  It prints the timestamp, source port, destination port,
 * and length of each such packet.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <net/if.h>
#include <netinet/if_ether.h>

#define APP_NAME		"sniffex"
#define APP_DESC		"Sniffer example using libpcap"
#define APP_COPYRIGHT	"Copyright (c) 2005 The Tcpdump Group"
#define APP_DISCLAIMER	"THERE IS ABSOLUTELY NO WARRANTY FOR THIS PROGRAM."

#include <pcap.h>
#include <cctype>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* We've included the UDP header struct for your ease of customization.
 * For your protocol, you might want to look at netinet/tcp.h for hints
 * on how to deal with single bits or fields that are smaller than a byte
 * in length.
 *
 * Per RFC 768, September, 1981.
 */
struct UDP_hdr {
    u_short	uh_sport;		/* source port */
    u_short	uh_dport;		/* destination port */
    u_short	uh_ulen;		/* datagram length */
    u_short	uh_sum;			/* datagram checksum */
};


/* Some helper functions, which we define at the end of this file. */

/* Returns a string representation of a timestamp. */
const char *timestamp_string(struct timeval ts);

/* Report a problem with dumping the packet with the given timestamp. */
void problem_pkt(struct timeval ts, const char *reason);

/* Report the specific problem of a packet being too short. */
void too_short(struct timeval ts, const char *truncated_hdr);

/* dump_UDP_packet()
 *
 * This routine parses a packet, expecting Ethernet, IP, and UDP headers.
 * It extracts the UDP source and destination port numbers along with the UDP
 * packet length by casting structs over a pointer that we move through
 * the packet.  We can do this sort of casting safely because libpcap
 * guarantees that the pointer will be aligned.
 *
 * The "ts" argument is the timestamp associated with the packet.
 *
 * Note that "capture_len" is the length of the packet *as captured by the
 * tracing program*, and thus might be less than the full length of the
 * packet.  However, the packet pointer only holds that much data, so
 * we have to be careful not to read beyond it.
 */
void dump_UDP_packet(const unsigned char *packet, struct timeval ts,
                     unsigned int capture_len)
{
    struct ip *ip;
    struct UDP_hdr *udp;
    unsigned int IP_header_length;
    
    /* For simplicity, we assume Ethernet encapsulation. */
    
    if (capture_len < sizeof(struct ether_header))
    {
        /* We didn't even capture a full Ethernet header, so we
         * can't analyze this any further.
         */
        too_short(ts, "Ethernet header");
        return;
    }
    
    /* Skip over the Ethernet header. */
    packet += sizeof(struct ether_header);
    capture_len -= sizeof(struct ether_header);
    
    if (capture_len < sizeof(struct ip))
    { /* Didn't capture a full IP header */
        too_short(ts, "IP header");
        return;
    }
    
    ip = (struct ip*) packet;
    IP_header_length = ip->ip_hl * 4;	/* ip_hl is in 4-byte words */
    
    if (capture_len < IP_header_length)
    { /* didn't capture the full IP header including options */
        too_short(ts, "IP header with options");
        return;
    }
    
    if (ip->ip_p != IPPROTO_UDP)
    {
        problem_pkt(ts, "non-UDP packet");
        return;
    }
    
    /* Skip over the IP header to get to the UDP header. */
    packet += IP_header_length;
    capture_len -= IP_header_length;
    
    if (capture_len < sizeof(struct UDP_hdr))
    {
        too_short(ts, "UDP header");
        return;
    }
    
    udp = (struct UDP_hdr*) packet;
    
    printf("%s UDP src_port=%d dst_port=%d length=%d\n",
           timestamp_string(ts),
           ntohs(udp->uh_sport),
           ntohs(udp->uh_dport),
           ntohs(udp->uh_ulen));
}

/* default snap length (maximum bytes per packet to capture) */
#define SNAP_LEN 1518

/* ethernet headers are always exactly 14 bytes [1] */
#define SIZE_ETHERNET 14

/* Ethernet addresses are 6 bytes */
#define ETHER_ADDR_LEN	6

/* Ethernet header */
struct sniff_ethernet {
    u_char  ether_dhost[ETHER_ADDR_LEN];    /* destination host address */
    u_char  ether_shost[ETHER_ADDR_LEN];    /* source host address */
    u_short ether_type;                     /* IP? ARP? RARP? etc */
};

/* IP header */
struct sniff_ip {
    u_char  ip_vhl;                 /* version << 4 | header length >> 2 */
    u_char  ip_tos;                 /* type of service */
    u_short ip_len;                 /* total length */
    u_short ip_id;                  /* identification */
    u_short ip_off;                 /* fragment offset field */
#define IP_RF 0x8000            /* reserved fragment flag */
#define IP_DF 0x4000            /* dont fragment flag */
#define IP_MF 0x2000            /* more fragments flag */
#define IP_OFFMASK 0x1fff       /* mask for fragmenting bits */
    u_char  ip_ttl;                 /* time to live */
    u_char  ip_p;                   /* protocol */
    u_short ip_sum;                 /* checksum */
    struct  in_addr ip_src,ip_dst;  /* source and dest address */
};
#define IP_HL(ip)               (((ip)->ip_vhl) & 0x0f)
#define IP_V(ip)                (((ip)->ip_vhl) >> 4)

/* TCP header */
typedef u_int tcp_seq;

struct sniff_tcp {
    u_short th_sport;               /* source port */
    u_short th_dport;               /* destination port */
    tcp_seq th_seq;                 /* sequence number */
    tcp_seq th_ack;                 /* acknowledgement number */
    u_char  th_offx2;               /* data offset, rsvd */
#define TH_OFF(th)      (((th)->th_offx2 & 0xf0) >> 4)
    u_char  th_flags;
#define TH_FIN  0x01
#define TH_SYN  0x02
#define TH_RST  0x04
#define TH_PUSH 0x08
#define TH_ACK  0x10
#define TH_URG  0x20
#define TH_ECE  0x40
#define TH_CWR  0x80
#define TH_FLAGS        (TH_FIN|TH_SYN|TH_RST|TH_ACK|TH_URG|TH_ECE|TH_CWR)
    u_short th_win;                 /* window */
    u_short th_sum;                 /* checksum */
    u_short th_urp;                 /* urgent pointer */
};

void
got_packet(u_char *args, const struct pcap_pkthdr *header, const u_char *packet);

void
print_payload(const u_char *payload, int len);

void
print_hex_ascii_line(const u_char *payload, int len, int offset);

void
print_app_banner(void);

void
print_app_usage(void);

/*
 * app name/banner
 */
void
print_app_banner(void)
{
    
    printf("%s - %s\n", APP_NAME, APP_DESC);
    printf("%s\n", APP_COPYRIGHT);
    printf("%s\n", APP_DISCLAIMER);
    printf("\n");
    
    return;
}

/*
 * print help text
 */
void
print_app_usage(void)
{
    
    printf("Usage: %s [interface]\n", APP_NAME);
    printf("\n");
    printf("Options:\n");
    printf("    interface    Listen on <interface> for packets.\n");
    printf("\n");
    
    return;
}

/*
 * print data in rows of 16 bytes: offset   hex   ascii
 *
 * 00000   47 45 54 20 2f 20 48 54  54 50 2f 31 2e 31 0d 0a   GET / HTTP/1.1..
 */
void
print_hex_ascii_line(const u_char *payload, int len, int offset)
{
    
    int i;
    int gap;
    const u_char *ch;
    
    /* offset */
    printf("%05d   ", offset);
    
    /* hex */
    ch = payload;
    for(i = 0; i < len; i++) {
        printf("%02x ", *ch);
        ch++;
        /* print extra space after 8th byte for visual aid */
        if (i == 7)
            printf(" ");
    }
    /* print space to handle line less than 8 bytes */
    if (len < 8)
        printf(" ");
    
    /* fill hex gap with spaces if not full line */
    if (len < 16) {
        gap = 16 - len;
        for (i = 0; i < gap; i++) {
            printf("   ");
        }
    }
    printf("   ");
    
    /* ascii (if printable) */
    ch = payload;
    for(i = 0; i < len; i++) {
        if (isprint(*ch))
            printf("%c", *ch);
        else
            printf(".");
        ch++;
    }
    
    printf("\n");
    
    return;
}

/*
 * print packet payload data (avoid printing binary data)
 */
void
print_payload(const u_char *payload, int len)
{
    
    int len_rem = len;
    int line_width = 16;			/* number of bytes per line */
    int line_len;
    int offset = 0;					/* zero-based offset counter */
    const u_char *ch = payload;
    
    if (len <= 0)
        return;
    
    /* data fits on one line */
    if (len <= line_width) {
        print_hex_ascii_line(ch, len, offset);
        return;
    }
    
    /* data spans multiple lines */
    for ( ;; ) {
        /* compute current line length */
        line_len = line_width % len_rem;
        /* print line */
        print_hex_ascii_line(ch, line_len, offset);
        /* compute total remaining */
        len_rem = len_rem - line_len;
        /* shift pointer to aaremaining bytes to print */
        ch = ch + line_len;
        /* add offset */
        offset = offset + line_width;
        /* check if we have line width chars or less */
        if (len_rem <= line_width) {
            /* print last line and get out */
            print_hex_ascii_line(ch, len_rem, offset);
            break;
        }
    }
    
    return;
}

/*
 * dissect/print packet
 */
void
got_packet(u_char *args, const struct pcap_pkthdr *header, const u_char *packet)
{
    
    static int count = 1;                   /* packet counter */
    
    /* declare pointers to packet headers */
    const struct sniff_ethernet *ethernet;  /* The ethernet header [1] */
    const struct sniff_ip *ip;              /* The IP header */
    const struct sniff_tcp *tcp;            /* The TCP header */
    const u_char *payload;                    /* Packet payload */
    
    int size_ip;
    int size_tcp;
    int size_payload;
    
    printf("\nPacket number %d:\n", count);
    count++;
    
    /* define ethernet header */
    ethernet = (struct sniff_ethernet*)(packet);
    
    /* define/compute ip header offset */
    ip = (struct sniff_ip*)(packet + SIZE_ETHERNET);
    size_ip = IP_HL(ip)*4;
    if (size_ip < 20) {
        printf("   * Invalid IP header length: %u bytes\n", size_ip);
        return;
    }
    
    /* print source and destination IP addresses */
    printf("       From: %s\n", inet_ntoa(ip->ip_src));
    printf("         To: %s\n", inet_ntoa(ip->ip_dst));
    
    /* determine protocol */
    switch(ip->ip_p) {
        case IPPROTO_TCP:
            printf("   Protocol: TCP\n");
            break;
        case IPPROTO_UDP:
            dump_UDP_packet(packet, header->ts, header->caplen);
            printf("   Protocol: UDP\n");
            return;
        case IPPROTO_ICMP:
            printf("   Protocol: ICMP\n");
            return;
        case IPPROTO_IP:
            printf("   Protocol: IP\n");
            return;
        default:
            printf("   Protocol: unknown\n");
            return;
    }
    
    /*
     *  OK, this packet is TCP.
     */
    
    /* define/compute tcp header offset */
    tcp = (struct sniff_tcp*)(packet + SIZE_ETHERNET + size_ip);
    size_tcp = TH_OFF(tcp)*4;
    if (size_tcp < 20) {
        printf("   * Invalid TCP header length: %u bytes\n", size_tcp);
        return;
    }
    
    printf("   Src port: %d\n", ntohs(tcp->th_sport));
    printf("   Dst port: %d\n", ntohs(tcp->th_dport));
    
    /* define/compute tcp payload (segment) offset */
    payload = (u_char *)(packet + SIZE_ETHERNET + size_ip + size_tcp);
    
    /* compute tcp payload (segment) size */
    size_payload = ntohs(ip->ip_len) - (size_ip + size_tcp);
    
    /*
     * Print payload data; it might be binary, so don't just
     * treat it as a string.
     */
    if (size_payload > 0) {
        printf("   Payload (%d bytes):\n", size_payload);
        print_payload(payload, size_payload);
    }
    
    return;
}

/* This part is in C++ */
#include <iostream>
#include <string>
#include <cstring>
#include <sstream>
#include <unordered_map>

std::unordered_map<std::string, std::string> global_process_hash_table;
void build_hash_table(const char* pStr) {
	std::stringstream stringStream(pStr);
	std::string line;
	std::string local_ip = "130.245.188.149:";

	// only insert ports for localhost
	while(std::getline(stringStream, line)) {
	    std::size_t prev = 0, pos=0;
		// find position for first pattern after which source ip and port starts
	    if ((prev = line.find("TCP ", prev)) == std::string::npos && (prev = line.find("UDP ", prev)) == std::string::npos)
			continue;
		prev +=  strlen("TCP ");
		// find position for second pattern before which source port ends
		if ((pos = line.find("->", prev)) == std::string::npos &&  (pos = line.find(" ", prev)) == std::string::npos)
			continue;
		// if localhost or local ip string is found, trim them
		if (line.find("localhost:", prev) == prev)
			prev += strlen("localhost:");
		else if (line.find(local_ip, prev) == prev)
			prev += local_ip.length();
		else
			continue;
		if (pos <= prev)
			continue;
		// else we ignore other ip
		std::string token_key = line.substr(prev, pos-prev);
		bool hasDestIP = false;
		std::string second_key = "";
		if (pos+1 <  line.length() &&  line[pos+1] == '>')
			hasDestIP = true;
		if (! hasDestIP)
			goto no_second_key;
		prev = pos+2;
		
		// set pos and get dest port similarly for mapping
		// to get the second port, we go the same way verify ip
		if ((pos = line.find(" ", prev)) == std::string::npos)
			goto no_second_key;
		// std::cout<<"str from prev " << line.substr(prev)<<std::endl;
		if (line.find("localhost:", prev) == prev)
			prev += strlen("localhost:");
		else if (line.find(local_ip, prev) == prev)
			prev += local_ip.length();
		else
			goto no_second_key;
		
		second_key = line.substr(prev, pos-prev);
		
no_second_key:	
		// get process name
		if ((pos = line.find(" ", 0)) == std::string::npos)
			continue;
		std::string token_val = line.substr(0, pos);
		/* std::cout<<" " << token_key << " ->  " << token_val << std::endl; 
		std::pair<std::unordered_map<std::string, std::string>::iterator, bool> */
		auto res = global_process_hash_table.emplace(token_key, token_val);
		if (res.second)
			std::cout<< "Key " << token_key << " successfully inserted." << std::endl;
		else
			std::cout<< "Key " << token_key << " already exist!" << std::endl;
		if (second_key != "") {
			res = global_process_hash_table.emplace(second_key, token_val);
			if (res.second)
				std::cout<< "Second key " << second_key << " successfully inserted." << std::endl;
			else
				std::cout<< "Second key " << second_key << " already exist!" << std::endl;
		}
		// hasDestIP
    }
	/*
	not expected
    if (prev < line.length())
        //wordVector.push_back(line.substr(prev, std::string::npos));
		std::cout<<"we got token:  " << line.substr(prev, std::string::npos) << std::endl;
	}*/
	
	for (auto kv: global_process_hash_table) {
		std::cout<<" " << kv.first << " ->  " << kv.second << std::endl;		
	}
}

int get_process_mapping() {
	long lSize;
	// FILE *pFile = popen("/bin/ls -l /Users/musicapp/", "r");
	FILE *pFile = popen("lsof -i", "r");
	if (!pFile)
		return -1;
	fseek (pFile , 0 , SEEK_END);
	lSize = ftell (pFile);
	rewind (pFile);
	
	// char* buffer = new char[lSize+1];
	char* buffer = (char *) malloc(lSize+1);
	size_t result = fread(buffer, 1, lSize, pFile);
	buffer[lSize] = '\0';
	printf("%s\n", buffer);
	/* line_p = fgets(buffer, sizeof(buffer), pFile);
	printf("%s", line_p); */
	pclose(pFile);
	build_hash_table(buffer);
	puts("============================================");	
	free(buffer);
	return 0;
}

int main(int argc, char *argv[]) {
    /*
    pcap_t *pcap;
    const unsigned char *packet;
    char errbuf[PCAP_ERRBUF_SIZE];
    struct pcap_pkthdr header;
    ++argv; --argc;
    
    if ( argc != 1 )
    {
        fprintf(stderr, "program requires one argument, the trace file to dump\n");
        exit(1);
    }
    
    pcap = pcap_open_offline(argv[0], errbuf);
    if (pcap == NULL)
    {
        fprintf(stderr, "error reading pcap file: %s\n", errbuf);
        exit(1);
    }
    
    while ((packet = pcap_next(pcap, &header)) != NULL)
        dump_UDP_packet(packet, header.ts, header.caplen);
    
    // terminate
     */
	if (get_process_mapping()<0)
		return 0;
	
    char *dev = NULL;			/* capture device name */
    char errbuf[PCAP_ERRBUF_SIZE];		/* error buffer */
    pcap_t *handle;				/* packet capture handle */
    
    char filter_exp[] = "ip";		/* filter expression [3] */
    struct bpf_program fp;			/* compiled filter program (expression) */
    bpf_u_int32 mask;			/* subnet mask */
    bpf_u_int32 net;			/* ip */
    int num_packets = 10;			/* number of packets to capture */
    
    print_app_banner();
    
    /* check for capture device name on command-line */
    if (argc == 2) {
        dev = argv[1];
    }
    else if (argc > 2) {
        fprintf(stderr, "error: unrecognized command-line options\n\n");
        print_app_usage();
        exit(EXIT_FAILURE);
    }
    else {
        /* find a capture device if not specified on command-line */
        dev = pcap_lookupdev(errbuf);
        if (dev == NULL) {
            fprintf(stderr, "Couldn't find default device: %s\n",
                    errbuf);
            exit(EXIT_FAILURE);
        }
    }
    
    /* get network number and mask associated with capture device */
    if (pcap_lookupnet(dev, &net, &mask, errbuf) == -1) {
        fprintf(stderr, "Couldn't get netmask for device %s: %s\n",
                dev, errbuf);
        net = 0;
        mask = 0;
    }
    
    /* print capture info */
    printf("Device: %s\n", dev);
    printf("Number of packets: %d\n", num_packets);
    printf("Filter expression: %s\n", filter_exp);
    
    /* open capture device */
    handle = pcap_open_live(dev, SNAP_LEN, 1, 1000, errbuf);
    if (handle == NULL) {
        fprintf(stderr, "Couldn't open device %s: %s\n", dev, errbuf);
        exit(EXIT_FAILURE);
    }
    
    /* make sure we're capturing on an Ethernet device [2] */
    if (pcap_datalink(handle) != DLT_EN10MB) {
        fprintf(stderr, "%s is not an Ethernet\n", dev);
        exit(EXIT_FAILURE);
    }
    
    /* compile the filter expression */
    if (pcap_compile(handle, &fp, filter_exp, 0, net) == -1) {
        fprintf(stderr, "Couldn't parse filter %s: %s\n",
                filter_exp, pcap_geterr(handle));
        exit(EXIT_FAILURE);
    }
    
    /* apply the compiled filter */
    if (pcap_setfilter(handle, &fp) == -1) {
        fprintf(stderr, "Couldn't install filter %s: %s\n",
                filter_exp, pcap_geterr(handle));
        exit(EXIT_FAILURE);
    }
    
    /* now we can set our callback function */
    pcap_loop(handle, num_packets, got_packet, NULL);
    
    /* cleanup */
    pcap_freecode(&fp);
    pcap_close(handle);
    
    printf("\nCapture complete.\n");
    
    return 0;
}


/* Note, this routine returns a pointer into a static buffer, and
 * so each call overwrites the value returned by the previous call.
 */
const char *timestamp_string(struct timeval ts)
{
    static char timestamp_string_buf[256];
    
    sprintf(timestamp_string_buf, "%d.%06d",
            (int) ts.tv_sec, (int) ts.tv_usec);
    
    return timestamp_string_buf;
}

void problem_pkt(struct timeval ts, const char *reason)
{
    fprintf(stderr, "%s: %s\n", timestamp_string(ts), reason);
}

void too_short(struct timeval ts, const char *truncated_hdr)
{
    fprintf(stderr, "packet with timestamp %s is truncated and lacks a full %s\n",
            timestamp_string(ts), truncated_hdr);
}

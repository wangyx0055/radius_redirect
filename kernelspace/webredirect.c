/*
  * Web -- a simple web redirect module
  * Authors
  *		jxq19881013@gmail.com
  */

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include "webredirect.h"
#include "json_parser.h"

MODULE_LICENSE("GPL");

//store all configure
struct ConfigSet gConfigSet;
JsonTask_t gJsonTask;

//insert a url redirect entry to url map list
static u_int32_t url_redirect_entry_insert(urlRedirectEntry_t *new_entry) {
	write_lock(&gConfigSet.redirect_url_list_rwlock);
	list_add_tail_rcu(&(new_entry->list), &gConfigSet.redirect_url_list_head);
	write_unlock(&gConfigSet.redirect_url_list_rwlock);
	return 0;
}

//delete a url redirect entry from url map list
static u_int32_t url_redirect_entry_del(urlRedirectEntry_t *url_redirect_entry) {
	
	if (url_redirect_entry == NULL) {
		return 1;
	}

	write_lock(&gConfigSet.redirect_url_list_rwlock);
	list_del_rcu(&(url_redirect_entry->list));
	write_unlock(&gConfigSet.redirect_url_list_rwlock);
	return 0;
}


/*
  *get url redirect entry by src url
  *0: no match found*/
static urlRedirectEntry_t* url_redirect_entry_search_by_src_url(u_int8_t *url, uint32_t urlLen) {
	urlRedirectEntry_t *entry = NULL;

	if(list_empty(&gConfigSet.redirect_url_list_head)) {
		return NULL;
	}

	read_lock(&gConfigSet.redirect_url_list_rwlock);
	list_for_each_entry(entry, &gConfigSet.redirect_url_list_head, list) {
		if (urlLen == entry->srcUrl.urlLen && memcmp(url, entry->srcUrl.url, urlLen) == 0) {
			read_unlock(&gConfigSet.redirect_url_list_rwlock);
			return entry;
		}
	}

	read_unlock(&gConfigSet.redirect_url_list_rwlock);
	return NULL;
}

static u_int32_t url_redirect_map_free(void) {
	urlRedirectEntry_t *entry = NULL;
	urlRedirectEntry_t *next = NULL;

	//do not use list_for_each_entry when you need to delete an entry
	list_for_each_entry_safe(entry, next, &gConfigSet.redirect_url_list_head, list) {
		if (entry != NULL) {
			write_lock(&gConfigSet.redirect_url_list_rwlock);
			list_del_rcu(&(entry->list));
			write_unlock(&gConfigSet.redirect_url_list_rwlock);
			kfree(entry->srcUrl.url);
			kfree(entry->dstUrl.url);
			kfree(entry);
		}
	}
	return 0;
}

static inline void url_redirect_entry_init(urlRedirectEntry_t *urlRedirectEntry)
{
	urlRedirectEntry->action = ADD_URL_FILTER;
	urlRedirectEntry->srcUrl.url = NULL;
	urlRedirectEntry->srcUrl.urlLen = 0;
	urlRedirectEntry->dstUrl.url = NULL;
	urlRedirectEntry->dstUrl.urlLen = 0;
}

urlRedirectEntry_t * malloc_new_url_redirect_entry(void)
{
	urlRedirectEntry_t *urlRedirectEtnry = kzalloc(sizeof(urlRedirectEntry_t), GFP_ATOMIC);
	
	if ( unlikely( NULL == urlRedirectEtnry ) ) {
		return NULL;
	}

	url_redirect_entry_init(urlRedirectEtnry);
	return urlRedirectEtnry;
}

void setup_url_redirect_entry(urlRedirectEntry_t *urlRedirectEtnry, ACTION_TYPE action, 
	uint8_t *srcUrl, uint32_t srcUrlLen, uint8_t *dstUrl, uint32_t dstUrlLen)
{
	urlRedirectEtnry->action = action;
	urlRedirectEtnry->srcUrl.url = kzalloc(srcUrlLen, GFP_ATOMIC);
	memcpy(urlRedirectEtnry->srcUrl.url, srcUrl, srcUrlLen);
	urlRedirectEtnry->srcUrl.urlLen = srcUrlLen;
	web_redirect_build_url(dstUrl, &urlRedirectEtnry->dstUrl);
}

void setup_url_redirect_entry_map(void)
{
#if 1
	// we need parse the configfile to initial the url list
	uint8_t srcUrl1[] = "192.168.19.141/";
	uint8_t dstUrl1[] = "www.baidu.com";
	uint8_t srcUrl2[] = "www.3600.com/";
	uint8_t dstUrl2[] = "203.195.196.55/html/football/index.htm";
	uint8_t srcUrl3[] = "www.hao123.com/";
	uint8_t dstUrl3[] = "203.195.196.55/html/news/index.htm";
	urlRedirectEntry_t *urlRedirectEtnry = NULL;
	urlRedirectEntry_t *urlRedirectEtnry2 = NULL;
	urlRedirectEntry_t *urlRedirectEtnry3 = NULL;

	urlRedirectEtnry = malloc_new_url_redirect_entry();
	setup_url_redirect_entry(urlRedirectEtnry, ADD_URL_FILTER, srcUrl1, strlen(srcUrl1), dstUrl1, strlen(dstUrl1));
	url_redirect_entry_insert(urlRedirectEtnry);

	urlRedirectEtnry2 = malloc_new_url_redirect_entry();
	setup_url_redirect_entry(urlRedirectEtnry2, ADD_URL_FILTER, srcUrl2, strlen(srcUrl2), dstUrl2, strlen(dstUrl2));
	url_redirect_entry_insert(urlRedirectEtnry2);

	urlRedirectEtnry3 = malloc_new_url_redirect_entry();
	setup_url_redirect_entry(urlRedirectEtnry3, ADD_URL_FILTER, srcUrl3, strlen(srcUrl3), dstUrl3, strlen(dstUrl3));
	url_redirect_entry_insert(urlRedirectEtnry3);

	gConfigSet.url_number = 3;
////////////////////////////////////////
#endif
#if 0
	uint32_t i = 0;
	uint32_t j = 0;
	urlRedirectEntry_t *urlRedirectEtnry = NULL;
	JsonTask_t dstUrlgJsonTask = gJsonTask;
	for (i = 0; i < gJsonTask.jsonTaskDataNum; i++)
	{
		urlRedirectEtnry = malloc_new_url_redirect_entry();
		for (j = 0; j < gJsonTask.JsonTaskData[i].jsonTargetListNum; j++) {
			setup_url_redirect_entry(urlRedirectEtnry, ADD_URL_FILTER, gJsonTask.JsonTaskData[i].szSrcUrl, strlen(gJsonTask.JsonTaskData[i].szSrcUrl), dstUrlgJsonTask.JsonTaskData[i].JsonTargetList[j].url, strlen(dstUrlgJsonTask.JsonTaskData[i].JsonTargetList[j].url));
			url_redirect_entry_insert(urlRedirectEtnry);
		}
	}
#endif
}


/* ================================================================================ */	

int web_redirect_init(void)
{
	memset(&gConfigSet, 0, sizeof(struct ConfigSet));
	gConfigSet.check_interval = 300; //five minutes
	//initial the redirect url list
	INIT_LIST_HEAD(&gConfigSet.redirect_url_list_head);
	// init the rw lock for url list
	rwlock_init(&gConfigSet.redirect_url_list_rwlock);
	
	setup_url_redirect_entry_map();
	return 0;
}

void web_redirect_deinit(void)
{
	//release the memory
	url_redirect_map_free();
}

int web_redirect_build_url( const char *szUrl, urlEntry_t *dstUrlEntry )
{
	int ret = -1;
	uint8_t *szHead = NULL, *szBuf = NULL;
	uint32_t ulHeadLen = 0;

	if (dstUrlEntry == NULL)
		goto out;

	if ( ( szHead = (uint8_t *)kzalloc(PATH_MAX, GFP_ATOMIC) ) == NULL ) {
		printk("Web  http url alloc failed\n");
		goto out;
	}

	ulHeadLen = snprintf(szHead, PATH_MAX, http_redirect_header, szUrl);

	if ( ( szBuf = (uint8_t *)kzalloc(ulHeadLen, GFP_ATOMIC) ) == NULL) {
		printk("Web  url head alloc failed\n");
		goto out;
	}

	dstUrlEntry->url = szBuf;
	dstUrlEntry->urlLen = ulHeadLen;

	memcpy(szBuf, szHead, ulHeadLen);

	ret = 0;
	
out:
	if (szHead)
		kfree(szHead);
	return ret;
}

int skb_iphdr_init( struct sk_buff *skb, uint8_t protocol,
 
                    uint32_t saddr, uint32_t daddr, uint32_t ip_len )
{
	struct iphdr *iph = NULL;

	// Keep ip head room
	skb_push( skb, sizeof(struct iphdr) );
    skb_reset_network_header( skb );
    iph = ip_hdr( skb );

	iph->version  = 4;
    iph->ihl      = 5;
    iph->tos      = 0;
    iph->tot_len  = htons( ip_len );
    iph->id       = 0;
    iph->frag_off = htons(IP_DF);
    iph->ttl      = 64;
    iph->protocol = protocol;
    iph->check    = 0;
    iph->saddr    = saddr;
    iph->daddr    = daddr;
    iph->check    = ip_fast_csum( ( unsigned char * )iph, iph->ihl );
	return 0;
}

struct sk_buff *web_redirect_new_pkt(uint32_t saddr, uint32_t daddr, 
										uint16_t sport, uint16_t dport,
								        uint32_t seq, uint32_t ack_seq,
        								uint8_t *msg, uint32_t len)
{
	struct sk_buff *skb = NULL;
    uint32_t total_len, eth_len, ip_len, tcp_len, header_len;    
    struct tcphdr *th;
	__wsum tcp_hdr_csum;

	// Caculate the pkt length
	tcp_len = len + sizeof( struct tcphdr );
    ip_len = tcp_len + sizeof( struct iphdr );
	eth_len = ip_len + ETH_HLEN;
	total_len = eth_len + NET_IP_ALIGN;
    total_len += LL_MAX_HEADER;
	header_len = total_len - len;

	if (( skb = alloc_skb(total_len, GFP_ATOMIC) ) == NULL) {
		printk("Web  skb alloc failed\n");
		return NULL;
	}

	// Keep head room
	skb_reserve(skb, header_len);

	// Copy skb data
	skb_copy_to_linear_data( skb, msg, len );
    skb->len += len;

	// Push Tcp head
	skb_push( skb, sizeof( *th ) );
    skb_reset_transport_header( skb );
    th = tcp_hdr( skb );

	memset(th, 0, sizeof(struct tcphdr));

	th->doff = 5;
	th->source = sport;
	th->dest = dport;
	th->seq = seq;
	th->ack_seq = ack_seq;
	th->urg_ptr = 0;
	th->psh = 1;
	th->ack = 1;
	th->window = htons(63857);
	th->check = 0;

	tcp_hdr_csum = csum_partial( th, tcp_len, 0 );
    th->check = csum_tcpudp_magic( saddr, daddr,
            tcp_len, IPPROTO_TCP, tcp_hdr_csum );
 
    skb->csum=tcp_hdr_csum;                        
 
    if ( th->check == 0 )
        th->check = CSUM_MANGLED_0;

	skb_iphdr_init( skb, IPPROTO_TCP, saddr, daddr, ip_len );
	return skb;
}

int web_redirect_send_pkt(struct sk_buff *skb, struct iphdr *iph,
        						struct tcphdr *th, urlEntry_t *dstUrlEntry)
{
	int ret = -1;
	uint32_t ulTcpLen = 0, ulAckSeq = 0;
	struct sk_buff *pskb = NULL;
	struct ethhdr *eth = NULL;
	struct vlan_hdr *vhdr = NULL;

	ulTcpLen = ntohs(iph->tot_len) - ((iph->ihl + th->doff) << 2);
	ulAckSeq = htonl(ntohl(th->seq) + ulTcpLen);

	if (( pskb = web_redirect_new_pkt(iph->daddr, iph->saddr,
                					th->dest, th->source, 
                					th->ack_seq, ulAckSeq,
                					dstUrlEntry->url, dstUrlEntry->urlLen) ) == NULL)
		goto out;

	// Copy VLAN info
	if ( __constant_htons(ETH_P_8021Q) == skb->protocol ) {
 
        vhdr = (struct vlan_hdr *)skb_push(pskb, VLAN_HLEN );
 
        vhdr->h_vlan_TCI = vlan_eth_hdr(skb)->h_vlan_TCI;
 
        vhdr->h_vlan_encapsulated_proto = __constant_htons(ETH_P_IP);
 
    }

	// Rebuild ethernet head
	eth = (struct ethhdr *) skb_push(pskb, ETH_HLEN);
    skb_reset_mac_header(pskb);
	pskb->protocol  = eth_hdr(skb)->h_proto;
    eth->h_proto    = eth_hdr(skb)->h_proto;
    memcpy( eth->h_source, eth_hdr(skb)->h_dest, ETH_ALEN);
    memcpy( eth->h_dest, eth_hdr(skb)->h_source, ETH_ALEN );

	// Send out the pkt
	if ( skb->dev ) {
        pskb->dev = skb->dev;       
        dev_queue_xmit( pskb );
        ret = 0;
{
		char *szPopup = web_popup_header(DEFAULT_REDIRECT_URL);
		printk(">>> build popup %s\n", szPopup);
		if (szPopup)
			kfree(szPopup);
}
    } else {
        kfree_skb( pskb );
        printk( "skb dev is NULL/n" );
    }
out:
	return ret;
}

int web_redirect(struct sk_buff *skb, struct iphdr *iph, struct tcphdr *tcph, urlRedirectEntry_t* urlRedirectEntry)
{
	int ret = 0;
	
	urlEntry_t *pTmp = &urlRedirectEntry->dstUrl;
	//rcu_read_lock();
	ret = web_redirect_send_pkt(skb, iph, tcph, pTmp);
	//rcu_read_unlock();
	
	return ret;
}

char * get_host_from_http(struct sk_buff *skb, uint32_t *host_len)
{
	char *tcp_data = (unsigned char *)eth_hdr(skb) + sizeof(struct ethhdr) + sizeof(struct iphdr)+ sizeof(struct tcphdr);
	char *http_get = tcp_data + 4;
	char *pNext = strstr(tcp_data, " HTTP/1.");
	int get_len  = 0;
	char *tmp = NULL;
	char *http_host = NULL;
	char *tmp1 = NULL;
	
	if (pNext == NULL)
			return NULL;
	 //*pNext = '\0';
	 
	 get_len = pNext - http_get;
	 tmp = strstr(pNext+1, "Host: ");
	 
	 if (tmp == NULL)
			return NULL;
	 http_host = tmp + 6;
	 tmp1 = strchr(http_host, '\r');
	 if (tmp1 == NULL)
			return NULL;
	 //*tmp1 = '\0';
	 *host_len = tmp1 - http_host;
	 return http_host;

}

char * get_uri_from_http(struct sk_buff *skb, uint32_t *uri_len)
{

	char *http_uri = NULL;
	char *tcp_data = (unsigned char *)eth_hdr(skb) + sizeof(struct ethhdr) + sizeof(struct iphdr)+ sizeof(struct tcphdr);		 
	char *pNext = strstr(tcp_data, " HTTP/1.");

	if (pNext == NULL) {
		 return NULL;
	}

	*pNext = '\0';
	http_uri = tcp_data + 4;
	*uri_len = pNext - http_uri;

	return http_uri;
}


static unsigned int hook_pkt_in(unsigned   int   hooknum,    
									struct   sk_buff   *skb,    
									const   struct   net_device   *inDev,    
									const   struct   net_device   *outDev,    
									int   (*okfn)(struct   sk_buff   *))
{
	struct ethhdr *eth = eth_hdr(skb);
	struct iphdr *iph = ip_hdr(skb);
	struct tcphdr *tcph = NULL;

	unsigned int sip, dip;
    unsigned short source, dest;
    unsigned char *payload;
    int plen = 0;
	int32_t host_len = 0;
	int32_t uri_len = 0;
	char *http_host =  NULL;
	char *http_uri = NULL;
	char *http_url = NULL;

	urlRedirectEntry_t* urlRedirectEntry = NULL;

	if (!skb || !eth || !iph)
		return NF_ACCEPT;

	if (skb->pkt_type == PACKET_BROADCAST)
		return NF_ACCEPT;

	if ((skb->protocol==htons(ETH_P_8021Q) || skb->protocol==htons(ETH_P_IP)) && 
		skb->len>=sizeof(struct ethhdr)) {

		if (skb->protocol==htons(ETH_P_8021Q))
			iph = (struct iphdr *)((uint8_t *)iph+4);
		
		/* IPv4 demand */
		if(iph->version != IP_VERSION_4)
			return NF_ACCEPT;

		/* SKb length too short */
		if (skb->len < IP_HEAD_LEN)
			return NF_ACCEPT;
		
		/* Skb  length inspection*/
		if ((iph->ihl * 4) > skb->len ||
			skb->len < ntohs(iph->tot_len) ||
			(iph->frag_off & htons(0x1fff)) != 0)
			return NF_ACCEPT;

		sip = iph->saddr;
		dip = iph->daddr;

		if (iph->protocol ==  IPPROTO_TCP) {
			tcph = (struct tcphdr *)((unsigned char *)iph+iph->ihl*4);
			source = ntohs(tcph->source);
			dest = ntohs(tcph->dest);

			/* Pass DNS pkt */
			if(dest == 53 || source == 53)
                return NF_ACCEPT;

			/* HTTP data length */
			plen = ntohs(iph->tot_len) - iph->ihl*4 - tcph->doff*4;

			if (plen > 10 && dest == 80) {
				/* HTTP data field */
				payload = (unsigned char *)tcph + tcph->doff*4;
				/* HTTP GET request pkt */
				if (memcmp(payload, "GET ", 4) == 0) {
					//TODO: generate a gerenal funcation to get information from http get packets
					http_host =  get_host_from_http(skb, &host_len);
					http_uri = get_uri_from_http(skb, &uri_len);

					if (http_host == NULL)
						return NF_ACCEPT;
					http_url = kzalloc(host_len+uri_len+1, GFP_ATOMIC);
					if (NULL == http_url)
						return NF_ACCEPT;
					strncpy(http_url, http_host, host_len);
					if (NULL != http_uri) {
						strncat(http_url, http_uri, uri_len);
					}
					printk("%s,%d URL:%s\n", __func__, __LINE__, http_url);
					//TODO: we can check the url list whether there it is the request url is in the redirect url list
					urlRedirectEntry = url_redirect_entry_search_by_src_url(http_url, strlen(http_url));
					if (NULL != urlRedirectEntry) {
						web_redirect(skb, iph, tcph, urlRedirectEntry);
					}
					kfree(http_url);
				}
			}
		}
	}
	return   NF_ACCEPT;
}

static unsigned int hook_pkt_out(unsigned int hooknum, 
									struct sk_buff *skb,
									const struct net_device *inDev,
									const struct net_device *outDev,
									int (*okfn)(struct sk_buff *))
{	
	return NF_ACCEPT;
}

static uint8_t s_webRedirectMiscId = WEB_REDIRECT_MISC_DEV_MAJOR;

static int webRedirectMiscOpen(struct inode *pNode, struct file *pFile)
{
			//MOD_INC_USE_COUNT;
				return 0;
}

static int webRedirectMiscClose(struct inode *pNode, struct file *pFile)
{
		   //MOD_DEC_USE_COUNT;
		      return 0;
}

long webRedirectMiscIoctl(struct file *pFile, 
						unsigned int iCmd, unsigned long ulTag)
{
// process the request
	switch (iCmd)
	{
		case WEB_REDIRECT_IOCTL_PLATFORM_GET:
			//copy_to_user((void*)ulTag, &gJsonTask, sizeof(gJsonTask));
			break;
		case WEB_REDIRECT_IOCTL_PLATFORM_SET:
			printk(".............................\n");
			copy_from_user(&gJsonTask, (void*)ulTag, sizeof(gJsonTask));
			//setup url redirect entry map, we get the configure file from configserver with json packets 
			setup_url_redirect_entry_map();
			break;
	}
	return 0;
}

static struct file_operations s_webRedirectMiscFops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl = webRedirectMiscIoctl,
	.open		= webRedirectMiscOpen,
	.release	= webRedirectMiscClose,
};

#if 0
static struct file_operations s_radiusMiscFops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl = radiusMiscIoctl,
	.open		= radiusMiscOpen,
	.release	= radiusMiscClose,
};
#endif
static struct   nf_hook_ops   nfho[] __read_mostly = 
{
	{
        .hook     = (nf_hookfn *)hook_pkt_in,
        .owner    = THIS_MODULE,
        .pf       = PF_INET,
        .hooknum  = NF_INET_PRE_ROUTING,
        .priority = NF_IP_PRI_FIRST,
    },
    {
        .hook     = (nf_hookfn *)hook_pkt_out,
        .owner    = THIS_MODULE,
        .pf       = PF_INET,
        .hooknum  = NF_INET_LOCAL_OUT,
        .priority = NF_IP_PRI_FIRST,
    },
};

int init_module(void)
{
	memset(&gJsonTask, 0, sizeof(gJsonTask));
	register_chrdev(s_webRedirectMiscId, WEB_REDIRECT_MISC_NAME, &s_webRedirectMiscFops);
	printk("WEB_REDIRECT_MISC_NAME:%s\n", WEB_REDIRECT_MISC_NAME);
	web_redirect_init();

	//register_chrdev(s_webRedirectMiscId, "radius", &s_readiusMiscFops);

	nf_register_hooks(nfho, ARRAY_SIZE(nfho));
	
    printk("Web redirect load success\n");
	
	return 0;
}

void cleanup_module(void)
{
	nf_unregister_hooks(nfho, ARRAY_SIZE(nfho));
	web_redirect_deinit();
	unregister_chrdev(s_webRedirectMiscId, WEB_REDIRECT_MISC_NAME);
}

MODULE_DESCRIPTION("Web  redirect the web request to specific server!\n");
MODULE_ALIAS("A simple http redirecter");

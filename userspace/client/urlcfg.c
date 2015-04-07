/*************************************************************************
    > File Name: user.c
    > Author: ICKelin
    > Mail: 18277973721@sina.cn 
    > Created Time: 2015年04月03日 星期五 16时49分41秒
 ************************************************************************/

#include<stdio.h>
#include <unistd.h>
#include <errno.h>
#include <error.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <linux/netlink.h>
#include <linux/socket.h>
#include <sys/types.h>
#include <asm/types.h>
#define CONFIG_URL "../../config/url.conf"

#define NETLINK_URL_CONFIG 25
#define NETLINK_RADIUS_KERNEL 26
#define MAX_PAYLOAD 512
#define error(msg)\
	{fprintf(stderr,"%s error with:%s\n", msg, strerror(errno));exit(-1);}
//地址结构
struct sockaddr_nl nl_src_addr, nl_dest_addr;

//消息报头
struct nlmsghdr *nlh = NULL;
//IO
struct iovec iov;
int nl_fd;
//消息体
struct msghdr nl_msg;

void init_nl()
{
	nl_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_URL_CONFIG);
	if(nl_fd<0)
	{
		perror("socket");
		exit(-1);
	}
	memset(&nl_msg, 0, sizeof(nl_msg));

	memset(&nl_src_addr, 0, sizeof(nl_src_addr));
	nl_src_addr.nl_family = AF_NETLINK;
	nl_src_addr.nl_pid = getpid();
	nl_src_addr.nl_groups = 0;
#if 0
	if(bind(nl_fd,(struct sockaddr*)&nl_src_addr, sizeof(nl_src_addr))<0)
	{
		perror("bind");
		exit(-1);
	}
#endif
	memset(&nl_dest_addr, 0, sizeof(nl_dest_addr));
	nl_dest_addr.nl_family = AF_NETLINK;
	nl_dest_addr.nl_pid = 0;
	
}

void sendnlmsg(char *message)
{
	nlh = (struct nlmsghdr*)malloc(NLMSG_SPACE(MAX_PAYLOAD));
	nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_flags = 0;

	strcpy((char*)NLMSG_DATA(nlh), message);
	iov.iov_base = (void*)nlh;
	iov.iov_len = nlh->nlmsg_len;
	nl_msg.msg_name = (void*)&nl_dest_addr;
	nl_msg.msg_namelen = sizeof(nl_dest_addr);
	nl_msg.msg_iov = &iov;
	nl_msg.msg_iovlen = 1;

	fprintf(stderr,"Start to send message.");
	if(sendmsg(nl_fd, &nl_msg, 0)<0)
	{
		perror("sendmsg");
		exit(-1);
	}
}

int main(int argc, char **argv)
{
	FILE *fp = NULL;

	if(argc != 2)
		return 0;
//	init_nl();

//	sendnlmsg(argv[1]);
	fp = fopen(CONFIG_URL, "a+");
	if(fp == NULL)
		error("fopen");
	fprintf(fp, "%s\n", (char*)&(argv[1][1]));
	fclose(fp);
}

#include <stdio.h>
#include "function.h"
#include "tools.h"
#include "getlanhost.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <setjmp.h>
#include <errno.h>
#include <sys/select.h>
#include <fcntl.h>

#define PACKET_SIZE 1024
#define NEXT_PING_TIME 180
struct lanhost *g_lanhost =NULL;

/* 计算校验和的算法 */
unsigned short cal_chksum(unsigned short *addr,int len)
{
	int sum=0;
	int nleft = len;
	unsigned short *w = addr;
	unsigned short answer = 0;
	/* 把ICMP报头二进制数据以2字节为单位累加起来 */
	while(nleft > 1){
		sum += *w++;
		nleft -= 2;
	}
	/*
	 * 若ICMP报头为奇数个字节，会剩下最后一字节。
	 * 把最后一个字节视为一个2字节数据的高字节，
	 * 这2字节数据的低字节为0，继续累加
	 */
	if(nleft == 1){
		*(unsigned char *)(&answer) = *(unsigned char *)w;
		sum += answer;    /* 这里将 answer 转换成 int 整数 */
	}
	sum = (sum >> 16) + (sum & 0xffff);        /* 高位低位相加 */
	sum += (sum >> 16);        /* 上一步溢出时，将溢出位也加到sum中 */
	answer = ~sum;             /* 注意类型转换，现在的校验和为16位 */
	return answer;
}
int livetest(char* ip) {

	char    sendpacket[PACKET_SIZE]={0};    /* 发送的数据包 */
	//char    recvpacket[PACKET_SIZE]={0};    /* 接收的数据包 */
	int    datalen = 56;    /* icmp数据包中数据的长度 */
	struct protoent *protocol=NULL;
	int sockfd;
	pid_t pid=getpid();
	int size = 16*1024;

	if(!ip)  return 0;
	protocol = getprotobyname("icmp");
	if((sockfd = socket(AF_INET, SOCK_RAW, protocol->p_proto)) < 0) {
		perror("socket error");
	}
	setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size) );

	struct sockaddr_in dest_addr;
	bzero(&dest_addr, sizeof(dest_addr));
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_addr.s_addr = inet_addr(ip);
	//send packet;
	int packsize;
	struct icmp *icmp=NULL;
	struct timeval *tval=NULL;
	icmp = (struct icmp*)sendpacket;
	icmp->icmp_type = ICMP_ECHO;    /* icmp的类型 */
	icmp->icmp_code = 0;            /* icmp的编码 */
	icmp->icmp_cksum = 0;           /* icmp的校验和 */
	icmp->icmp_seq = 1;       /* icmp的顺序号 */
	icmp->icmp_id = pid;            /* icmp的标志符 */
	packsize = 8 + datalen;   /* icmp8字节的头 加上数据的长度(datalen=56), packsize = 64 */
	tval = (struct timeval *)icmp->icmp_data;    /* 获得icmp结构中最后的数据部分的指针 */
	gettimeofday(tval, NULL); /* 将发送的时间填入icmp结构中最后的数据部分 */
	icmp->icmp_cksum = cal_chksum((unsigned short *)icmp, packsize);/*填充发送方的校验和*/

	if(sendto(sockfd, sendpacket, packsize, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0){
		perror("sendto error");
	}
	printf("send to %s, send done\n",ip );
	fcntl(sockfd, F_SETFL, O_NONBLOCK);
	struct timeval timeo = {1,0};
	fd_set set;
	FD_ZERO(&set);
	FD_SET(sockfd, &set);
	//read , write;
	int retval = select(sockfd+1, &set, NULL, NULL, &timeo);
	if(retval == -1) {
		printf("select error\n");
		close(sockfd);
		return 0;
	}else if(retval == 0 ) {
		close(sockfd);
		printf("timeout\n");
		return 0;
	}else{
		if( FD_ISSET(sockfd, &set) ){
			close(sockfd);
			printf("host is live\n");
			return 1;
		}
	}
	// n = recvfrom(sockfd, recvpacket,sizeof(recvpacket), 0, (struct sockaddr *)&from, (socklen_t *)&fromlen);
	// if(n<0) {
	//     perror("recvfrom error");
	// }else{
	//     printf("%d\n",n);
	// }
	//return 0;
}

void update_devname_by_mac(char *mac,char *devname)
{
	struct lanhost *tmp;
	char namenew[32]={0};
	if(!mac)
		return;
	if(!devname || 0==strcmp(devname,"*")){
		strcpy(namenew,"unkown");
	}else {
		strcpy(namenew,devname);
	}
	tmp = g_lanhost;
	pthread_rwlock_wrlock(&rwlock);      //释放写锁
	while(tmp){
		printf("mac:%s,tmp->mac:%s]\n",mac,tmp->mac);
		if(strcmp(tmp->mac,mac) == 0){
			strcpy(tmp->devname,namenew);
			break;
		}
		tmp = tmp->next;
	}
	pthread_rwlock_unlock(&rwlock);      //释放写锁
	return;
}
int del_lanhost(struct lanhost *del)
{
	struct lanhost *ptr;
	struct lanhost *p=NULL;
	ptr = g_lanhost;
	p=g_lanhost;
	pthread_rwlock_wrlock(&rwlock); 
	if(g_lanhost == del){
		printf("I will del lanhost %s\n",g_lanhost->mac);
		g_lanhost = g_lanhost->next;
		pthread_rwlock_unlock(&rwlock);      //释放写锁
		free(del);
		return 0;
	}
	while(ptr){
		p = ptr;
		ptr = ptr->next;
		if(ptr==del){
			printf("I will del lanhost %s\n",ptr->mac);
			p->next = ptr->next;
			free(del);
			break;
		}
	}
	pthread_rwlock_unlock(&rwlock);      //释放写锁
	return 0;
}

int add_lanhost(struct lanhost *new)
{
	struct lanhost *p=NULL;
	struct lanhost *p_up=NULL;
	pthread_rwlock_wrlock(&rwlock);      //写者加写锁
	if(!g_lanhost){
		g_lanhost=new;
		printf("I will add lanhost %s\n",g_lanhost->mac);
	}else{
		p= g_lanhost;
		p_up = g_lanhost;
		while(p){
			if(strcmp(p->mac,new->mac)==0 ){
				if(strcmp(p->ip,new->ip) !=0 ){
					strcpy(p->ip,new->ip);
				}
				p->stat = 0;
				free(new);
				pthread_rwlock_unlock(&rwlock);      //释放写锁
				return 0;
			}
			p_up=p;
			p=p->next;
		}
		//add a new one
		p_up->next = new;
		pthread_rwlock_unlock(&rwlock);      //释放写锁
		printf("I will add new lanhost %s\n",new->mac);
	}

	return 0;
}

void del_lanhost_by_stat()
{

	struct lanhost *tmp = g_lanhost;
	struct lanhost *p = g_lanhost;
	pthread_rwlock_wrlock(&rwlock);
	while(g_lanhost->stat == 1){
		free(tmp);
		g_lanhost = g_lanhost->next;
		tmp = g_lanhost;
	}
	p = g_lanhost;
	tmp = g_lanhost->next;
	while(tmp){
		if(tmp->stat == 1)
		{
			//del me
			p->next = tmp->next;
			printf("i will del mac:%s\n",tmp->mac);
			free(tmp);
			tmp = p->next;

			continue;
		}
		p = tmp;
		tmp = tmp->next;
	}
	pthread_rwlock_unlock(&rwlock);      //释放写锁
	return;
}

void reset_stat()
{
	struct lanhost *tmp = g_lanhost;
	pthread_rwlock_wrlock(&rwlock);
	while(tmp){
		tmp->stat = 1;
		tmp = tmp->next;
	}
	pthread_rwlock_unlock(&rwlock);      //释放写锁
	return;
}

//每隔3分钟向所有下挂设备发送一次ping，收到回复说明下挂设备还在，否则删除此下挂设备
void* thread_update_lanhost(void *arg)
{
	time_t now=0;
	FILE *fd=NULL;
	char buf[256]={0};
	struct lanhost *tmp=NULL;
	while(1){
		memset(buf,0,sizeof(buf));
		reset_stat();
		fd=fopen("/proc/net/arp","r");
		//fd=fopen("./arp","r");
		if(!fd){
			printf("open arp file fail!\n");
			sleep(10);
			continue;
		}
		fgets(buf,sizeof(buf),fd);
		while(fgets(buf,sizeof(buf),fd)){
			if(strstr(buf,"br-lan")){
				tmp=malloc(sizeof(struct lanhost));
				if(!tmp){
					printf("struct lanhost malloc fail!!\n");
					continue;
				}
				memset(tmp,0,sizeof(struct lanhost));
				sscanf(buf,"%s%*s%*s%s",tmp->ip,tmp->mac);
				printf("%s,%s\n",tmp->ip,tmp->mac);
				add_lanhost(tmp);
				tmp=NULL;
			}
		}
		fclose(fd);
		del_lanhost_by_stat();
		//then ping all ip
		tmp = g_lanhost;
		while(tmp){
			time(&now);
			if(1 == livetest(tmp->ip)){
				if(strlen(tmp->offline) || strlen(tmp->online)==0 ){  //当前记录是下线状态,或从没上线过
					tmp->online_t = now;
					tmp->offline[0]='\0';
					time_to_str(now,tmp->online,sizeof(tmp->online));
				}
			}else{//下线
				if(strlen(tmp->online)){ //上线过
					if(now- tmp->online_t > 1800 )//下线超过30分钟
						del_lanhost(tmp);
					else {
						if(strlen(tmp->offline) == 0)
							time_to_str(now,tmp->offline,sizeof(tmp->offline));
					}
				}else{
					//del this recode
					del_lanhost(tmp);
				}
			}
			tmp=tmp->next;
		}

		//then find dev name from dhcp.release
		fd = fopen("/tmp/dhcp.leases","r");
		//fd = fopen("./dhcp.lease","r");
		if(!fd){ //打不开此文件就找不到devname，直接进入下一个循环
			printf("cant read /tmp/dhcp.leases!!!\n");
			sleep(NEXT_PING_TIME);
			continue;
		}
		char mac[20]={0};
		char devname[32]={0};
		while(fgets(buf,sizeof(buf),fd)){
			sscanf(buf,"%*s%s%*s%s",mac,devname);
			//getNthValueSafe(1,buf,' ',mac,sizeof(mac));
			//getNthValueSafe(3,buf,' ',devname,sizeof(devname));
			printf("mac:%s,name:%s\n",mac,devname);
			update_devname_by_mac(mac,devname);
		}
		fclose(fd);fd=NULL;
		//next cycle

		sleep(NEXT_PING_TIME);
	}
}

int get_lanhost(char *res,int len)
{
	struct lanhost *tmp;
	json_object *my_arr=NULL;
	json_object *my_lanhost=NULL;
	tmp = g_lanhost;

	if(res && len)
	{
		my_arr = json_object_new_array();
		if(!tmp)
		{
			strncpy(res,json_object_to_json_string(my_arr),len);
			json_object_put(my_arr);
			return 0;
		}
		pthread_rwlock_rdlock(&rwlock); 
		while(tmp){
			my_lanhost = json_object_new_array();
			json_object_array_add(my_lanhost, json_object_new_string(tmp->mac));
			json_object_array_add(my_lanhost,json_object_new_string(tmp->ip));
			json_object_array_add(my_lanhost, json_object_new_string(tmp->online));
			json_object_array_add(my_lanhost, json_object_new_string(tmp->offline));
			json_object_array_add(my_lanhost, json_object_new_string(tmp->devname));
			json_object_array_add(my_lanhost,json_object_new_int(tmp->type));
			json_object_array_add(my_arr,my_lanhost);
			tmp= tmp->next;
		}	
		pthread_rwlock_unlock(&rwlock);
		strncpy(res,json_object_to_json_string(my_arr),len);
		eat_space(res);
		json_object_put(my_arr);

	}
	return 0;

}


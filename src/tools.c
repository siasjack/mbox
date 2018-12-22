#include <stdio.h> 
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h> //for struct ifreq
#include <errno.h> 
#include <sys/stat.h>
#include <fcntl.h> 
#include <termios.h> 
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <bits/wordsize.h>  //32bit or 64bit

#include <json/json.h>
#include <mtd/mtd-user.h>
#include <stdarg.h>
#include <net/route.h> 
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include "function.h"
#include "md5.h"

#define jdebug(x,fmt,arg...) printf(fmt,##arg)

struct dev_flow{
	char flow_up[32];
	char flow_down[32];
	long long int up_;
	long long int down_;
	long long int last_up_;
	long long int last_down_;

};
enum
{
	DEBUG_DBG,
	DEBUG_INFO,
	DEBUG_STATE,
	DEBUG_WARN,
	DEBUG_ERR,
	DEBUG_MAX
};

union {
	int number;
	char s;
}Endian;

int isBigEndian() {
	Endian.number = 0x01000002;
	return (Endian.s == 0x01);
}

/*
 * substitution of getNthValue which dosen't destroy the original value
 */
int getNthValueSafe(int index, char *value, char delimit, char *result, int len) 
{
	int i=0, result_len=0;
	char *begin, *end;

	if(!value || !result || !len)
		return -1; 

	begin = value;
	end = strchr(begin, delimit);

	while(i<index && end){
		begin = end+1;
		end = strchr(begin, delimit);
		i++; 
	}    

	//no delimit
	if(!end){
		if(i == index){
			end = begin + strlen(begin);
			result_len = (len-1) < (end-begin) ? (len-1) : (end-begin);
		}else
			return -1; 
	}else
		result_len = (len-1) < (end-begin)? (len-1) : (end-begin);

	memcpy(result, begin, result_len );
	*(result+ result_len ) = '\0';

	return 0;
}
int GetValByKey(json_object * jobj, const  char  *sname,enum json_type type_me,void *rev)
{
	json_object *pval = NULL;
	enum json_type type;
	if(rev == NULL) 
		return -1;

	json_object_object_get_ex(jobj, sname,&pval);
	if(NULL!=pval){
		type = json_object_get_type(pval);
		if(type_me != type)
			return -2;
		switch(type)
		{
			case    json_type_string:
				//jdebug(DEBUG_STATE,"Key:%s  value: %s\n", sname, json_object_get_string(pval));
				memcpy((char*)rev,json_object_get_string(pval),strlen(json_object_get_string(pval)));
				*(char*)(rev+strlen(json_object_get_string(pval))) = '\0';
				break;
			case    json_type_int:
				//jdebug(DEBUG_STATE,"Key:%s  value: %d\n", sname, json_object_get_int(pval));
				*(int*)rev = json_object_get_int(pval);
				break;
			case json_type_array:
				//jdebug(DEBUG_STATE,"Key:%s  value: %d\n", sname, json_object_get_int(pval));
				break;
			default:
				jdebug(DEBUG_STATE,"json type is not handle,type=%d\n",type);
				break;
		}
	} else {
		jdebug(DEBUG_STATE,"%s:%s in json string not found!\n",__FUNCTION__,sname);
		return -1;
	}   
	return SUCCESS;
}
void eat_enter(char *str)
{
	char *tmp = NULL;
	if(!str)
		return;
	tmp = str;
	while(*tmp)
	{   
		if(*tmp == '\n')
		{   
			*tmp = '\0';
			break;
		}   
		tmp++;
	}
}

long int file_len2(char* filename)
{
	struct stat statbuf;  
	if(0!=stat(filename,&statbuf))
		return -1;
	long int size=statbuf.st_size;  

	return size;
}


long int file_len(char* filename)
{
	FILE *fp=fopen(filename,"r");  
	if(!fp) return -1;  
	fseek(fp,0L,SEEK_END);  
	int size=ftell(fp);
	fclose(fp);  

	return size;
}

int write2file(const char*filename,char *data,int len)
{
	FILE* fd;
	int ret=0;
	if(!filename)
		return -1;
	fd = fopen(filename,"w");
	if(!fd){
		jdebug(DEBUG_ERR,"open file %s err!\n",filename);
		return -1;
	}
	ret = fwrite(data,1,len,fd);
	fclose(fd);
	return ret;
}
int get_dev_mac(const char* szDevName ,unsigned char *mac)
{
	struct ifreq ifr;
	if(!mac || !szDevName)
		return -1;          
	int s = socket(AF_INET, SOCK_DGRAM, 0); 
	if (s < 0)
	{   
		jdebug(DEBUG_ERR,"socket create is err:%d\n",s);
		return -1;                  
	}   

	strcpy(ifr.ifr_name, szDevName);
	if (ioctl(s, SIOCGIFHWADDR, &ifr) < 0)
	{   
		close(s);
		jdebug(DEBUG_ERR,"ioctl is err:%d\n",s);
		return -1; 
	}   
	memcpy(mac,ifr.ifr_hwaddr.sa_data,6);
	close(s);
	return 0;
}

void copy_mac2str(unsigned char *mac,char *str)
{
	if(!mac || !str)
		return ;
	sprintf(str,"%02x%02x%02x%02x%02x%02x",
			mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
	return;
}
void get_ap_mac(char *mac)
{
	unsigned char mac6[8]={0};
	//if(0 == get_dev_mac("br-lan",mac6))//get success
	if(0 == get_dev_mac("eth0",mac6))//get success
	{
		copy_mac2str(mac6,mac);
	}
	else
	{
		strcpy(mac,"");
	}

	return;
}

/**
 *获取设备外网口
 *return 1 is ok!
 **/

int get_ext_iface(char *device)
{
	FILE *input=NULL;
	char dest[16]={0};
	char dev[16]={0};
	if(!device )
		return -1;
	input = fopen("/proc/net/route", "r");
	while (!feof(input)) {
		/* XXX scanf(3) is unsafe, risks overrun */
		if ((fscanf(input, "%s %s %*s %*s %*s %*s %*s %*s %*s %*s %*s\n", dev, dest) == 2) && strcmp(dest, "00000000") == 0) {
			strcpy(device,dev);
			jdebug(DEBUG_STATE, "Detected %s as the default interface!\n", device);
			fclose(input);
			return SUCCESS;
		}
		memset(dev,0,sizeof(dev));
		memset(dest,0,sizeof(dest));
	}
	fclose(input);
	return ERR_NOTFOUND;
}

static int get_addr(char *ifname, char *addr, int flag)
{
	int sockfd = 0;
	struct sockaddr_in *sin;
	struct ifreq ifr;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("socket error!\n");
		return FALSE;
	}

	memset(&ifr, 0, sizeof(ifr));
	snprintf(ifr.ifr_name, (sizeof(ifr.ifr_name) - 1), "%s", ifname);

	if(ioctl(sockfd, flag, &ifr) < 0 )
	{
		perror("ioctl error!\n");
		close(sockfd);
		return FALSE;
	}
	close(sockfd);

	if (SIOCGIFHWADDR == flag){
		memcpy((void *)addr, (const void *)&ifr.ifr_ifru.ifru_hwaddr.sa_data, 6);
		/*printf("mac address: %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n", addr[0],addr[1],addr[2],addr[3],addr[4],addr[5]);*/
	}else{
		sin = (struct sockaddr_in *)&ifr.ifr_addr;
		snprintf((char *)addr, 16, "%s", inet_ntoa(sin->sin_addr));
	}

	return SUCCESS;
}

int get_ip(char *ifname,char ip[16])
{
	return get_addr(ifname,ip, SIOCGIFADDR);
}

int get_netmask(char *ifname,char ip[16])
{
	return get_addr(ifname,ip, SIOCGIFNETMASK);
}

int get_mac(char *ifname,unsigned char addr[6])
{
	return get_addr(ifname,addr, SIOCGIFHWADDR);
}

int get_gateway(char *ip)
{    
	FILE *fp;    
	char buf[256]={0}; // 128 is enough for linux    
	char iface[16]={0};    
	unsigned long long dest_addr=0, gate_addr=0;
	int a[4]={0};

	fp = fopen("/proc/net/route", "r");    
	if (fp == NULL)    
		return ERR_OPEN_FILE; 
	/* Skip title line */    
	fgets(buf, sizeof(buf), fp);    
	while (fgets(buf, sizeof(buf), fp)) {    
		if (sscanf(buf, "%s\t%llX\t%llX", iface,&dest_addr, &gate_addr) != 3 || dest_addr != 0)
			continue;
		if(!isBigEndian){
			a[0] = gate_addr>>24;
			a[1] = (gate_addr&0x00ffffff)>>16;
			a[2] = (gate_addr&0x0000ffff)>>8;
			a[3] = (gate_addr&0x000000ff);
		}else{
			a[3] = gate_addr>>24;
			a[2] = (gate_addr&0x00ffffff)>>16;
			a[1] = (gate_addr&0x0000ffff)>>8;
			a[0] = (gate_addr&0x000000ff);

		}
		snprintf(ip,16,"%d.%d.%d.%d",a[0],a[1],a[2],a[3]);
		break;
	}    

	fclose(fp);
	return 0;
}
//atleast 32 byte dns1
int get_dns(char *dns1)
{
	FILE *fd = NULL;
	char buf[128]={0};
	int save=0;
	int p=0;
	if(!dns1 )  return ERR_IN_PARA;
	fd =fopen("/tmp/resolv.conf.auto","r");
	if(!fd){
		return ERR_OPEN_FILE;
	}
	while(fgets(buf,sizeof(buf),fd)){
		if(buf[0] == '#')	continue;
		if(strstr(buf,"nameserver") && save<2)
		{
			if(save==0)
			{
				getNthValueSafe(1,buf,' ',dns1,16);
				eat_enter(dns1);
				dns1[strlen(dns1)]=' ';
				p = strlen(dns1) ;
			}else if(save==1 ){
				getNthValueSafe(1,buf,' ',dns1+p,16);
				eat_enter(dns1);
			}
			save++;
		}
	}
	fclose(fd);
	return SUCCESS;

}
int is_valid_ip(char ipaddr[16])
{
	int ret = 0;
	struct in_addr inp;
	memset(&inp,0,sizeof(struct in_addr));
	ret = inet_aton(ipaddr, &inp);
	if (0 == ret)
	{
		return FALSE;
	}
	else
	{
		printf("inet_aton:ip=%u\n",ntohl(inp.s_addr));
	}

	return TRUE;
}

/*
 * 先验证是否为合法IP，然后将掩码转化成32无符号整型，取反为000...00111...1，
 * 然后再加1为00...01000...0，此时为2^n，如果满足就为合法掩码
 *
 * */
int is_valid_netmask(char netmask[16])
{
	unsigned int b = 0, i, n[4]={0};
	if(is_valid_ip(netmask) > 0)
	{
		sscanf(netmask, "%u.%u.%u.%u", &n[3], &n[2], &n[1], &n[0]);
		for(i = 0; i < 4; ++i) //将子网掩码存入32位无符号整型
			b += n[i] << (i * 8);
		b = ~b + 1;
		if((b & (b - 1)) == 0) //判断是否为2^n
			return TRUE;
	}

	return FALSE;
}

static int set_addr(char *ifname,char ip[16], int flag)
{
	struct ifreq ifr;
	struct sockaddr_in sin;
	int sockfd;

	if (is_valid_ip(ip) < 0)
	{
		printf("ip was invalid!\n");
		return ERR_IN_PARA;
	}

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sockfd == -1){
		fprintf(stderr, "Could not get socket.\n");
		perror("eth0\n");
		return ERR_OPEN_FILE;
	}
	memset(&ifr,0,sizeof(struct ifreq));
	snprintf(ifr.ifr_name, (sizeof(ifr.ifr_name) - 1), "%s",ifname);

	/* Read interface flags */
	if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
		fprintf(stderr, "ifdown: shutdown ");
		perror(ifr.ifr_name);
		return ERR_IOCTRL;
	}

	memset(&sin, 0, sizeof(struct sockaddr));
	sin.sin_family = AF_INET;
	inet_aton(ip, &sin.sin_addr);
	memcpy(&ifr.ifr_addr, &sin, sizeof(struct sockaddr));
	if (ioctl(sockfd, flag, &ifr) < 0){
		fprintf(stderr, "Cannot set IP address. ");
		perror(ifr.ifr_name);
		return ERR_IOCTRL;
	}

	return SUCCESS;
}

int set_ip_netmask(char *ifname,unsigned char ip[16])
{
	return set_addr(ifname,(char *)ip, SIOCSIFNETMASK);
}

int set_ip(char *ifname,unsigned char ip[16])
{
	return set_addr(ifname,(char*)ip, SIOCSIFADDR);
}



int set_gateway(char ip[16])
{
	int sockFd;
	struct sockaddr_in sockaddr;
	struct rtentry rt;

	if (is_valid_ip(ip) < 0)
	{
		printf("gateway was invalid!\n");
		return FALSE;
	}

	sockFd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockFd < 0)
	{
		perror("Socket create error.\n");
		return FALSE;
	}

	memset(&rt, 0, sizeof(struct rtentry));
	memset(&sockaddr, 0, sizeof(struct sockaddr_in));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = 0;
	if(inet_aton(ip, &sockaddr.sin_addr)<0)
	{
		perror("inet_aton error\n" );
		close(sockFd);
		return FALSE;
	}

	memcpy ( &rt.rt_gateway, &sockaddr, sizeof(struct sockaddr_in));
	((struct sockaddr_in *)&rt.rt_dst)->sin_family=AF_INET;
	((struct sockaddr_in *)&rt.rt_genmask)->sin_family=AF_INET;
	rt.rt_flags = RTF_GATEWAY;
	if (ioctl(sockFd, SIOCADDRT, &rt)<0)
	{
		perror("ioctl(SIOCADDRT) error in set_default_route\n");
		close(sockFd);
		return FALSE;
	}

	return TRUE;
}
int get_sys_uptime(char *up)
{
	FILE *uptime=NULL;
	char time[128]={0};

	if(!up) 
		return -1;
	uptime = fopen("/proc/uptime","r");
	if(!uptime) 
		return -1;
	fgets(time,sizeof(time),uptime);
	if(!strlen(time))
	{
		fclose(uptime);
		return -1;
	}   
	fclose(uptime);
	sscanf(time,"%s %*s",up);
	jdebug(DEBUG_STATE,"uptime is %s\n",up);
	return SUCCESS;
}

int get_current_time(char *res)
{
	time_t s = 0;
	struct tm *local = NULL;
	if(!res)
		return -1;
	s=time(NULL); //可行
	local=localtime(&s);
	sprintf(res,"%d-%02d-%02d %02d:%02d:%02d",(1900+local->tm_year),(1+local->tm_mon), local->tm_mday,
			local->tm_hour, local->tm_min, local->tm_sec);
	jdebug(DEBUG_STATE,"current time is %s\n",res);
	return SUCCESS;
}

int get_opversion(char *my_version,int len)
{
	int ret=0;
	FILE *ver=NULL;
	if(!my_version || len<=0) 
		return -1; 
	ver = fopen("/etc/openwrt_version","r");
	if(ver == NULL){
		printf("get my version is err!\n");
		return -1;
	}   
	fgets(my_version,len,ver);
	fclose(ver);
	eat_enter(my_version);
	ret =strlen(my_version);
	jdebug(DEBUG_STATE,"my version is %s\n",my_version);
	return ret;
}

int get_meminfo(int *all,int *free)
{
	if(!all || !free)
		return -1;
	FILE *fd = NULL;
	char tmp[32]={0};
	char buf[128]={0};
	fd = fopen("/proc/meminfo","r");
	if(!fd) return -1;
	while(fgets(buf,sizeof(buf),fd) != NULL){
		if(strstr(buf,"MemTotal:")){
			getNthValueSafe(1,buf,':',tmp,sizeof(tmp));
			*all = atoi(tmp);
		}else if(strstr(buf,"MemFree:")){
			getNthValueSafe(1,buf,':',tmp,sizeof(tmp));
			*free = atoi(tmp);
		}
	}
	fclose(fd);
	return 0;

}

int get_cpuuse(char *cpu_use1,char * cpu_use5,char * cpu_use15)
{
	FILE *fd = NULL;
	if(!cpu_use1 || !cpu_use5||!cpu_use15){
		return -1;	
	}
	char buf[128];
	fd = fopen("/proc/loadavg","r");
	if(!fd) return -2;
	fgets(buf,sizeof(buf),fd);
	if(!strlen(buf)) {
		fclose(fd);
		return -3;
	}
	fclose(fd);
	sscanf(buf,"%s %s %s %*s %*s",cpu_use1,cpu_use5,cpu_use15);
	return 0;
}


int get_wifi_flow(struct dev_flow *flow)
{
	FILE *dev=NULL;
	char buf[512]={0};
	char *ptr=NULL;
	char name[32];
	long long int one_up=0;
	long long int one_down=0;

	dev = fopen("/proc/net/dev","r");
	if(!dev)
	{
		return 0;
	}
	fgets(buf,sizeof(buf),dev);
	fgets(buf,sizeof(buf),dev);
	while(fgets(buf,sizeof(buf),dev) != NULL)
	{
		ptr = buf;
		while(*ptr==' ' || *ptr=='\t')
			ptr++;
		getNthValueSafe(0,ptr,':',name,sizeof(name));

		if(strncmp("wlan",name,4) == 0 || strncmp("ra",name,2) == 0)
		{
			ptr=strchr(ptr,':');
			ptr++;
			sscanf(ptr,"%lld %*s %*s %*s %*s %*s %*s %*s %lld %*s %*s %*s %*s %*s %*s %*s",&one_up,&one_down);
			jdebug(1,"wlan name:[%s],up:%lld,down:%lld\n",name,one_up,one_down);
			flow->up_ += one_up;
			flow->down_ += one_down;
			one_up = one_down= 0;
		}
	}
	fclose(dev);
	return SUCCESS;
}
int get_dev_flow(char *dev_name,struct dev_flow *flow)
{
	FILE *dev=NULL;
	char buf[512]={0};
	char *ptr=NULL;
	//char up[16]={0},down[16]={0};
	char name[32];
	if(!strlen(dev_name))
	{
		strcpy(flow->flow_down,"");
		strcpy(flow->flow_up,"");
		return ERR_IN_PARA;
	}
	dev = fopen("/proc/net/dev","r");
	if(!dev)
	{
		return ERR_OPEN_FILE;
	}
	fgets(buf,sizeof(buf),dev);
	fgets(buf,sizeof(buf),dev);
	while(fgets(buf,sizeof(buf),dev) != NULL)
	{
		ptr = buf;
		while(*ptr==' ' || *ptr=='\t')
			ptr++;
		//      jdebug(DEBUG_STATE,"ptr=%s\n",ptr);
		getNthValueSafe(0,ptr,':',name,sizeof(name));

		if(strcmp(dev_name,name) == 0)
		{
			ptr=strchr(ptr,':');
			ptr++;
			sscanf(ptr,"%lld %*s %*s %*s %*s %*s %*s %*s %lld %*s %*s %*s %*s %*s %*s %*s",&flow->up_,&flow->down_);
			break;
		}
	}
	fclose(dev);
	return SUCCESS;
}

int time_to_str(time_t t,char *res,int res_len)
{
	struct tm *tm_now =NULL;
	if(!t || !res || !res_len)
		return ERR_IN_PARA;

	tm_now = localtime(&t) ;
	snprintf(res,res_len,"%d%02d%02d %d:%d",tm_now->tm_year+1900,tm_now->tm_mon+1,tm_now->tm_mday,tm_now->tm_hour, tm_now->tm_min);
	return 0;
}

char *eat_char_big(char *org,char *dest,int len,char d)
{
	int i=len;
	int j=0,n=0;
	if(!org || !dest || !len){
		return NULL;
	}
	while(i && org[j]!='\0'){
		if(org[j]!= d){
			dest[n++]=org[j];
			i--;
		}
		j++;
	}
	dest[n]='\0';
	return dest;
}
char *eat_char(char *org,char d)
{
	int j=0,n=0;
	if(!org ){
		return NULL;
	}
	while(org[j]!='\0'){
		if(org[j]!= d){
			org[n++]=org[j++];
		}else
			j++;
	}
	org[n]='\0';
	return org;
}
//add by jack 20170624   www.openwrtdl.com
int md5_string(char *string,char *ret_md5)
{
    md5_state_t state;
    md5_byte_t digest[16] = {0};
    int di = 0;
    if(!string ||!ret_md5)
        return -1;
    md5_init(&state);
    md5_append(&state, (const md5_byte_t *)string, strlen(string));
    md5_finish(&state, digest);
    for (di = 0; di < 16; ++di)
        sprintf(ret_md5 + di * 2, "%02x", digest[di]);
    return 0;

}
//add by jack 20181205   www.openwrtdl.com
int md5_file(char *filename,char *ret_md5)
{
    md5_state_t state;
    md5_byte_t digest[16] = {0};
    char *p;
	FILE *fd = NULL;
	long int file_len=0;
	int di = 0;
	if(!filename ||!ret_md5)
		return -1;
	struct stat statbuf;
	memset(&statbuf,0,sizeof(struct stat ));
	if(0!=stat(filename,&statbuf))
		return -1;
	file_len=statbuf.st_size;
	fd =fopen(filename,"r");
	if(!fd) return -2;

	p = (char *)malloc(file_len+1);
	if(!p) {
		fclose(fd);
		return ERR_MEM;
	}
	memset(p,0,file_len+1);
	fread(p,file_len,1,fd);
	fclose(fd);

	md5_init(&state);
	md5_append(&state, (const md5_byte_t *)p,file_len);
	md5_finish(&state, digest);
	free(p);
	for (di = 0; di < 16; ++di)
		sprintf(ret_md5 + di * 2, "%02x", digest[di]);
	
	return 0;
}
char *eat_space(char *org)
{
	return eat_char(org,' ');
}

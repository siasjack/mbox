#ifndef __FUNCTION__H_
#define __FUNCTION__H_

#define MY_VERSION	"V0.0.1"

#define	QOS_ONCE			0  //只发一次，不关心是否broker接受到
#define QOS_ONCE_ATLEAST	1  //至少发一次，确保broker接收到
#define QOS_ONCE_MUST		2  //使用两阶段确认来保证消息的不丢失和不重复。在Qos2情况下，Broker肯定会收到消息，且只收到一次
								//开销最大

enum ERRCODE{
	SUCCESS=0,
	ERR_MEM,
	ERR_IN_PARA,
	ERR_NOTFOUND,
	ERR_IO,
	ERR_PARSE,
	ERR_DUPLICATE,
	ERR_UNKNOWN,
	ERR_OPEN_FILE,
	ERR_WRITE_FILE,
	ERR_ALREADY_BIND,
	ERR_IOCTRL,
	ERR_IPADDR,
	ERR_NETMASK,
	ERR_URL,
	ERR_MD5,
	ERR_SAME_VER,
	ERR_UNMATCH_VER,
	ERR_LAST
};

struct sysinfo{
    char devtype[32];//型号
    char dev_manufacture[32];//厂商名字
    char mac[20];
	char op_version[16];
	char my_version[16];
    int uptime;
    char cpu1[8];  //最近1分钟的平均负载
    char cpu5[8];//5分钟
    char cpu15[8];//15分钟
    int all; //KB
    int free;
    //int network_status;//联网状态
};

struct bind_info{
	char telnum[16];
	char bindtime[20];
	int bind;//0:unbind,1:bind
};
struct network_lan{
	int dhcp_disable;
	char lan_ip[16];
	char lan_nm[16];
	unsigned int dhcp_start;
	unsigned int dhcp_end;
};
struct network_wan{
	char wan_type[8];//0,1,2
    int wan_line_statu;//网线连接状态
    int wan_conn_statu;//网络连接状态
    char wan_ip[16];
    char wan_nm[16];
    char wan_gw[16];
    char wan_dns[32]; // 2 at most
    char wan_pppoe_user[32];
    char wan_pppoe_pwd[32];
};
struct network_info{
	struct network_lan lan;
	struct network_wan wan1;
	struct network_wan wan2;
};

struct wifi_info{
	int band;//24=2.4G,5=5G
	int ch;
	char iface_name[16];//保存匿名的iface名称
	char device[16];
	char hwmode[12];
	char ssid[64];
	char encrypt[16];
	char passwd[32];
	char mode[8];
	char txpower[4];
	int enable;
};
struct mqtt_conf{
	char *host;	
	int port ;
	int ssl;
	char *user;
	char *pwd;
};

extern struct sysinfo g_sys;
extern struct network_info g_network;
extern struct wifi_info g_wifi24;
extern struct mqtt_conf g_mqtt_conf;
extern struct wifi_info g_wifi5;
extern struct bind_info g_bind;

extern char *errmsg[128];
int get_sysinfo(char *res,int len);
int get_bindinfo(char *res,int len);
int set_bindinfo(char *);
int set_wifi_cmd(char *);
int set_updatefw_cmd(char *json);
int set_updateme_cmd(char *json);
int get_server_conf(struct mqtt_conf *conf);
int get_wifiinfo(char *,int);
int set_wan_cmd(char *json);
int set_lan_cmd(char *json);
int get_network_info(char *res,int len);
#endif

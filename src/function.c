#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <malloc.h>
#include <unistd.h>
#include "function.h"
#include <mosquitto.h>
#include "tools.h"
#include <json/json.h>
#include <uci.h>
#include <time.h>

#define MQTT_CONFIG_FILE	"mbox"
#define WIFI_CONFIG_FILE	"wireless"
#define NETWORK_CONFIG_FILE	"network"
#define DHCP_CONFIG_FILE	"dhcp"

struct sysinfo g_sys;
struct network_info g_network;
struct wifi_info g_wifi24;
struct wifi_info g_wifi5;
struct bind_info g_bind;
struct mqtt_conf g_mqtt_conf;
extern struct mosquitto *mosq ;
extern char g_myrtopic[64];

//char errmsg[128]={0};

char *errmsg[128]={
	"ok",
	"内存相关错误",
	"参数错误",
	"数据未发现",
	"IO错误",
	"配置文件解析错误",
	"获取信息错误",
	"未知错误",
	"打开配置文件失败",
	"写入配置文件失败",
	"设备已被绑定",
	"系统操作错误",
	"IP地址错误",
	"子网掩码错误",
	"无法访问的URL",
	"MD5校验错误",
	"相同的版本号，无需升级",
	"版本号不匹配，无法升级"
	}; //这种方法获取错误信息要求所有的错误码都是正值,此方法最有效速度最快
/*
char *get_err_reason(int errcode)
{
	memset(errmsg,0,sizeof(errmsg));
	switch(errcode){
		case SUCCESS:break;
		case ERR_OPEN_FILE:
			strcpy(errmsg,"打开配置文件失败");break;
		case ERR_WRITE_FILE:
			strcpy(errmsg,"写入配置文件失败");break;
	
		case ERR_MALLOC:
			 strcpy(errmsg,"内存申请失败");break;
		case ERR_NO_WIFI:
			strcpy(errmsg,"未发现WIFI信息");break;
		case ERR_IN_PARA:
			strcpy(errmsg,"参数输入错误");break;
		case ERR_UNKOWN:
			strcpy(errmsg,"未知错误");break;
		default:
			strcpy(errmsg,"未知错误,可能需要检查配置文件");break;
	}
	return errmsg;
}
*/
int set_bindinfo(char *telnum)
{
	int ret=0;
	char time_str[20]={0};
	time_t now=time(NULL);
	struct tm *tm_now =NULL;
	struct uci_ptr ptr;
	memset(&ptr,0,sizeof(struct uci_ptr ));
	ptr.package=MQTT_CONFIG_FILE;
	ptr.section="bind";

	if(!telnum)
		return ERR_IN_PARA;
	struct uci_context * _ctx=uci_alloc_context();//申请上下文
	if(strcmp(telnum,"unbind") == 0){
		ptr.option="telnum";
		ptr.value="";
		ret=uci_set(_ctx,&ptr);//写入配置
		if(ret != UCI_OK)
			goto cleanup;
		ptr.option="bindtime";
		ptr.value="";
		ret=uci_set(_ctx,&ptr);//写入配置
		if(ret != UCI_OK) goto cleanup;
		ptr.option="bind";
		ptr.value="0";
		ret=uci_set(_ctx,&ptr);//写入配置
		if(ret != UCI_OK) goto cleanup;

		ret=uci_commit(_ctx,&ptr.p,false);//提交保存更改
		if(ret != UCI_OK) goto cleanup;
		memset(&g_bind,0,sizeof(g_bind));
		goto cleanup;
	}
	if(g_bind.bind == 1)
	{
		ret = ERR_ALREADY_BIND;
		goto cleanup;
	}
	ptr.option="telnum";
	ptr.value=telnum;
	ret=uci_set(_ctx,&ptr);//写入配置
	if(ret != UCI_OK)
		goto cleanup;
	printf("uci set %d\n",ret);

	//set bind time
	tm_now = localtime(&now) ;
	snprintf(time_str,sizeof(time_str),"%d-%d-%d %d:%d",tm_now->tm_year+1900,tm_now->tm_mon+1,tm_now->tm_mday,tm_now->tm_hour, tm_now->tm_min);
	ptr.option="bindtime";
	ptr.value=time_str;
	ret=uci_set(_ctx,&ptr);//写入配置
	if(ret != UCI_OK) goto cleanup;
	ptr.option="bind";
	ptr.value="1";
	ret=uci_set(_ctx,&ptr);//写入配置
	if(ret != UCI_OK) goto cleanup;

	ret=uci_commit(_ctx,&ptr.p,false);//提交保存更改
	if(ret != UCI_OK) goto cleanup;

	strcpy(g_bind.telnum,telnum);
	strcpy(g_bind.bindtime,time_str);
	g_bind.bind = 1;
cleanup:
	uci_unload(_ctx,ptr.p);//卸载包
	uci_free_context(_ctx);//释放上下文
	return ret;
}
int get_server_conf(struct mqtt_conf *conf)
{
	struct uci_element *e;
	struct uci_package * pkg=NULL;
	struct uci_context * ctx=NULL;
	char *tmp=NULL;
	ctx=uci_alloc_context();
	if(!ctx) 
		return ERR_MEM;
	if(!conf){
		return ERR_IN_PARA;
	}
	printf("version:%s\n",VER);
	if(UCI_OK != uci_load(ctx,MQTT_CONFIG_FILE,&pkg))
	{
		return ERR_OPEN_FILE;
	}
	uci_foreach_element(&pkg->sections,e){
		struct uci_section* s = uci_to_section(e);
		if(strcmp(e->name,"server") != 0)
			continue;
		if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"host"))){
			conf->host = (char *)malloc(strlen(tmp)+1);
			memset(conf->host,0,strlen(tmp)+1);
			strcpy(conf->host,tmp);
		}

		if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"pwd"))){
			conf->pwd = (char *)malloc(strlen(tmp)+1);
			memset(conf->pwd,0,strlen(tmp)+1);
			strcpy(conf->pwd,tmp);
		}
		if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"user"))){
			conf->user = (char *)malloc(strlen(tmp)+1);
			memset(conf->user,0,strlen(tmp)+1);
			strcpy(conf->user,tmp);
		}
		if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"port")))
		{
			conf->port = atoi(tmp);
		}
		if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"ssl")))
		{
			conf->ssl = atoi(tmp);
		}
	}
	uci_unload(ctx,pkg);//释放pkg
	uci_free_context(ctx);
	return 0;
}

int get_bindinfo(char *res,int len)
{
	struct uci_element *e;
	struct uci_package * pkg=NULL;
	struct uci_context * ctx=NULL;
	char *telnum,*bindtime,*bind;
	json_object *my_object=NULL;
	ctx=uci_alloc_context();
	if(!ctx) 
		return ERR_MEM;
	if(UCI_OK != uci_load(ctx,MQTT_CONFIG_FILE,&pkg))
	{
		return ERR_OPEN_FILE;
	}
	uci_foreach_element(&pkg->sections,e){
		struct uci_section* s = uci_to_section(e);
		if(strcmp(e->name,"bind") != 0)
			continue;
		if(NULL!=(telnum=(char*)uci_lookup_option_string(ctx,s,"telnum"))){
			strcpy(g_bind.telnum,telnum);
		}
		if(NULL!=(bindtime=(char*)uci_lookup_option_string(ctx,s,"bindtime")))
		{
			strcpy(g_bind.bindtime,bindtime);
		}
		if(NULL!=(bind=(char*)uci_lookup_option_string(ctx,s,"bind")))
		{
			g_bind.bind = atoi(bind);
		}
	}
	uci_unload(ctx,pkg);//释放pkg
	uci_free_context(ctx);
	
	//printf("num:%s,bindtime:%s,bind:%d\n",telnum,bindtime,atoi(bind));
	if(res && len)
	{
		my_object = json_object_new_object();
		json_object_object_add(my_object, "telnum", json_object_new_string(g_bind.telnum));
		json_object_object_add(my_object, "bind_time", json_object_new_string(g_bind.bindtime));
		json_object_object_add(my_object, "bind", json_object_new_int(g_bind.bind));
		strncpy(res,json_object_to_json_string(my_object),len);
		eat_space(res);
		json_object_put(my_object);
	}
	return 0;
}
int get_devinfo( struct sysinfo *ptr)
{
	FILE *fd=NULL;
	char buf[128]={0};

	fd = fopen("/etc/device_info","r");
	if(!fd){
		return -1;	
	}
	while(fgets(buf,sizeof(buf),fd) != NULL){
		if(strstr(buf,"MANUFACTURER")){
			getNthValueSafe(1,buf,'\"',ptr->dev_manufacture,sizeof(ptr->dev_manufacture));
		}else if(strstr(buf,"PRODUCT")){
			getNthValueSafe(1,buf,'\"',ptr->devtype,sizeof(ptr->devtype));
		}
	}
	fclose(fd);
	return 0;
}

int get_sysinfo(char *res,int len)
{
	char tmp[128]={0};
	json_object *my_object=NULL;
	if(!strlen(g_sys.dev_manufacture) || !strlen(g_sys.devtype))
		get_devinfo(&g_sys);
	if(!strlen(g_sys.mac))
		get_ap_mac(g_sys.mac);
	get_sys_uptime(tmp);
	g_sys.uptime = atoi(tmp);
	if(g_sys.op_version[0] == 0)
		get_opversion(g_sys.op_version,sizeof(g_sys.op_version));
	get_cpuuse(g_sys.cpu1,g_sys.cpu5,g_sys.cpu15);
	get_meminfo(&g_sys.all,&g_sys.free);
	if(0 == g_sys.my_version[0])
		strncpy(g_sys.my_version,VER,sizeof(g_sys.my_version)-1);

	if(res && len)
	{
		my_object = json_object_new_object();
		json_object_object_add(my_object, "manufacture", json_object_new_string(g_sys.dev_manufacture));
		json_object_object_add(my_object, "myversion", json_object_new_string(g_sys.my_version));
		json_object_object_add(my_object, "type", json_object_new_string(g_sys.devtype));
		json_object_object_add(my_object, "brlan_mac", json_object_new_string(g_sys.mac));
		json_object_object_add(my_object, "op_version", json_object_new_string(g_sys.op_version));
		json_object_object_add(my_object, "cpu1", json_object_new_string(g_sys.cpu1));
		json_object_object_add(my_object, "cpu5", json_object_new_string(g_sys.cpu5));
		json_object_object_add(my_object, "cpu15", json_object_new_string(g_sys.cpu15));
		json_object_object_add(my_object, "uptime",json_object_new_int(g_sys.uptime));
		json_object_object_add(my_object, "memall",json_object_new_int(g_sys.all));
		json_object_object_add(my_object, "memfree",json_object_new_int(g_sys.free));
		strncpy(res,json_object_to_json_string(my_object),len);
		eat_space(res);
		json_object_put(my_object);
	}
	return 0;
}


int get_wifiinfo(char *res,int len)
{
	struct uci_element *e=NULL;
	struct uci_package * pkg=NULL;
	struct uci_context * ctx=NULL;
	char *band=NULL,*device=NULL,*tmp=NULL,*ch=NULL;
	json_object *my_object=NULL;
	json_object *wifiinfo=NULL;
	json_object *my_object5=NULL;
	ctx=uci_alloc_context();
	if(!ctx) 
		return -1;
	if(UCI_OK != uci_load(ctx,WIFI_CONFIG_FILE,&pkg))
	{
		printf("not find wireless file,r u support wifi!\n");
		uci_free_context(ctx);
		return ERR_NOTFOUND;
	}
	uci_foreach_element(&pkg->sections,e){
		struct uci_section* s = uci_to_section(e);
		if(strcmp(s->type,"wifi-device") == 0){
			tmp=NULL;
			band=NULL;
			if(NULL!=(band=(char*)uci_lookup_option_string(ctx,s,"band")) ||
				NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"hwmode"))){
				printf("band:%s,tmp:[%s]\n",band,tmp);
				if((band && strcasecmp("2.4G",band)==0) || (tmp && strcasecmp("11a",tmp)!=0)){
					//this is default para
					g_wifi24.band=24;
					g_wifi24.enable=1;
					strcpy(g_wifi24.txpower,"100");

					if(NULL!=(ch=(char*)uci_lookup_option_string(ctx,s,"channel")))
						g_wifi24.ch = atoi(ch);
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"disabled")))
						if(atoi(tmp) == 0)
							g_wifi24.enable = 0;
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"txpower"))){
						strcpy(g_wifi24.txpower,tmp);
					}
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"hwmode"))){
						strcpy(g_wifi24.hwmode,tmp);
					}
					strcpy(g_wifi24.device,e->name);
					printf("device:%s,channel:%d,enable:%d\n",g_wifi24.device,g_wifi24.ch,g_wifi24.enable);
				}else if((band && strcasecmp("5G",band)==0) || (tmp && strcasecmp("11a",tmp)==0)){
					g_wifi5.band=5;
					g_wifi5.enable = 1;
					strcpy(g_wifi5.txpower,"100");
					strcpy(g_wifi5.device,e->name);
					if(NULL!=(ch=(char*)uci_lookup_option_string(ctx,s,"channel")))
						g_wifi5.ch = atoi(ch);
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"disabled")))
					{
						if(atoi(tmp) == 0)
							g_wifi5.enable = 0;
					}
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"txpower"))){
						strcpy(g_wifi5.txpower,tmp);
					}
	
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"hwmode"))){
						strcpy(g_wifi5.hwmode,tmp);
					}
					printf("5g device:%s,channel:%d,enable:%d\n",g_wifi5.device,g_wifi5.ch,g_wifi5.enable);
				}
			}
		}else if(strcmp(s->type,"wifi-iface") == 0){
			if(NULL!=(device=(char*)uci_lookup_option_string(ctx,s,"device"))){
				printf("iface device :%s\n",device);
				if(0 == strcmp(device,g_wifi24.device)){//2.4G iface
					//如果存在多个iface，只取第一个
					if(strlen(g_wifi24.ssid) > 0){
						continue;
					}
					strcpy(g_wifi24.iface_name,e->name);
					printf("24g iface name:%s\n",g_wifi24.iface_name);
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"ssid"))){
						strncpy(g_wifi24.ssid,tmp,sizeof(g_wifi24.ssid)-1);
					}
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"mode"))){
						strncpy(g_wifi24.mode,tmp,sizeof(g_wifi24.mode)-1);
					}
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"encryption"))){
						strncpy(g_wifi24.encrypt,tmp,sizeof(g_wifi24.encrypt)-1);
					}
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"key"))){
						strncpy(g_wifi24.passwd,tmp,sizeof(g_wifi24.passwd)-1);
					}
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"disabled")))
						g_wifi24.enable = g_wifi24.enable && !atoi(tmp);

				}else if(0 == strcmp(device,g_wifi5.device)){
					if(strlen(g_wifi5.ssid) > 0){
						continue;
					}
					strcpy(g_wifi5.iface_name,e->name);	
					printf("5g iface name:%s\n",g_wifi5.iface_name);
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"ssid"))){
						strncpy(g_wifi5.ssid,tmp,sizeof(g_wifi5.ssid)-1);
					}
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"mode"))){
						strncpy(g_wifi5.mode,tmp,sizeof(g_wifi5.mode)-1);
					}
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"encryption"))){
						strncpy(g_wifi5.encrypt,tmp,sizeof(g_wifi5.encrypt)-1);
					}
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"key"))){
						strncpy(g_wifi5.passwd,tmp,sizeof(g_wifi5.passwd)-1);
					}
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"disabled")))
						g_wifi5.enable = g_wifi5.enable && !atoi(tmp);
				}
			}
		}
	}
	uci_unload(ctx,pkg);//释放pkg
	uci_free_context(ctx);
	if(res && len)
	{
		wifiinfo=json_object_new_object();
		if(g_wifi24.band == 24){
			printf("find wifi 2.4G\n");
			printf("2.4g:ssid:%s\n",g_wifi24.ssid);
			my_object = json_object_new_object();
			json_object_object_add(my_object, "channle", json_object_new_int(g_wifi24.ch));
			json_object_object_add(my_object, "hwmode", json_object_new_string(g_wifi24.hwmode));
			json_object_object_add(my_object, "ssid", json_object_new_string(g_wifi24.ssid));
			json_object_object_add(my_object, "encrypt", json_object_new_string(g_wifi24.encrypt));
			json_object_object_add(my_object, "passwd", json_object_new_string(g_wifi24.passwd));
			json_object_object_add(my_object, "mode", json_object_new_string(g_wifi24.mode));
			json_object_object_add(my_object, "device", json_object_new_string(g_wifi24.device));
			json_object_object_add(my_object, "txpower", json_object_new_string(g_wifi24.txpower));
			json_object_object_add(my_object, "en", json_object_new_int(g_wifi24.enable));
			json_object_object_add(wifiinfo, "2.4G", my_object);
		}
		if(g_wifi5.band == 5){
			printf("find wifi 5G\n");
			printf("5g:ssid:%s\n",g_wifi5.ssid);
			my_object5 = json_object_new_object();
			json_object_object_add(my_object5, "channle", json_object_new_int(g_wifi5.ch));
			json_object_object_add(my_object5, "hwmode", json_object_new_string(g_wifi5.hwmode));
			json_object_object_add(my_object5, "ssid", json_object_new_string(g_wifi5.ssid));
			json_object_object_add(my_object5, "encrypt", json_object_new_string(g_wifi5.encrypt));
			json_object_object_add(my_object5, "passwd", json_object_new_string(g_wifi5.passwd));
			json_object_object_add(my_object5, "mode", json_object_new_string(g_wifi5.mode));
			json_object_object_add(my_object5, "device", json_object_new_string(g_wifi5.device));
			json_object_object_add(my_object5, "txpower", json_object_new_string(g_wifi5.txpower));
			json_object_object_add(my_object5, "en", json_object_new_int(g_wifi5.enable));
			json_object_object_add(wifiinfo, "5G", my_object5);
		}

		strncpy(res,json_object_to_json_string(wifiinfo),len);
		eat_space(res);
		//if(my_object)	json_object_put(my_object);
		//if(my_object5)	json_object_put(my_object5);
		if(wifiinfo)	json_object_put(wifiinfo);
	}
	return 0;

}

int get_network_info(char *res,int len)
{
	char ext_ifname[16]={0};
	struct uci_element *e;
	struct uci_package * pkg=NULL;
	struct uci_context * ctx=NULL;
	struct uci_section *s=NULL;
	json_object *lan=NULL;
	json_object *wan=NULL;
	json_object *my_object=NULL;
	char *tmp=NULL;
	ctx=uci_alloc_context();
	if(!ctx) 
		return -1;
	if(UCI_OK != uci_load(ctx,NETWORK_CONFIG_FILE,&pkg))
	{
		printf("not find network file\n");
		uci_free_context(ctx);
		return ERR_NOTFOUND;
	}
	memset(&g_network,0,sizeof(g_network));
	uci_foreach_element(&pkg->sections,e){
		s = uci_to_section(e);
		if(strcmp(s->type,"interface") == 0 && strcmp(e->name,"lan") == 0){
			if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"ipaddr")))
			{
				printf("lan ipaddr:%s\n",tmp);
				strcpy(g_network.lan.lan_ip,tmp);
			}
			if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"netmask"))){
				printf("lan netmask:%s\n",tmp);
				strcpy(g_network.lan.lan_nm,tmp);
			}
		}else if(strcmp(s->type,"interface") == 0 && strcmp(e->name,"wan") == 0){
			if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"proto"))){
				printf("wan proto:%s\n",tmp);
				strcpy(g_network.wan1.wan_type,tmp);
				if(strcmp(tmp,"pppoe")==0 || strcmp(tmp,"dhcp")==0){
					continue;
				}
			}
			if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"ipaddr"))){
				printf("wan ipaddr:%s\n",tmp);
				strcpy(g_network.wan1.wan_ip,tmp);
			}
			if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"netmask"))){
				printf("wan ipaddr:%s\n",tmp);
				strcpy(g_network.wan1.wan_nm,tmp);
			}
			if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"gateway"))){
				printf("wan ipaddr:%s\n",tmp);
				strcpy(g_network.wan1.wan_gw,tmp);
			}
			if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"dns"))){
				printf("wan ipaddr:%s\n",tmp);
				strncpy(g_network.wan1.wan_dns,tmp,sizeof(g_network.wan1.wan_dns));
			}
		}
	}
	uci_unload(ctx,pkg);//释放pkg
	uci_free_context(ctx);
	//get dhcp info from dnsmasq
	ctx=uci_alloc_context();
	if(!ctx) 
		return -1;
	if(UCI_OK != uci_load(ctx,DHCP_CONFIG_FILE,&pkg))
	{
		printf("not find network file\n");
		uci_free_context(ctx);
		return ERR_NOTFOUND;
	}
	uci_foreach_element(&pkg->sections,e){
		s = uci_to_section(e);
		if(strcmp(s->type,"dhcp") == 0 && strcmp(e->name,"lan") == 0){
			if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"start")))
			{
				printf("lan start ipaddr:%s\n",tmp);
				g_network.lan.dhcp_start= atoi(tmp);
			}
			if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"limit")))
			{
				printf("lan limit ipaddr:%s\n",tmp);
				g_network.lan.dhcp_end = atoi(tmp);
			}
			if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"ignore")))
			{
				printf("lan dhcp ignore:%s\n",tmp);
				g_network.lan.dhcp_disable= atoi(tmp);
			}
		}
	}
	uci_unload(ctx,pkg);//释放pkg
	uci_free_context(ctx);
	
	//if wan type is dhcp or pppoe,I will get the ip info
	get_ext_iface(ext_ifname);
	if(strlen(ext_ifname) && 0==strlen(g_network.wan1.wan_ip)){
		get_ip(ext_ifname,g_network.wan1.wan_ip);
		get_netmask(ext_ifname,g_network.wan1.wan_nm);
		get_gateway(g_network.wan1.wan_gw);
		get_dns(g_network.wan1.wan_dns);
	}
	if(res && len){
		my_object=json_object_new_object();
		lan = json_object_new_object();
		json_object_object_add(lan, "ip", json_object_new_string(g_network.lan.lan_ip));
		json_object_object_add(lan, "nm", json_object_new_string(g_network.lan.lan_nm));
		json_object_object_add(lan, "dhcp", json_object_new_int(g_network.lan.dhcp_disable));
		json_object_object_add(lan, "dhcp_start", json_object_new_int(g_network.lan.dhcp_start));
		json_object_object_add(lan, "dhcp_end", json_object_new_int(g_network.lan.dhcp_end));
		wan = json_object_new_object();
		json_object_object_add(wan, "type", json_object_new_string(g_network.wan1.wan_type));
		json_object_object_add(wan, "ip", json_object_new_string(g_network.wan1.wan_ip));
		json_object_object_add(wan, "nm", json_object_new_string(g_network.wan1.wan_nm));
		json_object_object_add(wan, "gw", json_object_new_string(g_network.wan1.wan_gw));
		json_object_object_add(wan, "dns", json_object_new_string(g_network.wan1.wan_dns));
		if(strcmp(g_network.wan1.wan_type,"pppoe") == 0){
			json_object_object_add(wan, "pppoe_user", json_object_new_string(g_network.wan1.wan_pppoe_user));
			json_object_object_add(wan, "pppoe_pwd", json_object_new_string(g_network.wan1.wan_pppoe_pwd));
		}
		json_object_object_add(my_object, "lan", lan);
		json_object_object_add(my_object, "wan", wan);
		strncpy(res,json_object_to_json_string(my_object),len);
		eat_space(res);

		if(my_object)	json_object_put(my_object);
		if(lan)	json_object_put(lan);
		if(wan)	json_object_put(wan);
	}
	return SUCCESS;
}
int set_lan(struct network_lan *info)
{
	struct uci_context * _ctx=NULL;
	struct uci_context * _ctx2=NULL;
	struct uci_package * pkg=NULL;
	struct uci_package * pkg2=NULL;
	struct uci_ptr ptr,ptr2;
	int ret =0;
	int restart = 0;
	memset(&ptr,0,sizeof(struct uci_ptr));
	memset(&ptr2,0,sizeof(struct uci_ptr));
	if(info->dhcp_start>255)
		return ERR_IN_PARA;
	if(info->dhcp_end > 255){
		return ERR_IN_PARA;
	}
	if(info->lan_ip || info->lan_nm){
		if(FALSE ==is_valid_ip(info->lan_ip))  return ERR_IPADDR;
		if(FALSE == is_valid_netmask(info->lan_nm)) return ERR_NETMASK;
	}
	if(info->lan_ip && info->lan_nm){
		_ctx=uci_alloc_context();
		if(!_ctx) 
			return ERR_MEM;

		if(UCI_OK != uci_load(_ctx,NETWORK_CONFIG_FILE,&pkg))
		{
			printf("not find network file\n");
			uci_free_context(_ctx);
			return ERR_NOTFOUND;
		}

		ptr.package=NETWORK_CONFIG_FILE;
		ptr.section="lan";

		ptr.option="ipaddr";
		ptr.value=info->lan_ip;
		ret=uci_set(_ctx,&ptr);//写入配置
		if(ret != UCI_OK) goto cleanup;
		ptr.option="netmask";
		ptr.value=info->lan_nm;
		ret=uci_set(_ctx,&ptr);//写入配置
		if(ret != UCI_OK) goto cleanup;
		/*
		   ret=uci_commit(_ctx,&ptr.p,false);//提交保存更改
		   if(ret != UCI_OK) goto cleanup;

		   uci_unload(_ctx,ptr.p);//卸载包
		   uci_free_context(_ctx);//释放上下文
		   _ctx = NULL;
		 */
	}
	if(info->dhcp_start > 0 && info->dhcp_end > 0){
		_ctx2=uci_alloc_context();
		if(!_ctx2) 
			goto cleanup;

		if(UCI_OK != uci_load(_ctx2,DHCP_CONFIG_FILE,&pkg2))
		{
			printf("not find network file\n");
			ret = ERR_NOTFOUND;	
			goto cleanup;
		}
		char tmp[4]={0};
		ptr2.package=DHCP_CONFIG_FILE;
		ptr2.section="lan";
		ptr2.option="ignore";
		if(info->dhcp_disable !=0 ){
			ptr2.value="1";
		}else
			ptr2.value="0";
		ret=uci_set(_ctx2,&ptr2);//写入配置
		if(ret != UCI_OK) goto cleanup;
		ptr2.option="start";
		snprintf(tmp,sizeof(tmp),"%d",info->dhcp_start);
		ptr2.value=tmp;
		ret=uci_set(_ctx2,&ptr2);//写入配置
		if(ret != UCI_OK) goto cleanup;

		ptr2.option="limit";
		snprintf(tmp,sizeof(tmp),"%d",info->dhcp_end);
		ptr2.value=tmp;
		ret=uci_set(_ctx2,&ptr2);//写入配置
		if(ret != UCI_OK) goto cleanup;
		/*
		   ret=uci_commit(_ctx2,&ptr2.p,false);//提交保存更改
		   if(ret != UCI_OK) goto cleanup;
		   uci_unload(_ctx,ptr.p);//卸载包
		   uci_free_context(_ctx);//释放上下文
		   _ctx = NULL;
		 */
	}
	//system("/etc/init.d/network restart &");
cleanup:
	if(ret == UCI_OK){
		ret=uci_commit(_ctx,&ptr.p,false);//提交保存更改
		if(ret == UCI_OK){ 
			ret=uci_commit(_ctx2,&ptr2.p,false);//提交保存更改
			if(ret == UCI_OK) restart = 1;
		}
	}
	if(ret != UCI_OK){
		uci_revert(_ctx,&ptr);
		if(!_ctx2)
			uci_revert(_ctx2,&ptr2);
	}
	if(_ctx){
		uci_unload(_ctx,ptr.p);//卸载包
		uci_free_context(_ctx);//释放上下文
	}
	if(_ctx2){
		uci_unload(_ctx2,ptr2.p);//卸载包
		uci_free_context(_ctx2);//释放上下文
	}
	if(restart)
	{
		//system("/etc/init.d/network restart &");
		printf("/etc/init.d/network restart &\n");
	}
	return ret;
}
int set_lan_cmd(char *json)
{
	struct network_lan info;
	struct json_object *cmd_json=NULL;
	struct json_object *lan_json=NULL;
	if(!json) 
		return ERR_IN_PARA;

	cmd_json= json_tokener_parse(json);
	if(!cmd_json) 
		return ERR_IN_PARA;
	json_object_object_get_ex(cmd_json,"lan",&lan_json);
	memset(&info,0,sizeof(info));
	GetValByKey(lan_json,"ipaddr",json_type_string,info.lan_ip);
	GetValByKey(lan_json,"netmask",json_type_string,info.lan_nm);
	GetValByKey(lan_json,"dhcp_start",json_type_int,&info.dhcp_start);
	GetValByKey(lan_json,"dhcp_end",json_type_int,&info.dhcp_end);
	GetValByKey(lan_json,"dhcp_disable",json_type_int,&info.dhcp_disable);

	json_object_put(cmd_json);
	return set_lan(&info);
}

static int set_wan(struct network_wan *info)
{
	struct uci_context * _ctx=NULL;
	struct uci_package * pkg=NULL;
	struct uci_ptr ptr;
	int ret =0;
	memset(&ptr,0,sizeof(struct uci_ptr));
	_ctx=uci_alloc_context();
	if(!_ctx) 
		return ERR_MEM;

	if(UCI_OK != uci_load(_ctx,NETWORK_CONFIG_FILE,&pkg))
	{
		printf("not find network file\n");
		uci_free_context(_ctx);
		return ERR_NOTFOUND;
	}

	ptr.package=NETWORK_CONFIG_FILE;
	ptr.section="wan";

	ptr.option="proto";
	ptr.value=info->wan_type;
	ret=uci_set(_ctx,&ptr);//写入配置
	if(ret != UCI_OK) goto cleanup;
	if(strcmp(info->wan_type,"pppoe") == 0){
		ptr.option="username";
		ptr.value=info->wan_pppoe_user;
		ret=uci_set(_ctx,&ptr);//写入配置
		if(ret != UCI_OK) goto cleanup;
		ptr.option="password";
		ptr.value=info->wan_pppoe_pwd;
		ret=uci_set(_ctx,&ptr);//写入配置
		if(ret != UCI_OK) goto cleanup;

	}else if(0 == strcmp(info->wan_type,"static")){

		if(!is_valid_ip(info->wan_ip) ||!is_valid_netmask(info->wan_nm) ||!is_valid_ip(info->wan_gw))
			return ERR_IN_PARA;

		ptr.option="ipaddr";
		ptr.value=info->wan_ip;
		ret=uci_set(_ctx,&ptr);//写入配置
		if(ret != UCI_OK) goto cleanup;
		ptr.option="netmask";
		ptr.value=info->wan_nm;
		ret=uci_set(_ctx,&ptr);//写入配置
		if(ret != UCI_OK) goto cleanup;
		ptr.option="gateway";
		ptr.value=info->wan_gw;
		ret=uci_set(_ctx,&ptr);//写入配置
		if(ret != UCI_OK) goto cleanup;
	}
	ret=uci_commit(_ctx,&ptr.p,false);//提交保存更改
	if(ret != UCI_OK) {
		uci_revert(_ctx,&ptr);
		goto cleanup;
	}
cleanup:
	if(!ret){
		uci_revert(_ctx,&ptr);
	}else{
		//system("/etc/init.d/network restart &");
		printf("/etc/init.d/network restart &\n");
	}

	if(_ctx){
		uci_unload(_ctx,ptr.p);//卸载包
		uci_free_context(_ctx);//释放上下文
		_ctx = NULL;
	}
	return ret;
}

int set_wan_cmd(char *json)
{
	struct network_wan info;
	struct json_object *cmd_json=NULL;
	struct json_object *wan_json=NULL;
	if(!json) 
		return ERR_IN_PARA;

	cmd_json= json_tokener_parse(json);
	if(!cmd_json) 
		return ERR_IN_PARA;
	json_object_object_get_ex(cmd_json,"wan",&wan_json);
	memset(&info,0,sizeof(info));
	GetValByKey(wan_json,"type",json_type_string,info.wan_type);
	GetValByKey(wan_json,"ipaddr",json_type_string,info.wan_ip);
	GetValByKey(wan_json,"dns",json_type_string,info.wan_dns);
	GetValByKey(wan_json,"gateway",json_type_string,info.wan_gw);
	GetValByKey(wan_json,"netmask",json_type_string,info.wan_nm);
	GetValByKey(wan_json,"pppoeuser",json_type_string,info.wan_pppoe_user);
	GetValByKey(wan_json,"pppoepwd",json_type_string,info.wan_pppoe_pwd);

	json_object_put(cmd_json);
	return set_wan(&info);
}

int set_wifi(struct wifi_info *info)
{
	char buf[12]={0};
	if(!info)
		return ERR_IN_PARA;
	struct uci_context * _ctx=NULL;
	struct uci_package * pkg=NULL;
	struct uci_ptr ptr;
	int ret =0;
	memset(&ptr,0,sizeof(struct uci_ptr));
	_ctx=uci_alloc_context();
	if(!_ctx) 
		return ERR_MEM;

	if(UCI_OK != uci_load(_ctx,WIFI_CONFIG_FILE,&pkg))
	{
		printf("not find network file\n");
		uci_free_context(_ctx);
		return ERR_NOTFOUND;
	}

	ptr.package=WIFI_CONFIG_FILE;
	ptr.section=info->iface_name;

	ptr.option="ssid";
	ptr.value=info->ssid;
	ret=uci_set(_ctx,&ptr);//写入配置
	if(ret != UCI_OK) goto cleanup;
	ptr.option="encryption";
	ptr.value=info->encrypt;
	ret=uci_set(_ctx,&ptr);//写入配置
	if(ret != UCI_OK) goto cleanup;
	ptr.option="key";
	ptr.value=info->passwd;
	ret=uci_set(_ctx,&ptr);//写入配置
	if(ret != UCI_OK) goto cleanup;
	if(info->enable==1){
		strcpy(buf,"0\0");
	}else{
		strcpy(buf,"1\0");
	}
	ptr.option="disabled";
	ptr.value=buf;
	ret=uci_set(_ctx,&ptr);//写入配置
	if(ret != UCI_OK) goto cleanup;

	ptr.section=info->device;
	//set ch
	snprintf(buf,sizeof(buf),"%d",info->ch);
	ptr.option="channel";
	ptr.value=buf;
	ret=uci_set(_ctx,&ptr);//写入配置
	if(ret != UCI_OK) goto cleanup;
	//set txpower
	ptr.option="txpower";
	ptr.value=info->txpower;
	ret=uci_set(_ctx,&ptr);//写入配置
	if(ret != UCI_OK) goto cleanup;

	ret=uci_commit(_ctx,&ptr.p,false);//提交保存更改
	if(ret != UCI_OK) goto cleanup;

cleanup:
	if(ret != UCI_OK){
		uci_revert(_ctx,&ptr);
	}
	uci_unload(_ctx,ptr.p);//卸载包
	uci_free_context(_ctx);//释放上下文
	return ret;

}

int set_wifi_cmd(char *json)
{
	struct wifi_info info_24g;
	struct wifi_info info_5g;
	struct json_object *cmd_json=NULL;
	struct json_object *json_24g=NULL;
	struct json_object *json_5g=NULL;
	int ret =0;
	if(!json) 
		return ERR_IN_PARA;

	cmd_json= json_tokener_parse(json);
	if(!cmd_json) 
		return ERR_IN_PARA;
	json_object_object_get_ex(cmd_json,"2.4g",&json_24g);
	json_object_object_get_ex(cmd_json,"5g",&json_5g);
	memset(&info_24g,0,sizeof(info_24g));
	memset(&info_5g,0,sizeof(info_5g));
	if(json_24g){
		if(strlen(g_wifi24.device)){
			info_24g.band = 24;
			strcpy(info_24g.txpower,"100");
			GetValByKey(json_24g,"ssid",json_type_string,info_24g.ssid);
			GetValByKey(json_24g,"encrypt",json_type_string,info_24g.encrypt);
			GetValByKey(json_24g,"passwd",json_type_string,info_24g.passwd);
			GetValByKey(json_24g,"txpower",json_type_string,info_24g.txpower);
			GetValByKey(json_24g,"enable",json_type_int,&info_24g.enable);
			GetValByKey(json_24g,"ch",json_type_int,&info_24g.ch);
			strcpy(info_24g.device,g_wifi24.device);
			strcpy(info_24g.iface_name,g_wifi24.iface_name);
			printf("enable:%d,ch:%d\n",info_24g.enable,info_24g.ch);
			ret = set_wifi(&info_24g);
			if(ret != 0){
				printf("set 2.4G err!ret=%d\n",ret);
				goto cleanup;
			}
		}
	}
	if(json_5g){
		if(strlen(g_wifi5.device)){
			info_5g.band = 5;
			strcpy(info_5g.txpower,"100");
			GetValByKey(json_5g,"ssid",json_type_string,info_5g.ssid);
			GetValByKey(json_5g,"encrypt",json_type_string,info_5g.encrypt);
			GetValByKey(json_5g,"passwd",json_type_string,info_5g.passwd);
			GetValByKey(json_5g,"enable",json_type_int,&info_5g.enable);
			GetValByKey(json_5g,"txpower",json_type_string,info_5g.txpower);
			GetValByKey(json_5g,"ch",json_type_int,&info_5g.ch);
			strcpy(info_5g.device,g_wifi5.device);
			strcpy(info_5g.iface_name,g_wifi5.iface_name);
			printf("5g enable:%d,ch:%d\n",info_24g.enable,info_24g.ch);
			ret = set_wifi(&info_5g);
			if(ret != 0){
				printf("set 5G err!ret=%d\n",ret);
				goto cleanup;
			}
		}
	}
cleanup:
	json_object_put(cmd_json);

	return ret;
}
int del_update_stat()
{
	int ret=0;
	struct uci_ptr ptr;
	memset(&ptr,0,sizeof(struct uci_ptr ));
	ptr.package=MQTT_CONFIG_FILE;
	ptr.section="server";

	struct uci_context * _ctx=uci_alloc_context();//申请上下文
	ptr.option="update";
	ret=uci_delete(_ctx,&ptr);//写入配置
	if(ret != UCI_OK)
		goto cleanup;

	ret=uci_commit(_ctx,&ptr.p,false);//提交保存更改
	if(ret != UCI_OK) goto cleanup;
cleanup:
	uci_unload(_ctx,ptr.p);//卸载包
	uci_free_context(_ctx);//释放上下文
	return ret;
}

int get_update_stat(char *update_type)
{
	struct uci_element *e=NULL;
	struct uci_package * pkg=NULL;
	struct uci_context * ctx=NULL;//申请上下文
	struct uci_section *s=NULL;
	char *tmp=NULL;

	if(!update_type)
		return ERR_IN_PARA;
	ctx=uci_alloc_context();
	if(!ctx) 
		return -1;
	if(UCI_OK != uci_load(ctx,MQTT_CONFIG_FILE,&pkg))
	{
		printf("not find network file\n");
		uci_free_context(ctx);
		return ERR_NOTFOUND;
	}
	uci_foreach_element(&pkg->sections,e){
		s = uci_to_section(e);
		if(strcmp(s->type,"conf") == 0 && strcmp(e->name,"server") == 0){
			if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"update")))
			{
				strcpy(update_type,tmp);
			}else{
				update_type[0]='\0';
			}
		}
	}
	uci_unload(ctx,pkg);//释放pkg
	uci_free_context(ctx);
	return 0;
}


int set_update_stat(char *update_type)
{
	int ret=0;
	struct uci_ptr ptr;
	memset(&ptr,0,sizeof(struct uci_ptr ));
	ptr.package=MQTT_CONFIG_FILE;
	ptr.section="server";

	if(!update_type)
		return ERR_IN_PARA;
	struct uci_context * _ctx=uci_alloc_context();//申请上下文
	ptr.option="update";
	ptr.value=update_type;
	ret=uci_set(_ctx,&ptr);//写入配置
	if(ret != UCI_OK)
		goto cleanup;

	ret=uci_commit(_ctx,&ptr.p,false);//提交保存更改
	if(ret != UCI_OK) goto cleanup;
cleanup:
	uci_unload(_ctx,ptr.p);//卸载包
	uci_free_context(_ctx);//释放上下文
	return ret;
}

struct update_info
{
	char url[256];
	char md5[33];
	char from[32];
	char to[32];
	int delay;
	char model[32];
	char hdver[16];
	int mid;
};
void* thread_update_fw(void *arg)
{
	char buf[512]={0};
	int ret=0;
	char md5[33]={0};
	int my_delay=0;
	struct update_info *update_info = (struct update_info *)arg;
	printf("url:%s,md5:%s,to:%s\n",update_info->url,update_info->md5,update_info->to);
	printf("delay=%d,mid=%d,to=%s\n",update_info->delay,update_info->mid,update_info->to);
	if(strlen(update_info->url)<5 || strncmp(update_info->url,"https://",9)==0 ) 
	{
		ret = ERR_URL;goto cleanup;
	}
	if(!strlen(update_info->to))
	{
		ret = ERR_UNMATCH_VER;
		goto cleanup;
	}
	if(strlen(update_info->md5) <32){
		ret = ERR_MD5;goto cleanup;
	}

	//随机延时10分钟以内下载时间
	if(update_info->delay==-1){
		srand((int)time(0));
		my_delay = rand()%60;
	}else
		my_delay = update_info->delay;
	printf("i will sleep %d\n",my_delay);
	sleep(my_delay);
	//调用wget下载
	snprintf(buf,sizeof(buf),"rm -f /tmp/new.fw ; wget -O /tmp/new.fw -q -c %s",update_info->url);
	system(buf);
	//校验MD5
	md5_file("/tmp/new.fw",md5);
	if(strncmp(md5,update_info->md5,20) != 0)
	{
		ret = ERR_MD5;goto cleanup;
	}
	//开始烧录


cleanup:
	//回复mqtt消息，准备重启
	snprintf(buf,sizeof(buf),"{\"cmd\":\"updateFW\",\"mid\":%d,\"result\":%d,\"errmsg\":\"%s\",\"delay\":%d}",update_info->mid,ret,errmsg[ret],my_delay);
	if(0!= mosquitto_publish(mosq, NULL,g_myrtopic,strlen(buf),buf, QOS_ONCE_ATLEAST, 0))
	{
		fprintf(stderr, "Error: Publish returned %d, disconnecting.\n", ret);	
	}
	if(update_info) free(update_info);
	//重启
	if(ret ==0){
		system("reboot");
		system("reboot");
		system("reboot");
		system("reboot");
	}
	pthread_exit(NULL);
}
int set_updatefw_cmd(char *json)
{
	struct json_object *cmd_json=NULL;
	struct update_info *update_info;
	pthread_t th_fw=0;
	int ret =0;
	update_info = malloc(sizeof( struct update_info));
	memset(update_info,0,sizeof( struct update_info));
	if(!json) 
		return ERR_IN_PARA;

	cmd_json= json_tokener_parse(json);
	if(!cmd_json) 
		return ERR_IN_PARA;
	GetValByKey(cmd_json,"downURL",json_type_string,update_info->url);
	GetValByKey(cmd_json,"md5",json_type_string,update_info->md5);
	GetValByKey(cmd_json,"delay",json_type_int,&update_info->delay);
	GetValByKey(cmd_json,"mid",json_type_int,&update_info->mid);
	GetValByKey(cmd_json,"from",json_type_string,update_info->from);
	GetValByKey(cmd_json,"to",json_type_string,update_info->to);
	GetValByKey(cmd_json,"model",json_type_string,update_info->model);
	GetValByKey(cmd_json,"hdver",json_type_string,update_info->hdver);
	printf("url:%s,md5:%s,to:%s,mid:%d\n",update_info->url,update_info->md5,update_info->to,update_info->mid);
	if(strcmp(update_info->to,g_sys.op_version)==0){
		ret = ERR_SAME_VER;goto cleanup;
	}
	if(strlen(update_info->url)<5 || strncmp(update_info->url,"https://",9)==0 ) 
	{
		ret = ERR_URL;goto cleanup;
	}
	if(!strlen(update_info->to))
	{
		ret = ERR_UNMATCH_VER;
		goto cleanup;
	}
	if(strlen(update_info->md5) <32){
		ret = ERR_MD5;goto cleanup;
	}

	pthread_create(&th_fw,NULL,(void*)thread_update_fw,(void*)update_info);
	pthread_detach(th_fw);

cleanup:
	json_object_put(cmd_json);
	return ret;
}

void* thread_update_me(void *arg)
{
	char buf[256]={0};
	int ret=0;
	char md5[33]={0};
	const char *p=NULL;
	int my_delay=0;
	struct update_info *update_info = (struct update_info *)arg;
	if(strlen(update_info->url)<5 || strncmp(update_info->url,"https://",9)==0 ) 
	{
		ret = ERR_URL;goto cleanup;
	}
	if(!strlen(update_info->to))
	{
		ret = ERR_UNMATCH_VER;
		goto cleanup;
	}
	if(strlen(update_info->md5) <32){
		ret = ERR_MD5;goto cleanup;
	}

	//判断版本
	p = strchr(VER,'-');
	if(p==NULL){
		ret = ERR_UNMATCH_VER;goto cleanup;
	}
	if(0 != strncmp(VER,update_info->to,p-&VER[0]+1)){
		ret = ERR_UNMATCH_VER;goto cleanup;
	}
	//随机延时10分钟以内下载时间
	if(update_info->delay==-1){
		srand((int)time(0));
		my_delay = rand()%600;
	}else
		my_delay = update_info->delay;
	sleep(my_delay);
	//调用wget下载
	snprintf(buf,sizeof(buf),"rm -f /tmp/new.me ; wget -O /tmp/new.me -q -c %s",update_info->url);
	system(buf);
	//校验MD5
	md5_file("/tmp/new.me",md5);
	if(strncmp(md5,update_info->md5,20) != 0)
	{
		ret = ERR_MD5;goto cleanup;
	}
	//开始烧录
	system("cp /tmp/new.me /sbin/mbox ; chmod 777 /sbin/mbox");
	set_update_stat("mbox");
	system("/sbin/mbox &");
	exit(0);
	exit(0);
	exit(0);
cleanup:
	//回复mqtt消息，准备重启
	snprintf(buf,sizeof(buf),"{\"cmd\":\"updateME\",\"mid\":%d,\"result\":%d,\"errmsg\":\"%s\"}",update_info->mid,ret,errmsg[ret]);
	ret = mosquitto_publish(mosq, NULL,g_myrtopic,strlen(buf),buf, QOS_ONCE_ATLEAST, 0);
	if(ret !=0 ){
		fprintf(stderr, "Error: Publish returned %d, disconnecting.\n", ret);	
	}
	pthread_exit(NULL);
	return NULL;
}

int set_updateme_cmd(char *json)
{
	struct json_object *cmd_json=NULL;
	struct update_info *update_info=NULL;
	pthread_t th_me=0;
	int ret =0;
	update_info = malloc(sizeof( struct update_info));
	if(!update_info)
		return ERR_MEM;
	memset(update_info,0,sizeof( struct update_info));

	if(!json) 
		return ERR_IN_PARA;

	cmd_json= json_tokener_parse(json);
	if(!cmd_json) 
		return ERR_IN_PARA;
	GetValByKey(cmd_json,"downURL",json_type_string,update_info->url);
	GetValByKey(cmd_json,"md5",json_type_string,update_info->md5);
	GetValByKey(cmd_json,"delay",json_type_int,&update_info->delay);
	GetValByKey(cmd_json,"mid",json_type_int,&update_info->mid);
	GetValByKey(cmd_json,"from",json_type_string,update_info->from);
	GetValByKey(cmd_json,"to",json_type_string,update_info->to);
	GetValByKey(cmd_json,"model",json_type_string,update_info->model);
	GetValByKey(cmd_json,"hdver",json_type_string,update_info->hdver);
	if(strcmp(update_info->to,VER)==0){
		ret = ERR_SAME_VER;goto cleanup;
	}
	if(strlen(update_info->url)<5 || strncmp(update_info->url,"https://",9)==0 ) 
	{
		ret = ERR_URL;goto cleanup;
	}
	if(!strlen(update_info->to))
	{
		ret = ERR_UNMATCH_VER;
		goto cleanup;
	}
	if(strlen(update_info->md5) <32){
		ret = ERR_MD5;goto cleanup;
	}

	pthread_create(&th_me,NULL,(void*)thread_update_me,(void*)update_info);
	pthread_detach(th_me);
cleanup:
	json_object_put(cmd_json);
	return ret;
}

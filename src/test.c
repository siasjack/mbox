#include<stdio.h>
#include <string.h>
#include <uci.h>
#include <json/json.h>
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


#define MQTT_CONFIG_FILE	"mqttctrl"
#define WIFI_CONFIG_FILE	"wireless"
#define NETWORK_CONFIG_FILE	"network"
#define DHCP_CONFIG_FILE	"dhcp"


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
				*update_type='\0';
			}
		}
	}
	uci_unload(ctx,pkg);//释放pkg
	uci_free_context(ctx);
	return 0;
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
			printf("[%s]\n",tmp);
			*all = atoi(tmp);
		}else if(strstr(buf,"MemFree:")){
			getNthValueSafe(1,buf,':',tmp,sizeof(tmp));
			printf("[%s]\n",tmp);
			*free = atoi(tmp);
		}
	}
	fclose(fd);
	return 0;

}

int get_cpuuse(char *cpu_use1,char* cpu_use5,char* cpu_use15)
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


struct wifi_info{
	int band;//24=2.4G,5=5G
	int ch;
	char device[16];
	char hwmode[12];
	char ssid[64];
	char encryt[16];
	char passwd[32];
	char mode[8];
	int enable;
};

struct wifi_info g_wifi24;
struct wifi_info g_wifi5;

int get_wifiinfo(char *res,int len)
{
	struct uci_element *e;
	struct uci_package * pkg=NULL;
	struct uci_context * ctx=NULL;
	char *band=NULL,*device=NULL,*tmp=NULL,*ch=NULL;
	ctx=uci_alloc_context();
	if(!ctx) 
		return -1;
	if(UCI_OK != uci_load(ctx,WIFI_CONFIG_FILE,&pkg))
	{
		printf("xxxxx\n");
		uci_free_context(ctx);
		return -1;
	}
	uci_foreach_element(&pkg->sections,e){
		struct uci_section* s = uci_to_section(e);
		if(strcmp(s->type,"wifi-device") == 0){
			if(NULL!=(band=(char*)uci_lookup_option_string(ctx,s,"band")) ||
					NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"hwmode"))){
				printf("band:%s,hwmode:%s\n",band,tmp);
				if((band && strcasecmp("2.4G",band)==0) || (tmp && strcasecmp("11a",tmp)!=0)){
					g_wifi24.band=24;
					if(NULL!=(ch=(char*)uci_lookup_option_string(ctx,s,"channel")))
						g_wifi24.ch = atoi(ch);
					g_wifi24.enable=1;
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"disabled")))
						if(atoi(tmp) == 0)
							g_wifi24.enable = 0;

					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"hwmode"))){
						strcpy(g_wifi24.hwmode,tmp);
					}
					strcpy(g_wifi24.device,e->name);
					printf("device:%s,channel:%d,enable:%d\n",g_wifi24.device,g_wifi24.ch,g_wifi24.enable);
				}else if((band && strcasecmp("5G",band)==0) || (tmp && strcasecmp("11a",tmp)==0)){
					printf("5g device:%s,channel:%d,enable:%d\n",g_wifi5.device,g_wifi5.ch,g_wifi5.enable);
					g_wifi5.band=5;
					printf("5g device:%s,channel:%d,enable:%d\n",g_wifi5.device,g_wifi5.ch,g_wifi5.enable);
					if(NULL!=(ch=(char*)uci_lookup_option_string(ctx,s,"channel")))
						g_wifi5.ch = atoi(ch);
					printf("5g device:%s,channel:%d,enable:%d\n",g_wifi5.device,g_wifi5.ch,g_wifi5.enable);
					strcpy(g_wifi5.device,e->name);
					printf("5g device:%s,channel:%d,enable:%d\n",g_wifi5.device,g_wifi5.ch,g_wifi5.enable);
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"disabled")))
						if(atoi(tmp) == 0)
							g_wifi5.enable = 0;
					printf("5g device:%s,channel:%d,enable:%d\n",g_wifi5.device,g_wifi5.ch,g_wifi5.enable);
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"hwmode"))){
						strcpy(g_wifi5.hwmode,tmp);
					}
					printf("5g device:%s,channel:%d,enable:%d\n",g_wifi5.device,g_wifi5.ch,g_wifi5.enable);
				}
			}
		}else if(strcmp(s->type,"wifi-iface") == 0){
			if(NULL!=(device=(char*)uci_lookup_option_string(ctx,s,"device"))){
				printf("device :%s\n",device);
				if(0 == strcmp(device,g_wifi24.device)){//2.4G iface
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"ssid"))){
						strncpy(g_wifi24.ssid,tmp,sizeof(g_wifi24.ssid)-1);
					}
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"mode"))){
						strncpy(g_wifi24.mode,tmp,sizeof(g_wifi24.mode)-1);
					}
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"encryption"))){
						strncpy(g_wifi24.encryt,tmp,sizeof(g_wifi24.encryt)-1);
					}
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"key"))){
						strncpy(g_wifi24.passwd,tmp,sizeof(g_wifi24.passwd)-1);
					}
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"disabled")))
						g_wifi24.enable = g_wifi24.enable && atoi(tmp);

				}else if(0 == strcmp(device,g_wifi5.device)){
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"ssid"))){
						strncpy(g_wifi5.ssid,tmp,sizeof(g_wifi5.ssid)-1);
					}
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"mode"))){
						strncpy(g_wifi5.mode,tmp,sizeof(g_wifi5.mode)-1);
					}
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"encryption"))){
						strncpy(g_wifi5.encryt,tmp,sizeof(g_wifi5.encryt)-1);
					}
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"key"))){
						strncpy(g_wifi5.passwd,tmp,sizeof(g_wifi5.passwd)-1);
					}
					if(NULL!=(tmp=(char*)uci_lookup_option_string(ctx,s,"disabled")))
						g_wifi5.enable = g_wifi5.enable && atoi(tmp);
				}
			}
		}
	}
	uci_unload(ctx,pkg);//释放pkg
	uci_free_context(ctx);
	if(res && len)
	{
	}	
	return 0;

}

int set_bindinfo(char *telnum)
{
	int ret=0;
	struct uci_ptr ptr;
	memset(&ptr,0,sizeof(struct uci_ptr));
	ptr.package="mqtt";
	ptr.section="bind";

	if(!telnum)
		return -1;
	struct uci_context * _ctx=uci_alloc_context();//申请上下文
	printf("%d\n",__LINE__);
	ptr.option="telnum";
	printf("%d\n",__LINE__);
	ptr.value=telnum;
	printf("%d\n",__LINE__);
	ret=uci_set(_ctx,&ptr);//写入配置
	printf("%d\n",__LINE__);
	if(ret != UCI_OK)
		goto cleanup;
	ptr.option="bindtime";
	ptr.value="";
	printf("%d\n",__LINE__);
	ret=uci_set(_ctx,&ptr);//写入配置
	if(ret != UCI_OK) goto cleanup;
	ptr.option="bind";
	ptr.value="1";
	ret=uci_set(_ctx,&ptr);//写入配置
	if(ret != UCI_OK) goto cleanup;

	//	ret=uci_commit(_ctx,&ptr.p,false);//提交保存更改
	//	if(ret != UCI_OK) goto cleanup;
	//	goto cleanup;
cleanup:
	uci_unload(_ctx,ptr.p);//卸载包
	uci_free_context(_ctx);//释放上下文
	return ret;
}
void test_arr()
{
	int i=0;
	json_object *my_arr=NULL;
	json_object *my_lanhost=NULL;
	my_arr = json_object_new_array();
	while(i<3){
		my_lanhost = json_object_new_array();
		json_object_array_add(my_lanhost, json_object_new_string("xxxxxx"));
		json_object_array_add(my_lanhost, json_object_new_string("sss"));
		json_object_array_add(my_lanhost, json_object_new_int(1));
		json_object_array_add(my_arr,my_lanhost);
		//json_object_put(my_lanhost);
		i++;
	}
	printf("====%s\n",json_object_to_json_string(my_arr));
	json_object_put(my_arr);
}

char *eat_char(char *org,char *dest,int len,char d)
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
char *eat_char2(char *org,char d)
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
int main()
{
	int all,free;
	char cpu1[18],cpu5[18],cpu15[8];
	get_meminfo(&all,&free);
	get_cpuuse(cpu1,cpu5,cpu15);
	printf("%d %d %s %s %s\n",all,free,cpu1,cpu5,cpu15);
	get_wifiinfo(NULL,0);

	char str[]="{\"cmd\":123,\"ob\":{\"test\":1}}";
	struct json_object *cmd_json=NULL;
	cmd_json= json_tokener_parse(str);
	if(!cmd_json) return -1;
	struct json_object *ob=NULL;
	json_object_object_get_ex(cmd_json,"ob",&ob);
	printf("%s\n",json_object_to_json_string(ob));
	json_object_put(cmd_json);	

	//test uci set
	set_bindinfo("432143214");
	test_arr();
	strcpy(cpu1," x[\t]\nss2");
	eat_char(cpu1,cpu5,sizeof(cpu5),'s');
	printf("cpu5:[%s]\n",cpu5);
	eat_char2(cpu1,'s');
	printf("cpu1:[%s]\n",cpu1);
	char band[]="5G";
	char tmp[]="0";
	if((band && strcasecmp("5G",band)==0) || (tmp && strcasecmp("11a",tmp)==0)){
		printf("xxxxx\n");
	}

	char stat[16]={0};
	get_update_stat(stat);
	printf("%s\n",stat);
	set_update_stat("mbox");
	get_update_stat(stat);
	printf("%s\n",stat);
	del_update_stat();
	get_update_stat(stat);
	printf("%s\n",stat);
	
	
}

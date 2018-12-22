#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mosquitto.h>
#include <unistd.h>
#include <json/json.h>
#include "function.h"
#include "tools.h"
#include "getlanhost.h"
static int last_mid_sent = -1;
pthread_rwlock_t rwlock;    //声明读写锁
struct mosquitto *mosq = NULL;
 
#define devname "common"
static char g_mytopic[64]={0};
char g_myrtopic[64]={0};
//static char host[64] = "localhost\0";
//static int port = 1883;
//static int ssl = 1;
//接收到消息后回调
void my_message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
{
    int rc2=0;
    int mid_sent=0;
    struct json_object *res_json=NULL;
	char send_back[8192]={0};
	char tmp_str[8192]={0};
	if(message->payloadlen){
		printf("%s:%s %s\n",__FUNCTION__,message->topic, (char*)message->payload);
	}else{
		printf("%s (null)\n", message->topic);
	}
	if(strcmp(message->topic,g_mytopic) != 0){
		printf("get unkonw topic:%s\n",message->topic);
		return;
	}
	//prepare data
	char cmd[32]={0};
	int mid=0;
	int ret = 0;
	char telnum_tmp[16]={0};
	res_json= json_tokener_parse(message->payload);
	if(res_json){
		GetValByKey(res_json,"cmd",json_type_string,cmd);
		GetValByKey(res_json,"mid",json_type_int,&mid);
		if(strcmp(cmd,"bind") == 0){
			GetValByKey(res_json,"telnum",json_type_string,telnum_tmp);
			if(!strlen(telnum_tmp)){
				printf("telnum is null!\n");
				return;
			}
			ret = set_bindinfo(telnum_tmp);
			snprintf(send_back,sizeof(send_back),"{\"cmd\":\"%s\",\"mid\":%d,\"result\":%d,\"errmsg\":\"%s\"}",cmd,mid,ret,errmsg[ERR_ALREADY_BIND]);
		}else if(strcmp(cmd,"unbind") == 0)
		{
			ret = set_bindinfo("unbind");
			snprintf(send_back,sizeof(send_back),"{\"cmd\":\"%s\",\"mid\":%d,\"result\":%d,\"errmsg\":\"%s\"}",cmd,mid,ret,errmsg[ret]);

		}else if (strcmp(cmd,"getbindinfo") == 0)
		{
			ret = get_bindinfo(tmp_str,sizeof(tmp_str)-1);
			if(ret == SUCCESS)
				snprintf(send_back,sizeof(send_back),"{\"cmd\":\"%s\",\"mid\":%d,\"result\":%d,\"bindinfo\":%s}",cmd,mid,ret,tmp_str);
			else{
				snprintf(send_back,sizeof(send_back),"{\"cmd\":\"%s\",\"mid\":%d,\"result\":%d,\"errmsg\":%s}",cmd,mid,ret,errmsg[ret]);

			}
		}else if(strcmp(cmd,"getsysinfo") == 0){
			ret = get_sysinfo(tmp_str,sizeof(tmp_str)-1);
			snprintf(send_back,sizeof(send_back),"{\"cmd\":\"%s\",\"mid\":%d,\"result\":%d,\"sysinfo\":%s}",cmd,mid,ret,tmp_str);
		}else if (strcmp(cmd,"getwifiinfo") == 0){
			memset(&g_wifi24,0,sizeof(g_wifi24));
			memset(&g_wifi5,0,sizeof(g_wifi5));
			ret = get_wifiinfo(tmp_str,sizeof(tmp_str)-1);
			if(ret ==0)
				snprintf(send_back,sizeof(send_back),"{\"cmd\":\"%s\",\"mid\":%d,\"result\":%d,\"wifiinfo\":%s}",cmd,mid,ret,tmp_str);
			else
				snprintf(send_back,sizeof(send_back),"{\"cmd\":\"%s\",\"mid\":%d,\"result\":%d,\"errmsg\":\"%s\"}",cmd,mid,ret,errmsg[ret]);

		}else if (strcmp(cmd,"getnetworkinfo") == 0){
			memset(&g_network,0,sizeof(g_network));
			ret = get_network_info(tmp_str,sizeof(tmp_str)-1);
			if(ret == 0){
				snprintf(send_back,sizeof(send_back),"{\"cmd\":\"%s\",\"mid\":%d,\"result\":%d,\"networkinfo\":%s}",cmd,mid,ret,tmp_str);
			}else{
				snprintf(send_back,sizeof(send_back),"{\"cmd\":\"%s\",\"mid\":%d,\"result\":%d,\"errmsg\":\"%s\"}",cmd,mid,ret,errmsg[ret]);
			}

		}else if (strcmp(cmd,"getlanhostinfo") == 0){
			ret = get_lanhost(tmp_str,sizeof(tmp_str)-1);
			snprintf(send_back,sizeof(send_back),"{\"cmd\":\"%s\",\"mid\":%d,\"result\":%d,\"errmsg\":\"%s\",\"lanhostinfo\":%s}",cmd,mid,ret,errmsg[ret],tmp_str);

		}else if(strcmp(cmd,"setsysinfo")==0){

		}else if(strcmp(cmd,"setwifiinfo")==0){
			ret =set_wifi_cmd(message->payload);
			snprintf(send_back,sizeof(send_back),"{\"cmd\":\"%s\",\"mid\":%d,\"result\":%d,\"errmsg\":\"%s\"}",cmd,mid,ret,errmsg[ret]);
		}else if(strcmp(cmd,"setlan" ) == 0){
			ret = set_lan_cmd(message->payload);
			snprintf(send_back,sizeof(send_back),"{\"cmd\":\"%s\",\"mid\":%d,\"result\":%d,\"errmsg\":\"%s\"}",cmd,mid,ret,errmsg[ret]);
		}else if(strcmp(cmd,"setwan" ) == 0){
			ret = set_wan_cmd(message->payload);
			snprintf(send_back,sizeof(send_back),"{\"cmd\":\"%s\",\"mid\":%d,\"result\":%d,\"errmsg\":\"%s\"}",cmd,mid,ret,errmsg[ret]);

		}else if (strcmp(cmd,"reboot") == 0){
			snprintf(send_back,sizeof(send_back),"{\"cmd\":\"%s\",\"mid\":%d,\"result\":0}",cmd,mid);
			mosquitto_publish(mosq, &mid_sent,g_myrtopic,strlen(send_back),send_back, QOS_ONCE_ATLEAST, 0);
			//system("reboot");
			//system("reboot");
		}else if(strcmp(cmd,"updateFW" ) == 0){
			ret = set_updatefw_cmd(message->payload);
			if(ret !=0){
				snprintf(send_back,sizeof(send_back),"{\"cmd\":\"%s\",\"mid\":%d,\"result\":%d,\"errmsg\":\"%s\"}",cmd,mid,ret,errmsg[ret]);
			}
			//need create a thread
		}else if(strcmp(cmd,"updateME" ) == 0){
			ret = set_updateme_cmd(message->payload);
			if(ret!=0){
				snprintf(send_back,sizeof(send_back),"{\"cmd\":\"%s\",\"mid\":%d,\"result\":%d,\"errmsg\":\"%s\"}",cmd,mid,ret,errmsg[ret]);
			}
		}

		if(strlen(send_back)){
			rc2 = mosquitto_publish(mosq, NULL,g_myrtopic,strlen(send_back),send_back, QOS_ONCE_ATLEAST, 0);
			if(rc2){
				fprintf(stderr, "Error: Publish returned %d, disconnecting.\n", rc2);
				//mosquitto_disconnect(mosq);
			}
			send_back[0]='\0';
		}
	}
	else{
		printf("get payload is not json format:%s\n",(char*)message->payload);
	}
	fflush(stdout);
}
//链接成功后回调
void my_connect_callback(struct mosquitto *mosq, void *userdata, int result)
{
	char stat[16]={0};
	char send_buf[1024]={0};
	if(!result){
		/* Subscribe to broker information topics on successful connect. */
		//mosquitto_subscribe(mosq, NULL, "$SYS/#", 2);
		mosquitto_subscribe(mosq, NULL, g_mytopic, QOS_ONCE_ATLEAST);
		get_update_stat(stat);
		if(strcmp(stat,"mbox") == 0){
			snprintf(send_buf,sizeof(send_buf),"{\"cmd\":\"updateME\",\"new_ver\":\"%s\",\"result\":0}",g_sys.my_version);
			mosquitto_publish(mosq, NULL,g_myrtopic,strlen(send_buf),send_buf, QOS_ONCE_ATLEAST, 0);
		}else if(strcmp(stat,"firmware") == 0){
			snprintf(send_buf,sizeof(send_buf),"{\"cmd\":\"updateFW\",\"new_ver\":\"%s\",\"result\":0}",g_sys.op_version);
			mosquitto_publish(mosq, NULL,g_myrtopic,strlen(send_buf),send_buf, QOS_ONCE_ATLEAST, 0);
		}
		del_update_stat();
	}else{
		fprintf(stderr, "Connect failed\n");
	}
}

void my_publish_callback(struct mosquitto *mosq, void *obj, int mid)
{
	printf("%s:last_mid_sent=%d,mid=%d,\n","my_publish_callback",last_mid_sent,mid);
	last_mid_sent = mid;
}

//订阅成功而调用的函数
void my_subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos)
{
	int i;

	printf("Subscribed (mid: %d): %d", mid, granted_qos[0]);
	for(i=1; i<qos_count; i++){
		printf(", %d", granted_qos[i]);
	}
	printf("\n");
}

void my_log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str)
{
	/* Pring all log messages regardless of level. */
	printf("%s:l:%d,userdata:%s,%s\n",__FUNCTION__, level,(char*)userdata,str);
}

void my_disconnect_callback(struct mosquitto *mosq, void *obj, int rc)
{

	printf("%s:l:%d\n",__FUNCTION__, rc);
	if(rc == 7){
		mosquitto_disconnect(mosq);   
	}
}

void init_para()
{
	memset(&g_sys,0,sizeof(g_sys));
	memset(&g_network,0,sizeof(g_network));
	memset(&g_wifi24,0,sizeof(g_wifi24));
	memset(&g_wifi5,0,sizeof(g_wifi5));
	memset(&g_bind,0,sizeof(g_bind));
	memset(&g_mqtt_conf,0,sizeof(g_mqtt_conf));
	g_mqtt_conf.port = 1883;
	get_server_conf(&g_mqtt_conf);
	if(g_mqtt_conf.host == NULL)
	{
		g_mqtt_conf.host=strdup("localhost");
	}
	get_sysinfo(NULL,0);
	get_bindinfo(NULL,0);
	if(strlen(g_sys.mac))
	{	
		snprintf(g_mytopic,sizeof(g_mytopic),"/route/%s/%s/cmd",devname,g_sys.mac);
		snprintf(g_myrtopic,sizeof(g_myrtopic),"/route/%s/%s/rcmd",devname,g_sys.mac);
	}else{
		printf("get sysinfo mac err!!");
		exit(-1);
	}
	get_wifiinfo(NULL,0);
}

int main(int argc, char *argv[])
{
	int keepalive = 60; //60s一个心跳
	bool clean_session = true;
	//char buf[128]={0};
	pthread_t thid1=0;
	init_para();
	pthread_rwlock_init(&rwlock, NULL);   //初始化读写锁
	pthread_create(&thid1,NULL,(void*)thread_update_lanhost,NULL);
	pthread_detach(thid1);

	printf("version:%s\n",VER);
	// mosquitto
	mosquitto_lib_init();
	mosq = mosquitto_new(g_sys.mac, clean_session, NULL);
	if(!mosq){
		fprintf(stderr, "Error: Out of memory.\n");
		return 1;
	}
	if(g_mqtt_conf.user && g_mqtt_conf.pwd && strlen(g_mqtt_conf.user) && strlen(g_mqtt_conf.pwd)){
		mosquitto_username_pw_set(mosq, g_mqtt_conf.user, g_mqtt_conf.pwd);
	}
	mosquitto_log_callback_set(mosq, my_log_callback);
	mosquitto_connect_callback_set(mosq, my_connect_callback);
	mosquitto_disconnect_callback_set(mosq,my_disconnect_callback);
	mosquitto_message_callback_set(mosq, my_message_callback);
	mosquitto_subscribe_callback_set(mosq, my_subscribe_callback);
	mosquitto_publish_callback_set(mosq, my_publish_callback);
	//set ssl opt

	printf("host:%s,port:%d,ssl:%d\n",g_mqtt_conf.host, g_mqtt_conf.port,g_mqtt_conf.ssl);
	//connect
	while(1){
		if(mosquitto_connect(mosq, g_mqtt_conf.host, g_mqtt_conf.port, keepalive)){
			fprintf(stderr, "Unable to connect mqtt server.\n");
			sleep(3);
		}else{
			break;
		}
	}
	mosquitto_loop_forever(mosq, -1, 1);
	printf("I will quit~\n");
	pthread_rwlock_destroy(&rwlock);      //销毁读写锁
	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();
	return 0;
}

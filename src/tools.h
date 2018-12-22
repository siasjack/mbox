#ifndef __TOOLS_H__
#define __TOOLS_H__
#include <json/json.h>
#include <time.h>

int GetValByKey(json_object * jobj, const  char  *sname,enum json_type type_me,void *rev);
int get_devinfo( struct sysinfo *ptr);
int get_cpuuse(char *cpu_use1,char * cpu_use5,char * cpu_use15);
int get_meminfo(int *all,int *free);
int get_opversion(char *my_version,int len);
int get_current_time(char *res);
int get_sys_uptime(char *up);
int get_ext_iface(char *device);
void get_ap_mac(char *mac);
void copy_mac2str(unsigned char *mac,char *str);
int getNthValueSafe(int index, char *value, char delimit, char *result, int len);
int time_to_str(time_t t,char *res,int res_len);
int md5_string(char *string,char *ret_md5);
int md5_file(char *filename,char *ret_md5);

int get_ip(char *ifname,char ip[16]);
int get_netmask(char *ifname,char ip[16]);
int get_mac(char *ifname,unsigned char addr[6]);
int get_dns(char *dns1);
int is_valid_ip(char ipaddr[16]);
int is_valid_netmask(char netmask[16]);
int get_gateway(char *ip);

char *eat_char(char *org,char d);
char *eat_space(char *org);
#endif

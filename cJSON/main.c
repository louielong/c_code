/*************************************************************************
	> File Name: main.c
	> Author: louie.long
	> Mail: ylong@biigroup.cn
	> Created Time: 2016年08月10日 星期三 11时47分06秒
 ************************************************************************/

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include"cJSON.h"

char text[] = "{\"timestamp\":\"2016-8-10T11:49:00\",\"value\":1}";


cJSON *creat_node(cJSON *array, const double temp, const char *str)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "temp", temp);
    cJSON_AddStringToObject(root, "timestamp", str);
    cJSON_AddItemToArray(array, root);
    //cJSON_Delete(root);
    return array;
}


int main(int argc, const char* argv[])
{
#if 0
    cJSON *json, *json_value, *json_timestamp, *json_item;
    json = cJSON_Parse(text);

    if (!json) {
        printf("Error before : [%s]\n", cJSON_GetErrorPtr());
    } else {
        json_value = cJSON_GetObjectItem(json, "value");
        if (json_value->type == cJSON_Number) {
            printf("value : %d\n", json_value->valueint);
        } else if (json_value->type == cJSON_String) {
            printf("value : %s\n", json_value->valuestring);
        }

        json_timestamp = cJSON_GetObjectItem(json, "timestamp");
        if (json_timestamp->type == cJSON_String) {
            printf("timestamp : %s\n", json_timestamp->valuestring);
        }

        cJSON_Delete(json);
    }


    cJSON *root = cJSON_CreateObject();
    cJSON *arry = cJSON_CreateArray();
    cJSON_AddNumberToObject(root, "temp", 23.4);
    cJSON_AddStringToObject(root, "timestamp", "2016-8-10 15:05:08");
    cJSON_AddItemToArray(arry, root);
    cJSON *root1 = cJSON_CreateObject();
    cJSON_AddNumberToObject(root1, "temp", 27.4);
    cJSON_AddStringToObject(root1, "timestamp", "2016-8-10 18:10:23");
    cJSON_AddItemToArray(arry, root1);
    cJSON *root2 = cJSON_CreateObject();
    cJSON_AddNumberToObject(root2, "temp", 22.4);
    cJSON_AddStringToObject(root2, "timestamp", "2016-8-10 20:43:42");
    cJSON_AddItemToArray(arry, root2);
#endif

    cJSON *array, json_root, json_value, json_item;

    array = cJSON_CreateArray();
    array = creat_node(array, 23.4, "2016-8-10 15:05:08");
    array = creat_node(array, 27.0, "2016-8-10 18:10:23");
    array = creat_node(array, 21.6, "2016-8-10 20:43:42");

    char *out = cJSON_Print(array);
    printf("%s\n", out);
return 0;

#if 0
    json = cJSON_Parse(out);
    cJSON *task_array = cJSON_GetObjectItem(json, "root");
    printf("debug1\n");
    while(task_list != NULL) {
        json_value = cJSON_GetObjectItem(task_list, "temp");
        if (json_value->type == cJSON_Number) {
            temp = json_value->valuedouble;
        }

        json_timestamp = cJSON_GetObjectItem(task_list, "timestamp");
        if (json_timestamp->type == cJSON_String) {
            timestamp = json_timestamp->valuestring;
        }

        printf("Temp : %.2lf\nTime : %s\n", temp, timestamp);

        task_list = task_list->next;
    }
    cJSON_Delete(json);
    return 0;

    if (!json) {
        printf("Error before : [%s]\n", cJSON_GetErrorPtr());
    } else {
        json_value = cJSON_GetObjectItem(json, "temp");
        if (json_value->type == cJSON_Number) {
            temp = json_value->valuedouble;
        }

        json_timestamp = cJSON_GetObjectItem(json, "timestamp");
        if (json_timestamp->type == cJSON_String) {
            timestamp = json_timestamp->valuestring;
        }

        printf("Temp : %lf\nTime : %s\n", temp, timestamp);
        cJSON_Delete(json);
    }

    cJSON_Delete(root);
    cJSON_Delete(root1);
    cJSON_Delete(root2);
    free(out);
#endif



    return 0;
}










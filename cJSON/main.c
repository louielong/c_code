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

void json_parse(const char *str)
{
    /* prase JSON string */
    cJSON *json_root, *json_value, *json_array, *json_timestamp, *arr_item;
    int size, i;
    double temp;
    char *timestamp = NULL;

    json_root = cJSON_Parse(str);
    if (!json_root) {
        printf("Root Error : [%s]\n", cJSON_GetErrorPtr());
        return;
    }

    json_value = cJSON_GetObjectItem(json_root, "total");
    if (json_value->type == cJSON_Number) {
            printf("Totlal Num : %d\n", json_value->valueint);
    }

    json_array = cJSON_GetObjectItem(json_root, "rows");
    if (!json_array) {
        printf("Array Error : [%s]\n", cJSON_GetErrorPtr());
        return;
    }
    size = cJSON_GetArraySize(json_array);

    for (i = 0; i < size; ++i) {
        arr_item = cJSON_GetArrayItem(json_array, i);
        if (arr_item) {
            json_value = cJSON_GetObjectItem(arr_item, "temp");
            if (json_value->type == cJSON_Number) {
                temp = json_value->valuedouble;
            }

            json_timestamp = cJSON_GetObjectItem(arr_item, "timestamp");
            if (json_timestamp->type == cJSON_String) {
                timestamp = json_timestamp->valuestring;
            }
        }

        printf("Temp : %.2lf\nTime : %s\n", temp, timestamp);
    }

    cJSON_Delete(json_root);
    return;
}


int main(int argc, const char* argv[])
{
    char *timestamp[3] = {"2016-8-10 15:05:08", "2016-8-10 18:10:23",
        "2016-8-10 20:43:42"};
    double temp[3] =  {23.4, 27.4, 22.4};

    cJSON *root, *array, *arr;
    int i;

    /* creat JSON string */
    root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "total", 3);
    cJSON_AddItemToObject(root, "rows", array = cJSON_CreateArray());

    for (i = 0; i < 3; ++i) {
        cJSON_AddItemToArray(array, arr = cJSON_CreateObject());
        cJSON_AddItemToObject(arr, "temp", cJSON_CreateNumber(temp[i]));
        cJSON_AddItemToObject(arr, "timestamp", cJSON_CreateString(timestamp[i]));
    }

    char *out = cJSON_Print(root);
    printf("%s\n", out);
    cJSON_Delete(root);


    json_parse(out);

    free(out);

    return 0;
}










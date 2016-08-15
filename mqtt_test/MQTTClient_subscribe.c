/*******************************************************************************
 * Copyright (c) 2012, 2013 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution. 
 *
 * The Eclipse Public License is available at 
 *   http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at 
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial contribution
 *******************************************************************************/

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "MQTTClient.h"
#include"cJSON.h"

#define ADDRESS     "tcp://localhost:1883"
#define CLIENTID    "ExampleClientSub"
#define TOPIC       "MQTT Examples"
#define PAYLOAD     "Hello World!"
#define QOS         1
#define TIMEOUT     10000L

volatile MQTTClient_deliveryToken deliveredtoken;

void delivered(void *context, MQTTClient_deliveryToken dt)
{
    printf("Message with token value %d delivery confirmed\n", dt);
    deliveredtoken = dt;
}

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

        printf("Temp : %.2lfC\nTime : %s\n", temp, timestamp);
    }

    cJSON_Delete(json_root);
    return;
}

int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    int i;
    char* payloadptr;

    printf("Message arrived\n");
    printf("     topic: %s\n", topicName);
    printf("   message: ");

    payloadptr = (char *)message->payload;
    //for(i=0; i<message->payloadlen; i++)
    //{
    //    putchar(*payloadptr++);
    //}
    //putchar('\n');
    json_parse(payloadptr);
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

void connlost(void *context, char *cause)
{
    printf("\nConnection lost\n");
    printf("     cause: %s\n", cause);
}

int main(int argc, char* argv[])
{
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;
    int ch;

    MQTTClient_create(&client, ADDRESS, CLIENTID,
        MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered);

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to connect, return code %d\n", rc);
        exit(EXIT_FAILURE);
    }
    printf("Subscribing to topic %s\nfor client %s using QoS%d\n\n"
           "Press Q<Enter> to quit\n\n", TOPIC, CLIENTID, QOS);
    MQTTClient_subscribe(client, TOPIC, QOS);

    do 
    {
        ch = getchar();
    } while(ch!='Q' && ch != 'q');

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    return rc;
}

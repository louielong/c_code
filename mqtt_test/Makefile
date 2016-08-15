
#CROSS_COMPILE = arm-linux-

CC		=   $(CROSS_COMPILE)gcc
OBJS	=	MQTTClient_publish.o MQTTClient_subscribe.o cJSON.o
INCS	+=	-L/usr/local/lib/mqtt_lib
LIB		=	-lm -lpaho-mqtt3c
SUB		=	MQTTClient_subscribe.o
PUB		=	MQTTClient_publish.o
cJSON	=	cJSON.o


all:$(OBJS)
	$(CC) $(PUB) $(cJSON) $(INCS) $(LIB) -o MQTTClient_publish
	$(CC) $(SUB) $(cJSON) $(INCS) $(LIB) -o MQTTClient_subscribe

MQTTClient_publish.o:
	$(CC) -c MQTTClient_publish.c $(INCS) $(LIB) -o MQTTClient_publish.o

MQTTClient_subscribe.o:
	$(CC) -c MQTTClient_subscribe.c $(INCS) $(LIB) -o MQTTClient_subscribe.o

cJSON.o:
	$(CC) -c cJSON.c $(INCS) $(LIB) -o cJSON.o

.PHONY:clean
clean:
	rm -fr *.o MQTTClient_publish MQTTClient_subscribe

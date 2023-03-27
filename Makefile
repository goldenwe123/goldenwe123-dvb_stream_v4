PROJECT_DIR	?=/usr/local
LDFLAGS = -L/usr/local/lib
PRG_LIB_DIR		:= $(shell find ${PROJECT_DIR}/lib -type d |sed 'i -L') 
PRG_INC_DIR		:= $(shell find ${PROJECT_DIR}/include -type d |sed 'i -I') 
Libs:=-lrga -ldrm -lrockchip_mpp -lutils -lpthread
CC:=gcc
OBJ:=*.c
exe:=main


all:$(obj)
	
	$(CC) -g $(OBJ) -o $(exe) $(PRG_INC_DIR) $(PRG_LIB_DIR) $(Libs) $(LDFLAGS) `pkg-config libwebsockets --libs --cflags`
	@echo $(YELLOW)"========================Success========================"$(BLACK)

.PHONY:clean
clean:
	rm -rf $(obj) $(exe)

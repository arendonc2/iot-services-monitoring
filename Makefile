CC=gcc
CFLAGS=-Wall -Wextra -pthread -Iserver/include
SRC=$(wildcard server/src/*.c)
OUT=server_bin

all:
	$(CC) $(CFLAGS) $(SRC) -o $(OUT)

clean:
	rm -f $(OUT)

run: all
	./$(OUT)

docker-build:
	docker build -t iot-monitor .

docker-run:
	docker run -d --name iot-server -p 8080:8080 -p 9090:9090 -v $(PWD)/logs:/app/logs iot-monitor

docker-stop:
	docker stop iot-server && docker rm iot-server

compose-up:
	docker-compose up --build -d

compose-down:
	docker-compose down

.PHONY: all clean run docker-build docker-run docker-stop compose-up compose-down
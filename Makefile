PORT = /dev/ttyUSB0

all:
	idf.py -p $(PORT) build flash monitor

build:
	idf.py -p $(PORT) build

flash:
	idf.py -p $(PORT) flash

monitor:
	idf.py -p $(PORT) monitor

clean:
	idf.py clean

fullclean:
	idf.py fullclean

erase:
	idf.py -p $(PORT) erase-flash
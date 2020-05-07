all: 
	$(MAKE) -C src

install:
	cd contiki; git submodule update --init --recursive

run:
	cd contiki/tools/cooja; ant run
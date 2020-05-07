all: 
	$(MAKE) -C src

install:
	cd contiki; git submodule update --init --recursive

run:
	cd contiki/tools/cooja; ant run

help:
	@echo "Command: make <argument>"
	@echo "Possible argument:"
	@echo "- all"
	@echo "   Build the project"
	@echo "- install"
	@echo "   Install contiki submodules"
	@echo "- run"
	@echo "   Launch cooja tool"
	@echo "- help"
	@echo "   Display this help"
ifeq (server,$(firstword $(MAKECMDGOALS)))
  RUN_ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
  $(eval $(RUN_ARGS):;@:)
endif

all: 
	$(MAKE) -C src

install:
	cd contiki; git submodule update --init --recursive

run:
	cd contiki/tools/cooja; ant run

server:
	python3 src/Server/server.py $(RUN_ARGS)

help:
	@echo "Command: make <argument>"
	@echo "Possible argument:"
	@echo "- all"
	@echo "   Build the project"
	@echo "- install"
	@echo "   Install contiki submodules"
	@echo "- run"
	@echo "   Launch cooja tool"
	@echo "- server"
	@echo "   Launch server"
	@echo "- help"
	@echo "   Display this help"
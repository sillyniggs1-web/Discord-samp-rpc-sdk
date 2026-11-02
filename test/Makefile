SRC = test_main.cpp ../src/config.cpp ../src/presence_builder.cpp ../src/game_adapter.cpp ../src/rpc_manager.cpp ../src/logger.cpp
all: test_host
test_host: $(SRC)
	g++ -std=c++17 -O1 -g -Wall -Wextra -fsanitize=address,undefined -I../include $(SRC) -o $@ -pthread
run: test_host
	./test_host

rm ./server-test
g++ -Wall ./server.cpp -lpthread -o ./server-test -s
sudo ./server-test

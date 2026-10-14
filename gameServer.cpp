#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <csignal>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

static pthread_mutex_t rngLock = PTHREAD_MUTEX_INITIALIZER;

int randRange(int max)
{
	pthread_mutex_lock(&rngLock);
	int value = (rand() % max) + 1;
	pthread_mutex_unlock(&rngLock);
	return value;
}

bool sendLine(int fd, const std::string &text)
{
	std::string out = text + "\n";
	size_t sent = 0;
	while (sent < out.size())
	{
		ssize_t n = send(fd, out.c_str() + sent, out.size() - sent, MSG_NOSIGNAL);
		if (n <= 0)
			return false;
		sent += n;
	}
	return true;
}

bool recvLine(int fd, std::string &pending, std::string &line)
{
	while (true)
	{
		size_t pos = pending.find('\n');
		if (pos != std::string::npos)
		{
			line = pending.substr(0, pos);
			pending.erase(0, pos + 1);
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			return true;
		}
		char buffer[256];
		ssize_t n = recv(fd, buffer, sizeof(buffer), 0);
		if (n <= 0)
			return false; // client disconnected
		pending.append(buffer, n);
	}
}

bool playGame(int fd, std::string &pending, bool &won)
{
	const int maxNumber[3] = {12, 10, 8};
	won = false;

	for (int round = 0; round < 3; round++)
	{
		int max = maxNumber[round];
		int guess = 0;
		std::string line;

		while (true)
		{
			std::string prompt = "ASK Round " + std::to_string(round + 1) +
								 ": Enter a number between 1 and " + std::to_string(max) + ": ";
			if (!sendLine(fd, prompt) || !recvLine(fd, pending, line))
				return false;

			char *end = nullptr;
			long value = strtol(line.c_str(), &end, 10);
			if (end != line.c_str() && *end == '\0' && value >= 1 && value <= max)
			{
				guess = (int)value;
				break;
			}
			if (!sendLine(fd, "MSG Invalid input. Please try again."))
				return false;
		}

		int a = randRange(max);
		int b = randRange(max);
		int c = randRange(max);

		std::string result = "MSG The server drew: " + std::to_string(a) + " " +
							 std::to_string(b) + " " + std::to_string(c);
		if (!sendLine(fd, result))
			return false;

		if (guess == a || guess == b || guess == c)
		{
			return sendLine(fd, "MSG You were eaten!");
		}
		if (!sendLine(fd, "MSG You escaped."))
			return false;
	}

	won = true;
	return sendLine(fd, "MSG Congratulations, you survived all 3 rounds. You have won!");
}

void *clientThread(void *param)
{
	int fd = *(int *)param;
	free(param);

	std::string pending, line;
	bool playAgain = true;

	while (playAgain)
	{
		bool won = false;
		if (!playGame(fd, pending, won))
			break; 

		if (!sendLine(fd, "ASK Play again? (y/n): ") || !recvLine(fd, pending, line))
			break;
		playAgain = (!line.empty() && (line[0] == 'y' || line[0] == 'Y'));
	}

	sendLine(fd, "END");
	close(fd);
	std::cout << "Client disconnected (socket " << fd << ")\n";
	return nullptr;
}

int main()
{
	srand(time(NULL));
	signal(SIGPIPE, SIG_IGN);

	int server = socket(AF_INET, SOCK_STREAM, 0);
	if (server < 0)
	{
		perror("socket");
		return 1;
	}

	int yes = 1;
	setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

	sockaddr_in address{};
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(9001);

	if (bind(server, (sockaddr *)&address, sizeof(address)) < 0)
	{
		perror("bind");
		close(server);
		return 1;
	}
	if (listen(server, 5) < 0)
	{
		perror("listen");
		close(server);
		return 1;
	}
	std::cout << "Game server now listening on port 9001\n";

	while (true)
	{
		sockaddr_in client{};
		socklen_t clientSize = sizeof(client);
		int clientSocket = accept(server, (sockaddr *)&client, &clientSize);
		if (clientSocket < 0)
		{
			perror("accept");
			continue;
		}
		std::cout << "Client connected (socket " << clientSocket << ")\n";

		int *arg = (int *)malloc(sizeof(int));
		*arg = clientSocket;

		pthread_t tid;
		if (pthread_create(&tid, nullptr, clientThread, arg) != 0)
		{
			perror("pthread_create");
			free(arg);
			close(clientSocket);
			continue;
		}
		pthread_detach(tid); 
	}

	close(server);
	return 0;
}

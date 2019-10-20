// System/compiler includes
#include <iostream>    // Basic console input/output
#include <vector>      // Dynamic arrays for Boost libraries
#include <string>      // String variables
#include <WinSock2.h>  // For the sockets 
#include <WS2tcpip.h>  // For InetPton 
#include <Windows.h>   // Windows default header
#include <sstream>     // Converting variables
#include <fstream>     // File in-/output 
#include <thread>      // Multi-threading
#include <atomic>      // For atomic booleans that can be read thread-to-thread 
//#include <future>      // Thread return values

// Boost includes
#include <boost/algorithm/string.hpp>           // boost::split
#include <boost/algorithm/string/replace.hpp>   // boost::replace
#include <boost/filesystem.hpp>                 // Used for listing files


// Pragma
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

// Define
#define BUFSIZE MAX_PATH
#define BUFFERSIZE 1024

// LS (list files) binaries 
struct path_leaf_string
{
	std::string operator()(const boost::filesystem::directory_entry& entry) const
	{
		return entry.path().leaf().string();
	}
};

// Initializing global variables
std::string serverip = "192.168.0.8"; // IP adress server will be running on, if not local
int port = 5555;                      // Port the server will be running on
char buffer[1024];                    // Buffer with max lenght of 1024 bytes
char pongBuffer[1024] = "pong";      // Pong buffer containing message "pong"
char pingBuffer[1024];                // Ping buffer (will be used on recv end from client) 
bool debug = false;                   // Show debug information on/off (default == off)

// Initializing socket variables
WSADATA WSAData;
SOCKET server, client;
SOCKADDR_IN serverAddr, clientAddr;

// Atomic boolean
std::atomic<bool> terminate_thread(false);
std::atomic<bool> disconnect(false);
std::atomic<bool> uptimeCalled(false);

// Defining thread functions
void serverHandler(int argc, const char* argv[]);
void inputHandler();
void uptimeHandler();

									/********************************\
									|								 |
									|   TODO: Make receive handler   |
									|								 |
									|                                |
									\********************************/


// Main function
int main(int argc, const char* argv[]) {
	// Algorithm for checking arguments
	if (argc > 0) {
		for (int i = 1; i < argc; ++i) {
			if ((strcmp(argv[i], "-port") == 0) || (strcmp(argv[i], "-p") == 0) || (strcmp(argv[i], "port") == 0) || (strcmp(argv[i], "p") == 0)) {
				std::stringstream iss;
				iss << argv[i + 1];
				iss >> port;
				if (iss.fail()) {
					std::cout << "Could not convert input to int. Returning 1." << std::endl;
					return 1;
				}
			}
			else if ((strcmp(argv[i], "-ip") == 0) || (strcmp(argv[i], "ip") == 0)) {
				serverip = argv[i + 1];
			}
			else if ((strcmp(argv[i], "-local") == 0) || (strcmp(argv[i], "local") == 0) || (strcmp(argv[i], "-l") == 0) || (strcmp(argv[i], "l") == 0)) {
				serverip = "127.0.0.1";
			}
			else if (std::string(argv[i]) == "-d") {
				debug = true; // Debug is now on
			}
		}
	}



	std::thread servHndlr(serverHandler, argc, argv);  // Creating thread for the server handler function
	std::thread inputHndlr(inputHandler);              // Creating thread for the input handler function
	std::thread uptimeHndlr(uptimeHandler);            // Creating thread for the uptime handler function

	servHndlr.join();   // Joining server handler thread to run on new thread
	inputHndlr.join();  // Joining input handler thread to run on new thread
	uptimeHndlr.join(); // Joining uptime handler thread to run on new thread

	while (!terminate_thread); // Check if threads need to be terminated
	std::this_thread::sleep_for(std::chrono::seconds(1));
	servHndlr.detach();        // Terminate server handler thread
	inputHndlr.detach();       // Terminate input handler thread
	uptimeHndlr.detach();      // Terminate uptime handler thread


	// Locking the console until keypress
	char input;
	std::cin >> input;
	return 0;
}


// Initializing thread functions
void serverHandler(int argc, const char* argv[]) {
	int iResult = WSAStartup(MAKEWORD(2, 0), &WSAData);
	server = socket(AF_INET, SOCK_STREAM, 0);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		terminate_thread = true;
		return;
	}

	//serverAddr.sin_addr.s_addr = INADDR_ANY;
	InetPton(AF_INET, serverip.c_str(), &serverAddr.sin_addr.s_addr);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);

	bind(server, (SOCKADDR*)& serverAddr, sizeof(serverAddr));
	listen(server, 0);

	std::cout << "Listening for incoming connections on \"" << serverip << ":" << port << "\"..." << std::endl;

	int clientAddrSize = sizeof(clientAddr);
	if ((client = accept(server, (SOCKADDR*)& clientAddr, &clientAddrSize)) != INVALID_SOCKET) {
		std::cout << "Client connected!" << std::endl;
		while (!terminate_thread && !disconnect) {
			recv(client, buffer, sizeof(buffer), 0);
			if (buffer != NULL && buffer[0] != '\0') {
				if ((strcmp(buffer, "disconnect") == 0) || (strcmp(buffer, "dc") == 0) || disconnect) {
					closesocket(client); // Disconnecting socket
					std::cout << "Client disconnected." << std::endl;
					terminate_thread = true;
					break; // Stops the infinite loop to prevent sending messages to unconnected server
				}
				else {
					// Algorithm that puts string into a vector<string> array to have command and argument(s) seperated
					std::string input = std::string(buffer);
					boost::replace_all(input, " ", "\t");
					std::vector<std::string> splitCommand;
					boost::split(splitCommand, input, boost::is_any_of("\t")); // splitCommand[0] is main command, from splitCommand[1] are argvs
					if (splitCommand[0] == "ping") {
						char rcvbuf[1024];
						recv(client, rcvbuf, sizeof(rcvbuf), 0);
						send(client, pongBuffer, sizeof(pongBuffer), 0);
						recv(client, pingBuffer, sizeof(pingBuffer), 0);
						std::cout << "Ping: " << pingBuffer << " ms" << std::endl;
						memset(rcvbuf, 0, sizeof(rcvbuf));
					}
					else if (splitCommand[0] == "pwd") {
						char buf[1024];
						GetCurrentDirectoryA(1024, buf);
						send(client, buf, sizeof(buf), 0);
					}
					else if (splitCommand[0] == "cd") {
						std::string toChdir = splitCommand[1];
						const char* argv2 = toChdir.c_str();
						TCHAR Buffer[BUFSIZE];
						DWORD dwRet;

						dwRet = GetCurrentDirectory(BUFSIZE, Buffer);

						if (dwRet == 0) {
							printf("GetCurrentDirectory failed\n");
							char buffer2[1024] = "GetCurrentDirectory failed";
							send(client, buffer2, sizeof(buffer2), 0);
						}
						if (dwRet > BUFSIZE) {
							printf("Buffer too small; need %d characters\n", dwRet);
							char buffer2[1024] = "Buffer too small";
							send(client, buffer2, sizeof(buffer2), 0);
						}

						if (!SetCurrentDirectory(argv2)) {
							printf("SetCurrentDirectory failed (%d)\n", GetLastError());
							char buffer2[1024] = "SetCurrentDirectory failed";
							send(client, buffer2, sizeof(buffer2), 0);
						}

						char buffer3[1024] = "succesful";
						send(client, buffer3, sizeof(buffer3), 0);
						memset(buffer3, 0, sizeof(buffer3));
						memset(Buffer, 0, sizeof(Buffer));
					}
					else if (splitCommand[0] == "ls") {
						std::vector<std::string> v;
						boost::filesystem::path p(".");
						boost::filesystem::directory_iterator start(p);
						boost::filesystem::directory_iterator end;
						std::transform(start, end, std::back_inserter(v), path_leaf_string());

						// Converting in to char to send vector size to client
						int iVectorSize = v.size();
						char vectorSize[1024];
						std::stringstream cvstr;
						cvstr << iVectorSize;
						strcpy_s(vectorSize, cvstr.str().c_str());

						// Sending vectorSize to client
						send(client, vectorSize, sizeof(vectorSize), 0);

						// Sending vector items one by one
						char cVecBuffer[1024];
						for (size_t i = 0; i < v.size(); ++i) {
							std::string strv2;
							strcpy_s(cVecBuffer, v[i].c_str());
							send(client, cVecBuffer, sizeof(cVecBuffer), 0);
						}
						memset(vectorSize, 0, sizeof(vectorSize));
						memset(cVecBuffer, 0, sizeof(cVecBuffer));
					}
					else if (splitCommand[0] == "download" || splitCommand[0] == "dl") {
						std::cout << "Attempting to send " << splitCommand[1] << " ..." << std::endl;
						std::string file = splitCommand[1];
						std::ifstream fin(file, std::ifstream::binary);
						//char filebuffer[1024]; // Reads only the first 1024 bytes
						std::vector<char> vFilebuffer(1025, 0); // Reads only the first 1024 bytes
						int r; // Pre-declaration variable for the send function to compensate for packet loss
						while (true) {
							fin.read(vFilebuffer.data(), 1024);
							std::streamsize s = ((fin) ? 1024 : fin.gcount());
							vFilebuffer[s] = 0;

							int offset = 0;
							while (offset < BUFFERSIZE) {
								//char filebuffer[1024] = vFilebuffer[0];
								std::string str(vFilebuffer.begin(), vFilebuffer.end());
								char filebuffer[1024];
								strcpy_s(filebuffer, str.c_str());
								r = send(client, filebuffer, sizeof(filebuffer), 0);
								if (debug == true) {
									std::cout << "r = " << r << std::endl << "sent: " << buffer << std::endl;
								}
								if (r <= 0) break;
								offset += r;
								if (debug == true) {
									std::cout << "Offset is " << offset << std::endl;
								}
							}
							if (!fin) break;
						}
						char completed[1024] = "complete";
						send(client, completed, sizeof(completed), 0);
						std::cout << "Sending " << splitCommand[1] << " completed." << std::endl;
						fin.close();
					}
					else if (splitCommand[0] == "uptime" || splitCommand[0] == "up") {
						uptimeCalled = true;
					}
					else {
						std::cout << "Client says: " << buffer << std::endl;
						continue;
					}
				}
				memset(buffer, 0, sizeof(buffer));
			}
		}
	}
}

void inputHandler() {
	while (!terminate_thread) {
		std::string input;
		getline(std::cin, input);
		if (input == "disconnect" || input == "dc") {
			char dcBuffer[1024] = "disconnect";
			send(client, dcBuffer, sizeof(dcBuffer), 0);
			std::cout << "disconnected from client" << std::endl;
			disconnect = true;
			memset(dcBuffer, 0, sizeof(dcBuffer));
			return;
		}
	}
	return;
}

void uptimeHandler() {
	auto start = std::chrono::high_resolution_clock::now();
	while (!terminate_thread && !disconnect) {
		if (!uptimeCalled) continue;
		else {
			auto end = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double, std::milli> elapsed = start - end;
			uptimeCalled = false;
		}
	}
}
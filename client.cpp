#define SDL_MAIN_HANDLED
#define _CRT_SECURE_NO_WARNINGS
#include <SDL_net.h>
#include <iostream>
#include <thread>
#include <cstring>
#include <string>
#include <sstream>

// Packet Types
enum PacketType {
    ChatMessage,
    PlayerJoinRoom,
    PlayerNewRoom,
    PlayerInput,
    RoomStateUpdate,
    KickPlayer
};

struct Packet {
    PacketType type;
    int playerID;
    char data[512];
};

TCPsocket socket;
bool active = false;

void renderRoom(const std::string& roomData) {
    system("cls"); // Clear console for updated map
    std::cout << "Room Map:\n";
    std::istringstream stream(roomData);
    std::string line;
    while (std::getline(stream, line, ';')) {
        std::cout << line << std::endl;
    }
}

void receiveThread() {
    Packet packet;
    while (SDLNet_TCP_Recv(socket, &packet, sizeof(packet)) > 0) {
        if (packet.type == RoomStateUpdate) {
            renderRoom(packet.data);
        }
    }
    std::cout << "Disconnected from server.\n";
}

void sendChat(const std::string& msg) {
    Packet packet;
    packet.type = ChatMessage;
    std::strncpy(packet.data, msg.c_str(), sizeof(packet.data));
    SDLNet_TCP_Send(socket, &packet, sizeof(packet));
}

void joinRoom(const std::string& roomName) {
    Packet packet;
    packet.type = PlayerJoinRoom;
    std::strncpy(packet.data, roomName.c_str(), sizeof(packet.data));
    SDLNet_TCP_Send(socket, &packet, sizeof(packet));
}

void newRoom(const std::string& roomName) {
    Packet packet;
    packet.type = PlayerNewRoom;
    std::strncpy(packet.data, roomName.c_str(), sizeof(packet.data));
    SDLNet_TCP_Send(socket, &packet, sizeof(packet));
}

void sendPlayerInput(float vx, float vy) {
    Packet packet;
    packet.type = PlayerInput;
    std::snprintf(packet.data, sizeof(packet.data), "%.2f %.2f", vx, vy);
    SDLNet_TCP_Send(socket, &packet, sizeof(packet));
}

int main() {
    SDLNet_Init();

    IPaddress ip;
    SDLNet_ResolveHost(&ip, "localhost", 1234);
    socket = SDLNet_TCP_Open(&ip);

    if (!socket) {
        std::cerr << "Failed to connect to server.\n";
        return -1;
    }

    std::thread(receiveThread).detach();

    std::string command;
    while (true) {
        std::getline(std::cin, command);
        if (command == "/quit") break;

        if (command.rfind("/chat ", 0) == 0) {
            sendChat(command.substr(6));
        }
        else if (command.rfind("/join ", 0) == 0) {
            joinRoom(command.substr(6));
        }
        else if (command.rfind("/newroom ", 0) == 0) {
            newRoom(command.substr(9));
        }
        else if (command.rfind("/move ", 0) == 0) {
            float vx, vy;
            sscanf(command.c_str(), "/move %f %f", &vx, &vy);
            sendPlayerInput(vx, vy);
        }
    }

    SDLNet_TCP_Close(socket);
    SDLNet_Quit();
    return 0;
}
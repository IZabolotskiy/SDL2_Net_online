#define SDL_MAIN_HANDLED
#define _CRT_SECURE_NO_WARNINGS
#include <SDL_net.h>
#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <thread>
#include <cstring>
#include <mutex>
#include <sstream>
#include <algorithm>

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

struct PlayerState {
    float x, y;
    float vx, vy;
    bool inRoom = false;
};

class Room {
public:
    std::string name;
    std::vector<int> players;
    std::map<int, PlayerState> playerStates;

    Room(const std::string& name) : name(name) {}

    void addPlayer(int id) {
        players.push_back(id);
        playerStates[id] = { 0, 0, 0, 0, true };
    }

    void removePlayer(int id) {
        players.erase(std::remove(players.begin(), players.end(), id), players.end());
        playerStates.erase(id);
    }

    void processEvents() {
        for (auto& [id, state] : playerStates) {
            state.x += state.vx;
            state.y += state.vy;
        }
    }

    std::string generateMap() {
        const int MAP_SIZE = 9;
        char grid[MAP_SIZE][MAP_SIZE];

        for (int i = 0; i < MAP_SIZE; i++)
            for (int j = 0; j < MAP_SIZE; j++)
                grid[i][j] = '.';

        for (const auto& [id, player] : playerStates) {
            int x = static_cast<int>(player.x);
            int y = static_cast<int>(player.y);
            if (x >= 0 && x < MAP_SIZE && y >= 0 && y < MAP_SIZE)
                grid[y][x] = '0' + (id % 10);
        }

        std::stringstream mapString;
        for (int i = 0; i < MAP_SIZE; i++) {
            for (int j = 0; j < MAP_SIZE; j++) {
                mapString << grid[i][j] << " ";
            }
            mapString << "\n";
        }
        return mapString.str();
    }

    void broadcastState(std::map<int, TCPsocket>& sockets) {
        for (int playerID : players) {
            Packet packet;
            packet.type = RoomStateUpdate;

            std::string stateData = generateMap();
            std::strncpy(packet.data, stateData.c_str(), sizeof(packet.data));

            for (int targetPlayerID : players) {
                SDLNet_TCP_Send(sockets[targetPlayerID], &packet, sizeof(packet));
            }
        }
    }
};

class Lobby {
public:
    std::vector<int> players;
    std::map<std::string, Room*> rooms;

    void addPlayer(int id) { players.push_back(id); }
    void removePlayer(int id) { players.erase(std::remove(players.begin(), players.end(), id), players.end()); }

    Room* createRoom(const std::string& name) {
        rooms[name] = new Room(name);
        return rooms[name];
    }

    Room* getRoom(const std::string& name) {
        return rooms.count(name) ? rooms[name] : nullptr;
    }
};

std::mutex roomMutex;
Lobby lobby;
std::map<int, TCPsocket> playerSockets;

void handlePlayerInput(int playerID, const std::string& data) {
    std::lock_guard<std::mutex> lock(roomMutex);
    auto tokens = data.find(' ');
    float vx = std::stof(data.substr(0, tokens));
    float vy = std::stof(data.substr(tokens + 1));

    for (auto& [name, room] : lobby.rooms) {
        if (std::find(room->players.begin(), room->players.end(), playerID) != room->players.end()) {
            room->playerStates[playerID].vx = vx;
            room->playerStates[playerID].vy = vy;
            break;
        }
    }
}

void playerThread(int playerID, TCPsocket socket) {
    playerSockets[playerID] = socket;
    lobby.addPlayer(playerID);

    Packet packet;
    while (SDLNet_TCP_Recv(socket, &packet, sizeof(packet)) > 0) {
        switch (packet.type) {
        case ChatMessage:
            std::cout << "Chat from " << playerID << ": " << packet.data << std::endl;
            break;
        case PlayerNewRoom: {
            std::string roomName(packet.data);
            Room* room = lobby.createRoom(roomName);
            std::cout << "Player " << playerID << " created room: " << roomName << std::endl;
            break;
        }
        case PlayerJoinRoom: {
            std::string roomName(packet.data);
            Room* room = lobby.getRoom(roomName);
            if (!room) room = lobby.createRoom(roomName);
            room->addPlayer(playerID);
            std::cout << "Player " << playerID << " joined room: " << roomName << std::endl;
            break;
        }
        case PlayerInput:
            handlePlayerInput(playerID, packet.data);
            break;
        default:
            break;
        }
    }

    lobby.removePlayer(playerID);
    playerSockets.erase(playerID);
    SDLNet_TCP_Close(socket);
    std::cout << "Player " << playerID << " disconnected.\n";
}

void roomThread() {
    while (true) {
        std::lock_guard<std::mutex> lock(roomMutex);
        for (auto& [name, room] : lobby.rooms) {
            room->processEvents();
            room->broadcastState(playerSockets);
        }
        SDL_Delay(50);
    }
}

int main() {
    SDLNet_Init();
    IPaddress ip;
    SDLNet_ResolveHost(&ip, nullptr, 1234);
    TCPsocket server = SDLNet_TCP_Open(&ip);

    std::thread(roomThread).detach();

    int nextPlayerID = 1;
    while (true) {
        TCPsocket client = SDLNet_TCP_Accept(server);
        if (client) {
            std::thread(playerThread, nextPlayerID++, client).detach();
        }
        SDL_Delay(50);
    }
    SDLNet_Quit();
    return 0;
}
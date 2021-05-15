#pragma once
#include <iostream>
#include <SFML\Graphics.hpp>
#include <SFML\Network.hpp>
#include <PlayerInfo.h>
#include <queue>
#include <Types.h>
#include <math.h>

#define SERVER_IP "localhost"
#define QUESTION "What's the result of this sum: "

struct Vector2
{
	int x;
	int y;
	Vector2(int _x, int _y)
	{
		x = _x;
		y = _y;
	}
};

//struct para almacenar la info del cliente
struct Clients
{
	sf::IpAddress ip;
	unsigned short port;
	uint64_t clientSalt;
	Vector2 position = Vector2(0, 0);
	std::string username;
	int color;
	sf::Time lastMessageRecieved;
	sf::Time lastTimeValidated;
	int roomTag;
	int lastPositionID = 0;
	int firstNumberChallenge;
	int secondNumberChallenge;
	std::vector<std::pair<sf::Vector2f, int>> positionsToValidate;
};

//struct para almacenar la info de la sala
struct Room
{
	int roomTag;
	std::vector<Clients> clients;
	sf::Time lastPlayerEntered;
	int numPlayers;
	bool playing;

	Room(Clients _client, int _lastTag)
	{
		roomTag = _lastTag + 1;
		clients.push_back(_client);
		numPlayers = 0;
		lastPlayerEntered;
		playing = false;
	}
};

//struct para almacenar la info de la bala
struct Bullet
{
	sf::Vector2f position;
	sf::Vector2f direction;
	float bulletReference;
	sf::Time serverReceivedBulletReference;
	sf::Time bulletSendReference;
	std::string clientIP;
	unsigned short clientPort;
	uint64_t clientSalt;
};

//busca un nuevo color diferente al que se nos indica
int GetNewColor(int colorToAvoid)
{
	int aux = rand() % 4;
	while (aux == colorToAvoid)
	{
		aux = rand() % 4;
	}
	return aux;
}

int main()
{
	srand(time(NULL));
	bool serverRunning = true;
	std::vector<Clients> newClients;
	std::vector<Clients> playAgainClients;
	std::vector<Clients> pendingConfirmationClients;
	std::vector<Room> rooms;
	std::vector<uint64_t> clientSalts;
	std::vector<Bullet> bullets;
	std::vector<std::pair<Clients, float>> startMessages;
	uint64_t serverSalt = std::rand();
	PlayerInfo playerInfo;
	sf::Packet packet;
	sf::UdpSocket socket;

	//conectamos el socket al puerto de escucha y informamos si ha habido un error de conexión
	sf::Socket::Status status = socket.bind(SERVER_PORT);
	if (status != sf::Socket::Done)
	{
		std::cout << "Unable to connect" << std::endl;
	}

	Clients recievedClient;
	socket.setBlocking(false);
	int aux;
	Commands command;
	sf::Clock clock;
	sf::Time time = clock.getElapsedTime();

	while (serverRunning)
	{
		time = clock.getElapsedTime();
		packet.clear();

		//si detectamos la recepción de un paquete por parte de un cliente lo procesamos
		if (socket.receive(packet, recievedClient.ip, recievedClient.port) == sf::Socket::Done)
		{
			packet >> aux;
			command = (Commands)aux;
			switch (command)
			{
			case Commands::HELLO:
			{
				bool existClient = false;
				//miramos si el cliente ya está en nuestro vector newClients
				for (auto client : newClients)
				{
					//si lo está reenviar el challenge
					if (client.ip == recievedClient.ip && client.port == recievedClient.port)
					{						
						existClient = true;
						packet.clear();
						packet << static_cast<int32_t>(Commands::CHALLENGE) << QUESTION + std::to_string(client.firstNumberChallenge) + " + " + std::to_string(client.secondNumberChallenge) + "?" << recievedClient.clientSalt << serverSalt;
						socket.send(packet, recievedClient.ip.toString(), recievedClient.port);
						break;
					}
				}

				if (!existClient)
				{
					//si no está en el vector newClients, miramos si ya está en alguna sala
					for (int i = 0; i < rooms.size(); i++)
					{
						for (int j = 0; j < rooms[i].clients.size(); j++)
						{
							if (rooms[i].clients[j].ip == recievedClient.ip && rooms[i].clients[j].port == recievedClient.port)
							{
								existClient = true;
							}
						}
					}

					//si no está en el vector newClients ni en una room, cogeremos la información del cliente, le asignamos posicion color y challenge aleatorios, hacemos push del cliente en el vector newClients y le enviamos el challenge
					if (!existClient)
					{
						packet >> recievedClient.clientSalt >> recievedClient.username;
						recievedClient.position = Vector2(rand() % 600 + 100, rand() % 400 + 100);
						recievedClient.color = rand() % 4;
						recievedClient.firstNumberChallenge = rand() % 10 + 1;
						recievedClient.secondNumberChallenge = rand() % 10 + 1;
						newClients.push_back(recievedClient);
						packet.clear();
						packet << static_cast<int32_t>(Commands::CHALLENGE) << QUESTION + std::to_string(recievedClient.firstNumberChallenge) + " + " + std::to_string(recievedClient.secondNumberChallenge) + "?" << recievedClient.clientSalt << serverSalt;
						socket.send(packet, recievedClient.ip.toString(), recievedClient.port);
					}
				}

				break;
			}
			case Commands::CHALLENGE_RESPONSE:
			{
				std::string answer;
				uint64_t salts;
				packet >> answer >> salts;

				//comprobamos si hay algún cliente en el vector newClients con la misma ip, puerto y salt que el cliente recibido
				for (int client = 0; client < newClients.size(); client++)
				{
					if (newClients[client].ip == recievedClient.ip && newClients[client].port == recievedClient.port)
					{
						if (salts == (newClients[client].clientSalt & serverSalt))
						{
							if (answer == std::to_string(newClients[client].firstNumberChallenge+ newClients[client].secondNumberChallenge))
							{
								//si la respuesta al challenge es correcta, comprobamos si hay alguna sala, si no la hay la creamos, añadimos al jugador en esta y se lo comunicamos
								if (rooms.empty())
								{
									rooms.push_back(Room(newClients[client], 0));
									newClients[client].roomTag = rooms.back().roomTag;
									clientSalts.push_back(newClients[client].clientSalt);
									rooms.back().numPlayers++;
									packet.clear();
									packet << static_cast<int32_t>(Commands::WELCOME) << (newClients[client].clientSalt & serverSalt) << newClients[client].color << newClients[client].position.x << newClients[client].position.y << 0;
									socket.send(packet, recievedClient.ip.toString(), recievedClient.port);
								}
								else
								{
									bool canAccesToARoom = false;
									//si hay alguna sala, comprobamos si el jugador puede entrar en ella, si puede lo añadimos, se lo comunicamos, y se lo comunicamos al otro jugador
									for (int i = 0; i < rooms.size(); i++)
									{
										if (rooms[i].numPlayers < MAX_PLAYERS_IN_A_ROOM && abs((int)rooms[i].clients.front().username[0] - (int)newClients[client].username[0]) <= 10)
										{
											canAccesToARoom = true;
											if (rooms[i].clients[0].color == newClients[client].color) newClients[client].color = GetNewColor(newClients[client].color);
											newClients[client].roomTag = rooms[i].roomTag;
											rooms[i].clients.push_back(newClients[client]);
											clientSalts.push_back(newClients[client].clientSalt);
											rooms[i].numPlayers++;
											packet.clear();
											packet << static_cast<int32_t>(Commands::WELCOME) << (newClients[client].clientSalt & serverSalt) << newClients[client].color << newClients[client].position.x << newClients[client].position.y << rooms[i].numPlayers - 1;
											for (int j = 0; j < rooms[i].clients.size() - 1; j++)
											{
												packet << rooms[i].clients[j].username << rooms[i].clients[j].color << rooms[i].clients[j].position.x << rooms[i].clients[j].position.y;
											}
											socket.send(packet, recievedClient.ip.toString(), recievedClient.port);

											for (int k = 0; k < rooms[i].clients.size() - 1; k++)
											{
												packet.clear();
												packet << static_cast<int32_t>(Commands::NEW_PLAYER) << (rooms[i].clients[k].clientSalt & serverSalt) << newClients[client].color << newClients[client].username << newClients[client].position.x << newClients[client].position.y;
												socket.send(packet, rooms[i].clients[k].ip.toString(), rooms[i].clients[k].port);
											}
										}
									}

									//si hay alguna sala pero no podemos entrar, creamos una sala nueva, añadimos al jugador en esta y se lo comunicamos
									if (!canAccesToARoom)
									{
										rooms.push_back(Room(newClients[client], rooms.back().roomTag));
										newClients[client].roomTag = rooms.back().roomTag;
										clientSalts.push_back(newClients[client].clientSalt);
										rooms.back().numPlayers++;
										packet.clear();
										packet << static_cast<int32_t>(Commands::WELCOME) << (newClients[client].clientSalt & serverSalt) << newClients[client].color << newClients[client].position.x << newClients[client].position.y << 0;
										socket.send(packet, recievedClient.ip.toString(), recievedClient.port);
									}
								}

								newClients.erase(newClients.begin() + client);
								break;
							}
						}
					}
				}

				break;
			}
			case Commands::ACKNOWLEDGE_WELCOME:
			{
				uint64_t salts;
				packet >> salts;

				//comprobamos si hay algún cliente en alguna sala con la misma ip, puerto y salt que el cliente recibido, y si su sala está llena
				for (auto salt : clientSalts)
				{
					if (salts == (salt & serverSalt))
					{
						for (int i = 0; i < rooms.size(); i++)
						{
							for (int j = 0; j < rooms[i].clients.size(); j++)
							{
								if (rooms[i].clients[j].ip == recievedClient.ip && rooms[i].clients[j].port == recievedClient.port && rooms[i].numPlayers == MAX_PLAYERS_IN_A_ROOM)
								{
									//si se cumplen las condiciones enviamos el comando start a todos los jugadores de la sala
									for (int k = 0; k < rooms[i].clients.size(); k++)
									{
										float sendTime = time.asMilliseconds();
										packet.clear();
										packet << static_cast<int32_t>(Commands::START) << (rooms[i].clients[k].clientSalt & serverSalt) << sendTime;
										socket.send(packet, rooms[i].clients[k].ip.toString(), rooms[i].clients[k].port);
										startMessages.push_back(std::make_pair(rooms[i].clients[k], sendTime));
									}

									rooms[i].playing = true;
									rooms[i].lastPlayerEntered = clock.getElapsedTime();
									break;
								}
							}
						}
					}
				}
				break;
			}
			case Commands::ACKNOWLEDGE_START:
			{
				uint64_t salts;
				float reference;
				packet >> salts;

				//comprobamos si hay algún cliente en alguna sala con la misma ip, puerto y salt que el cliente recibido
				for (int i = 0; i < rooms.size(); i++)
				{
					for (int j = 0; j < rooms[i].clients.size(); j++)
					{
						if (salts == (rooms[i].clients[j].clientSalt & serverSalt))
						{
							packet >> reference;
							//comprobamos si hay algun mensaje con el mismo identificador de tiempo en el vector startMessages, si lo hay lo eliminamos y enviamos el comando ready al jugador
							for (int message = 0; message < startMessages.size(); message++)
							{
								if (startMessages[message].second == reference)
								{
									startMessages.erase(startMessages.begin() + message);
									packet.clear();
									packet << static_cast<int32_t>(Commands::READY) << (rooms[i].clients[j].clientSalt & serverSalt);
									socket.send(packet, rooms[i].clients[j].ip.toString(), rooms[i].clients[j].port);
									break;
								}
							}
						}
					}
				}
				break;
			}
			case Commands::NEW_POSITION:
			{
				uint64_t salts;
				packet >> salts;

				//comprobamos si hay algún cliente en alguna sala con la misma ip, puerto y salt que el cliente recibido
				for(int i = 0; i < rooms.size(); i++)
				{
					for(int j = 0; j < rooms[i].clients.size(); j++)
					{
						if (salts == (rooms[i].clients[j].clientSalt & serverSalt))
						{
							int id;
							float x, y;
							packet >> id;
							//comprobamos si el identificador de movimiento recibido es más reciente que el ultimo verificado, si lo es añadimos el movimiento al vector positionsToValidate y nos guardamos el tiempo
							if (id > rooms[i].clients[j].lastPositionID)
							{
								packet >> x >> y;
								rooms[i].clients[j].positionsToValidate.push_back(std::make_pair(sf::Vector2f(x, y), id));
								rooms[i].clients[j].lastTimeValidated = clock.getElapsedTime();
								break;
							}
						}
					}
				}
				break;
			}
			case Commands::NEW_BULLET:
			{
				uint64_t salts;
				packet >> salts;

				//comprobamos si hay algún cliente en alguna sala con la misma ip, puerto y salt que el cliente recibido
				for (int i = 0; i < rooms.size(); i++)
				{
					for (int j = 0; j < rooms[i].clients.size(); j++)
					{
						if (salts == (rooms[i].clients[j].clientSalt & serverSalt))
						{
							float x, y, xDir, yDir, bulletReference;
							packet >> x >> y >> xDir >> yDir >> bulletReference;
							bool bulletExists = false;
							//comprobamos si la bala ya está en el vector bullets
							for (int bullet = 0; bullet < bullets.size(); bullet++)
							{
								if (bullets[bullet].clientIP == recievedClient.ip.toString() && bullets[bullet].clientPort == recievedClient.port && bullets[bullet].bulletReference == bulletReference) bulletExists = true;
							}
							//si no lo está la creamos, la añadimos y enviamos el comando new_remote_bullet a los otros jugadores
							if (!bulletExists)
							{
								for (int k = 0; k < rooms[i].clients.size(); k++)
								{
									if ((rooms[i].clients[k].clientSalt & serverSalt) != (rooms[i].clients[j].clientSalt & serverSalt))
									{
										Bullet newBullet;
										newBullet.position = sf::Vector2f(x, y);
										newBullet.direction = sf::Vector2f(xDir, yDir);
										newBullet.bulletReference = bulletReference;
										newBullet.serverReceivedBulletReference = clock.getElapsedTime();
										newBullet.bulletSendReference = clock.getElapsedTime();
										newBullet.clientIP = rooms[i].clients[k].ip.toString();
										newBullet.clientPort = rooms[i].clients[k].port;
										newBullet.clientSalt = rooms[i].clients[k].clientSalt;
										bullets.push_back(newBullet);
										packet.clear();
										packet << static_cast<int32_t>(Commands::NEW_REMOTE_BULLET) << (rooms[i].clients[k].clientSalt & serverSalt) << x << y << xDir << yDir << newBullet.serverReceivedBulletReference.asSeconds();
										socket.send(packet, rooms[i].clients[k].ip.toString(), rooms[i].clients[k].port);
									}
								}
							}
							//le enviamos el comando acknowledge_bullet al jugador
							packet.clear();
							packet << static_cast<int32_t>(Commands::ACKNOWLEDGE_BULLET) << (rooms[i].clients[j].clientSalt & serverSalt) << bulletReference;
							socket.send(packet, recievedClient.ip.toString(), recievedClient.port);
						}
					}
				}
			}
			case Commands::ACKNOWLEDGE_REMOTE_BULLET:
			{
				uint64_t salts;
				packet >> salts;

				//comprobamos si hay algún cliente en alguna sala con la misma ip, puerto y salt que el cliente recibido
				for (int i = 0; i < rooms.size(); i++)
				{
					for (int j = 0; j < rooms[i].clients.size(); j++)
					{
						if (salts == (rooms[i].clients[j].clientSalt & serverSalt))
						{
							float bulletReference;
							packet >> bulletReference;
							//comprobamos si hay alguna bala con el mismo identificador en el vector bullets, si la hay la eliminamos
							for (int k = 0; k < bullets.size(); k++)
							{
								if (bullets[k].bulletReference == bulletReference)
								{
									bullets.erase(bullets.begin() + i);
									break;
								}
							}
						}
					}
				}
			}
			case Commands::PLAY_AGAIN_ANSWER:
			{
				bool answer;
				uint64_t salts;
				packet >> salts >> answer;

				//comprobamos si hay algún cliente en el vector playAgainClients con el mismo salt que el cliente recibido
				for (int client = 0; client < playAgainClients.size(); client++)
				{
					if (salts == (playAgainClients[client].clientSalt & serverSalt))
					{
						//si la respuesta es afirmativa le asignamos una sala al jugador, siguiendo la misma lógica que en el comando challenge_response, pero enviando el comando new_game en lugar del welcome
						if (answer)
						{
							playAgainClients[client].position = Vector2(rand() % 600 + 100, rand() % 400 + 100);
							playAgainClients[client].color = rand() % 4;

							if (rooms.empty())
							{
								rooms.push_back(Room(playAgainClients[client], 0));
								playAgainClients[client].roomTag = rooms.back().roomTag;
								clientSalts.push_back(playAgainClients[client].clientSalt);
								rooms.back().numPlayers++;
								packet.clear();
								packet << static_cast<int32_t>(Commands::NEW_GAME) << (playAgainClients[client].clientSalt & serverSalt) << playAgainClients[client].color << playAgainClients[client].position.x << playAgainClients[client].position.y << 0;
								socket.send(packet, recievedClient.ip.toString(), recievedClient.port);
							}
							else
							{
								bool canAccesToARoom = false;
								for (int i = 0; i < rooms.size(); i++)
								{
									if (rooms[i].numPlayers < MAX_PLAYERS_IN_A_ROOM && abs((int)rooms[i].clients.front().username[0] - (int)playAgainClients[client].username[0]) <= 10)
									{
										canAccesToARoom = true;
										if (rooms[i].clients[0].color == playAgainClients[client].color) playAgainClients[client].color = GetNewColor(playAgainClients[client].color);
										playAgainClients[client].roomTag = rooms[i].roomTag;
										rooms[i].clients.push_back(playAgainClients[client]);
										clientSalts.push_back(playAgainClients[client].clientSalt);
										rooms[i].numPlayers++;
										packet.clear();
										packet << static_cast<int32_t>(Commands::NEW_GAME) << (playAgainClients[client].clientSalt & serverSalt) << playAgainClients[client].color << playAgainClients[client].position.x << playAgainClients[client].position.y << rooms[i].numPlayers - 1;
										for (int j = 0; j < rooms[i].clients.size() - 1; j++)
										{
											packet << rooms[i].clients[j].username << rooms[i].clients[j].color << rooms[i].clients[j].position.x << rooms[i].clients[j].position.y;
										}
										socket.send(packet, recievedClient.ip.toString(), recievedClient.port);

										for (int k = 0; k < rooms[i].clients.size() - 1; k++)
										{
											packet.clear();
											packet << static_cast<int32_t>(Commands::NEW_PLAYER) << (rooms[i].clients[k].clientSalt & serverSalt) << playAgainClients[client].color << playAgainClients[client].username << playAgainClients[client].position.x << playAgainClients[client].position.y;
											socket.send(packet, rooms[i].clients[k].ip.toString(), rooms[i].clients[k].port);
										}
									}
								}

								if (!canAccesToARoom)
								{
									rooms.push_back(Room(playAgainClients[client], rooms.back().roomTag));
									playAgainClients[client].roomTag = rooms.back().roomTag;
									clientSalts.push_back(playAgainClients[client].clientSalt);
									rooms.back().numPlayers++;
									packet.clear();
									packet << static_cast<int32_t>(Commands::NEW_GAME) << (playAgainClients[client].clientSalt & serverSalt) << playAgainClients[client].color << playAgainClients[client].position.x << playAgainClients[client].position.y << 0;
									socket.send(packet, recievedClient.ip.toString(), recievedClient.port);
								}
							}

							pendingConfirmationClients.push_back(playAgainClients[client]);
							playAgainClients.erase(playAgainClients.begin() + client);
							break;
						}
						//si la respuesta es negativa procedemos a hacer la desconexión limpia desde servidor
						else
						{
							packet.clear();
							packet << static_cast<int32_t>(Commands::BYE_CLIENT) << (playAgainClients[client].clientSalt & serverSalt);
							socket.send(packet, recievedClient.ip.toString(), recievedClient.port);
							break;
						}
					}
				}
				break;
			}
			case Commands::ACKNOWLEDGE_NEW_GAME:
			{
				uint64_t salts;
				packet >> salts;

				//comprobamos si hay algún cliente en el vector pendingConfirmationClients con el mismo salt que el cliente recibido
				for (int client = 0; client < pendingConfirmationClients.size(); client++)
				{
					if (salts == (pendingConfirmationClients[client].clientSalt & serverSalt))
					{
						//si lo hay comprobamos si la sala del jugador está llena, si lo está se empieza partida siguiendo la misma lógica que en el case acknowledge_welcome
						for (int i = 0; i < rooms.size(); i++)
						{
							for (int j = 0; j < rooms[i].clients.size(); j++)
							{
								if (rooms[i].clients[j].ip == recievedClient.ip && rooms[i].clients[j].port == recievedClient.port && rooms[i].numPlayers == MAX_PLAYERS_IN_A_ROOM)
								{
									for (int k = 0; k < rooms[i].clients.size(); k++)
									{
										float sendTime = time.asMilliseconds();
										packet.clear();
										packet << static_cast<int32_t>(Commands::START) << (rooms[i].clients[k].clientSalt & serverSalt) << sendTime;
										socket.send(packet, rooms[i].clients[k].ip.toString(), rooms[i].clients[k].port);
										startMessages.push_back(std::make_pair(rooms[i].clients[k], sendTime));
									}

									rooms[i].playing = true;
									rooms[i].lastPlayerEntered = clock.getElapsedTime();
									break;
								}
							}
						}

						pendingConfirmationClients.erase(pendingConfirmationClients.begin() + client);
					}
				}
				break;
			}
			case Commands::BYE_SERVER:
			{
				uint64_t salts;
				packet >> salts;
				
				//comprobamos si hay algún cliente con el mismo salt que el cliente recibido
				for (auto salt : clientSalts)
				{
					if (salts == (salt & serverSalt))
					{
						for (int i = 0; i < rooms.size(); i++)
						{
							for (int j = 0; j < rooms[i].clients.size(); j++)
							{
								if (rooms[i].clients[j].clientSalt == salt)
								{
									//si lo hay enviamos el comando acknowledge_bye_server al cliente, se lo comunicamos a los otros jugadores de la sala y eliminamos su salt del vector clientSalts
									packet.clear();
									packet << static_cast<int32_t>(Commands::ACKNOWLEDGE_BYE_SERVER) << (rooms[i].clients[j].clientSalt & serverSalt);
									socket.send(packet, rooms[i].clients[j].ip.toString(), rooms[i].clients[j].port);

									std::cout << "Connection with " << rooms[i].clients[j].username << " has been lost" << std::endl;
									for (int k = 0; k < rooms[i].clients.size(); k++)
									{
										if (rooms[i].clients[k].clientSalt != salt)
										{
											packet.clear();
											packet << static_cast<int32_t>(Commands::PLAYER_DISCONNECTED) << (rooms[i].clients[k].clientSalt & serverSalt) << rooms[i].clients[j].username;
											socket.send(packet, rooms[i].clients[k].ip.toString(), rooms[i].clients[k].port);
											rooms[i].clients[k].lastMessageRecieved = clock.getElapsedTime();
											playAgainClients.push_back(rooms[i].clients[k]);
										}
									}

									for (int salt = 0; salt < clientSalts.size(); salt++)
									{
										if (clientSalts[salt] == rooms[i].clients[j].clientSalt)
										{
											clientSalts.erase(clientSalts.begin() + salt);
										}
									}

									rooms.erase(rooms.begin() + i);
									break;
								}
							}
						}
					}
				}
				break;			}
			default: break;
			}

			//actualizamos la variable que almacena el último tiempo de respuesta del jugador
			for (int i = 0; i < rooms.size(); i++)
			{
				for (int j = 0; j < rooms[i].clients.size(); j++)
				{
					if (rooms[i].clients[j].clientSalt == recievedClient.clientSalt)
					{
						rooms[i].clients[j].lastMessageRecieved = clock.getElapsedTime();
					}
				}
			}
		}

		//si no hemos recibido una respuesta al mensaje start antes de que passen 2 segundos, actualizamos el identificador de tiempo y reenviamos el mensaje
		for (int i = 0; i < startMessages.size(); i++)
		{
			if (time.asMilliseconds() - startMessages[i].second > START_SEND_RATE)
			{
				startMessages[i].second = time.asMilliseconds();
				packet.clear();
				packet << static_cast<int32_t>(Commands::START) << (startMessages[i].first.clientSalt & serverSalt) << startMessages[i].second;
				socket.send(packet, startMessages[i].first.ip.toString(), startMessages[i].first.port);
			}
		}

		//si una de las balas del vector bullets lleva más de 1 segundo desde que se nos ha notificado su existencia la eliminamos de dicho vector, si han pasado 50 milisegundos desde la última vez que se ha notificado una de las balas del vector bullets se reenvia la notificación y se actualiza el tiempo de envio
		for (int i = 0; i < bullets.size(); i++)
		{
			if (time.asSeconds() - bullets[i].serverReceivedBulletReference.asSeconds() > BULLET_ERASE_REF)	bullets.erase(bullets.begin() + i);
			else if (time.asMilliseconds() - bullets[i].bulletSendReference.asMilliseconds() > BULLET_SEND_RATE)
			{
				packet.clear();
				packet << static_cast<int32_t>(Commands::NEW_REMOTE_BULLET) << (bullets[i].clientSalt & serverSalt) << bullets[i].position.x << bullets[i].position.y << bullets[i].direction.x << bullets[i].direction.y << bullets[i].bulletReference;
				socket.send(packet, bullets[i].clientIP, bullets[i].clientPort);
				bullets[i].bulletSendReference = clock.getElapsedTime();
			}
		}

		//si han pasado más de 7 segundos desde que se ha preguntado si el jugador quiere volver a jugar y aun no ha respondido, se reenvia el comando play_again
		if (!playAgainClients.empty())
		{
			for (int i = 0; i < playAgainClients.size(); i++)
			{
				if ((time.asSeconds() - playAgainClients[i].lastMessageRecieved.asSeconds()) > WAIT_FOR_PLAY_AGAIN_TIME)
				{
					packet.clear();
					packet << static_cast<int32_t>(Commands::PLAY_AGAIN) << (playAgainClients[i].clientSalt & serverSalt) << false;
					socket.send(packet, playAgainClients[i].ip.toString(), playAgainClients[i].port);
					playAgainClients[i].lastMessageRecieved = clock.getElapsedTime();
				}
			}
		}

		//comprobamos si han pasado más de 7 segundos desde que hemos asignado una nueva sala al jugador y aun no ha respondido
		if (!pendingConfirmationClients.empty())
		{
			for (int client = 0; client < pendingConfirmationClients.size(); client++)
			{
				if ((time.asSeconds() - pendingConfirmationClients[client].lastMessageRecieved.asSeconds()) > WAIT_FOR_PLAY_AGAIN_TIME)
				{
					for (int i = 0; i < rooms.size(); i++)
					{
						if (rooms[i].roomTag == pendingConfirmationClients[i].roomTag)
						{
							//si solo hay un jugador en la sala se reenvia la información al jugador
							if (rooms[i].numPlayers == 1)
							{
								packet.clear();
								packet << static_cast<int32_t>(Commands::NEW_GAME) << (pendingConfirmationClients[client].clientSalt & serverSalt) << pendingConfirmationClients[client].color << pendingConfirmationClients[client].position.x << pendingConfirmationClients[client].position.y << 0;
								socket.send(packet, recievedClient.ip.toString(), recievedClient.port);
							}
							//si hay más jugadores, se reenvia la informació al jugador y la pertinente información a los otros jugadores
							else
							{
								packet.clear();
								packet << static_cast<int32_t>(Commands::NEW_GAME) << (pendingConfirmationClients[client].clientSalt & serverSalt) << pendingConfirmationClients[client].color << pendingConfirmationClients[client].position.x << pendingConfirmationClients[client].position.y << rooms[i].numPlayers - 1;
								for (int j = 0; j < rooms[i].clients.size() - 1; j++)
								{
									packet << rooms[i].clients[j].username << rooms[i].clients[j].color << rooms[i].clients[j].position.x << rooms[i].clients[j].position.y;
								}
								socket.send(packet, recievedClient.ip.toString(), recievedClient.port);

								for (int k = 0; k < rooms[i].clients.size() - 1; k++)
								{
									packet.clear();
									packet << static_cast<int32_t>(Commands::NEW_PLAYER) << (rooms[i].clients[k].clientSalt & serverSalt) << pendingConfirmationClients[client].color << pendingConfirmationClients[client].username << pendingConfirmationClients[client].position.x << pendingConfirmationClients[client].position.y;
									socket.send(packet, rooms[i].clients[k].ip.toString(), rooms[i].clients[k].port);
								}
							}
							pendingConfirmationClients[i].lastMessageRecieved = clock.getElapsedTime();
						}						
					}
				}
			}
		}

		for (int i = 0; i < rooms.size(); i++)
		{
			//si alguna partida lleva más de 30 segundos en juego, enviamos el comando play_again a sus jugadores, los añadimos al vector playAgainClients y eliminamos la sala
			if (rooms[i].playing && (time.asSeconds() - rooms[i].lastPlayerEntered.asSeconds()) > MAX_TIME_PLAYING)
			{
				for (int j = 0; j < rooms[i].clients.size(); j++)
				{
					packet.clear();
					packet << static_cast<int32_t>(Commands::PLAY_AGAIN) << (rooms[i].clients[j].clientSalt & serverSalt) << true;
					socket.send(packet, rooms[i].clients[j].ip.toString(), rooms[i].clients[j].port);
					rooms[i].clients[j].lastMessageRecieved = clock.getElapsedTime();
					playAgainClients.push_back(rooms[i].clients[j]);
				}

				rooms.erase(rooms.begin() + i);
			}
			else
			{
				for (int j = 0; j < rooms[i].clients.size(); j++)
				{
					//si no recibimos nada por parte de un jugador en más de 30 segundos, procedemos a hacer una desconexión limpia desde servidor
					if (time.asSeconds() - rooms[i].clients[j].lastMessageRecieved.asSeconds() > WAIT_BEFORE_DESCONNECTING_TIME)
					{
						packet.clear();
						packet << static_cast<int32_t>(Commands::BYE_CLIENT) << (rooms[i].clients[j].clientSalt & serverSalt);
						socket.send(packet, rooms[i].clients[j].ip.toString(), rooms[i].clients[j].port);

						//informamos de la desconexión del jugador a los otros jugadores de la sala, los añadimos al vector de playAgainClients, eliminamos el salt de los jugadores del vector clientSlats, y eliminamos la sala
						std::cout << "Connection with " << rooms[i].clients[j].username << " has been lost" << std::endl;
						for (int k = 0; k < rooms[i].clients.size(); k++)
						{							
							if (rooms[i].clients[k].clientSalt != rooms[i].clients[j].clientSalt)
							{
								packet.clear();
								packet << static_cast<int32_t>(Commands::PLAYER_DISCONNECTED) << (rooms[i].clients[k].clientSalt & serverSalt) << rooms[i].clients[j].username;
								socket.send(packet, rooms[i].clients[k].ip.toString(), rooms[i].clients[k].port);
								rooms[i].clients[k].lastMessageRecieved = clock.getElapsedTime();
								playAgainClients.push_back(rooms[i].clients[k]);
							}
						}

						for (int salt = 0; salt < clientSalts.size(); salt++)
						{
							if (clientSalts[salt] == rooms[i].clients[j].clientSalt)
							{
								clientSalts.erase(clientSalts.begin() + salt);
							}
						}
						rooms.erase(rooms.begin() + i);
						break;
					}
					//si hace más de 50 milisegundos desde la última posición validada y aún tenemos posiciones a validar
					else if (!rooms[i].clients[j].positionsToValidate.empty() && time.asMilliseconds() - rooms[i].clients[j].lastTimeValidated.asMilliseconds() > POSITION_SEND_RATE)
					{
						bool validated = true;

						//comprobamos si la posición es válida
						if (rooms[i].clients[j].positionsToValidate.back().first.x < 3 * BLOCK_SIZE || rooms[i].clients[j].positionsToValidate.back().first.y < 3 * BLOCK_SIZE || rooms[i].clients[j].positionsToValidate.back().first.x >(W_WINDOW_PX - 5 * BLOCK_SIZE) || rooms[i].clients[j].positionsToValidate.back().first.y >(H_WINDOW_PX - 5 * BLOCK_SIZE)) validated = false;
						
						//enviamos al jugador la última posición válida y si esta es la posición por la que nos ha preguntado el jugador, se la enviamos a los otros jugadores de laa sala
						for (int k = 0; k < rooms[i].clients.size(); k++)
						{
							if (rooms[i].clients[k].ip == rooms[i].clients[j].ip && rooms[i].clients[k].port == rooms[i].clients[j].port)
							{
								packet.clear();
								packet << static_cast<int32_t>(Commands::VALIDATE_POSITION) << (rooms[i].clients[k].clientSalt & serverSalt);
								if (validated)
								{
									packet << rooms[i].clients[j].positionsToValidate.back().second;
									rooms[i].clients[k].lastPositionID = rooms[i].clients[j].positionsToValidate.back().second;
									rooms[i].clients[k].position = Vector2(rooms[i].clients[j].positionsToValidate.back().first.x, rooms[i].clients[j].positionsToValidate.back().first.y);
								}
								else
								{
									packet << rooms[i].clients[k].lastPositionID;
								}
								packet << validated;
								socket.send(packet, rooms[i].clients[k].ip.toString(), rooms[i].clients[k].port);
							}
							else if (validated)
							{
								packet.clear();
								packet << static_cast<int32_t>(Commands::UPDATE_POSITION) << (rooms[i].clients[k].clientSalt & serverSalt) << rooms[i].clients[j].positionsToValidate.back().second << rooms[i].clients[j].positionsToValidate.back().first.x << rooms[i].clients[j].positionsToValidate.back().first.y;
								socket.send(packet, rooms[i].clients[k].ip.toString(), rooms[i].clients[k].port);
							}
						}

						rooms[i].clients[j].positionsToValidate.clear();
						rooms[i].clients[j].lastTimeValidated = clock.getElapsedTime();
					}
				}
			}
		}
	}

	return 0;
}
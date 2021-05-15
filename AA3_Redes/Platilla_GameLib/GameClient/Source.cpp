#pragma once
#include <PlayerInfo.h>
#include <SFML\Network.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <fcntl.h>
#include <Types.h>

#define CLIENT_IP "localhost"
#define SERVER_IP "localhost"

//struct para almacenar la info del server
struct Server
{
	sf::IpAddress ip;
	unsigned short port;
	uint64_t serverSalt;
};

//struct para almacenar la info de las balas
struct Bullet
{
	sf::CircleShape shape;
	sf::Vector2f direction = sf::Vector2f(0.0f, 0.0f);
	float speed = 2.0f;
	float bulletReference;
};

//busca un número aleatorio, del uno al cien, que determinará si se pierde o no un paquete
int GetRandomPacketPercentage()
{
	return rand() % 100;
}

//hace el lerp entre dos posiciones dades y en el tiempo dado
sf::Vector2f lerp(sf::Vector2f start, sf::Vector2f end, float time)
{
	return start * (1 - time) + end * time;
}

int main()
{
	srand(time(NULL));
	PlayerInfo playerInfo;

	sf::RenderWindow _window(sf::VideoMode(W_WINDOW_PX, H_WINDOW_PX), "Never Split The Party");
	sf::RectangleShape shape(sf::Vector2f(BLOCK_SIZE, BLOCK_SIZE));
	sf::RectangleShape player(sf::Vector2f(2 * BLOCK_SIZE, 2 * BLOCK_SIZE));
	player.setFillColor(sf::Color::Transparent);
	sf::RectangleShape playerIndicator(sf::Vector2f(BLOCK_SIZE/2, BLOCK_SIZE/2));
	playerIndicator.setFillColor(sf::Color::Transparent);
	sf::RectangleShape player2(sf::Vector2f(2 * BLOCK_SIZE, 2 * BLOCK_SIZE));
	std::queue<std::pair<sf::Vector2f, int>> player2Positions;
	player2.setFillColor(sf::Color::Transparent);
	bool player2Connected = false;

	sf::UdpSocket socket;
	sf::Packet packet;
	int aux;

	//preguntamos el username al jugador
	std::cout << "Welcome, what would you like to use as your username" << std::endl;
	std::cin >> playerInfo.username;

	//enviamos el comando hello al servidor aplicando un porcentage de pérdida de paquetes
	packet << static_cast<int32_t>(Commands::HELLO) << playerInfo.playerSalt << playerInfo.username;
	socket.send(packet, SERVER_IP, SERVER_PORT);

	aux = GetRandomPacketPercentage();
	if (aux > LOST_PACKETS_PERCENTAGE)
	{
		socket.send(packet, SERVER_IP, SERVER_PORT);
	}
	else
	{
		std::cout << "Packet lost" << std::endl;
	}

	unsigned short recievedPort;
	sf::IpAddress recievedIP;
	socket.receive(packet, recievedIP, recievedPort);

	Server server;
	bool connectedToServer = false;
	socket.setBlocking(false);
	int lastPackedID = 0;
	Commands command;
	sf::Clock clock;
	sf::Time time = clock.getElapsedTime();
	sf::Time clientResponseTime = clock.getElapsedTime();
	sf::Time positionTime = clock.getElapsedTime(); 
	sf::Time lastBulletShotTime = clock.getElapsedTime();
	std::vector<std::pair<sf::Vector2f, int>> playerPositions;
	std::vector<Bullet> bullets;
	std::queue<Bullet> bulletsToSend;
	std::vector<std::pair<Bullet, sf::Time>> bulletsToConfirm;

	while (_window.isOpen())
	{
		time = clock.getElapsedTime();
		packet.clear();
		sf::Event event;
		bool playerMoved = false;
		//mientras la ventana esté abierta detectamos el input que recibe
		while (_window.pollEvent(event))
		{
			switch (event.type)
			{
			//si se cierra la ventana se procede a hacer la desconexión limpia desde cliente
			case sf::Event::Closed:
				system("CLS");
				std::cout << "Connection with server has been lost :(" << std::endl;
				packet.clear();
				packet << static_cast<int32_t>(Commands::BYE_SERVER) << (playerInfo.playerSalt & server.serverSalt);
				socket.send(packet, SERVER_IP, SERVER_PORT);
				_window.close();
				break;
			case sf::Event::KeyPressed:
				//si se pulsa la tecla escape se procede a hacer la desconexión limpia desde cliente
				if (event.key.code == sf::Keyboard::Escape)
				{
					system("CLS");
					std::cout << "Connection with server has been lost :(" << std::endl;
					packet.clear();
					packet << static_cast<int32_t>(Commands::BYE_SERVER) << (playerInfo.playerSalt & server.serverSalt);
					socket.send(packet, SERVER_IP, SERVER_PORT);
					_window.close();
				}
				//si se pulsa la tecla a, w, d, o s, y ya nos hemos conectado al servidor, comprobamos si el movimiento indicado es factible y añadimos la posición resultante al vector playerPositions
				if (event.key.code == sf::Keyboard::A)
				{
					if (_window.hasFocus() && connectedToServer)
					{
						if (player.getPosition().x > 3 * BLOCK_SIZE)
							player.setPosition(player.getPosition() + sf::Vector2f(-5, 0));
						else player.setPosition(3* BLOCK_SIZE, player.getPosition().y);
						playerPositions.push_back(std::make_pair(player.getPosition(), playerPositions.back().second + 1));
					}
				}
				else if (event.key.code == sf::Keyboard::W)
				{
					if (_window.hasFocus() && connectedToServer)
					{
						if (player.getPosition().y > 3 * BLOCK_SIZE)
							player.setPosition(player.getPosition() + sf::Vector2f(0, -5));
						else player.setPosition(player.getPosition().x, 3 * BLOCK_SIZE);
						playerPositions.push_back(std::make_pair(player.getPosition(), playerPositions.back().second + 1));
					}
				}
				else if (event.key.code == sf::Keyboard::D)
				{
					if (_window.hasFocus() && connectedToServer)
					{
						if (player.getPosition().x + player.getLocalBounds().width < W_WINDOW_PX - 3 * BLOCK_SIZE)
							player.setPosition(player.getPosition() + sf::Vector2f(5, 0));
						else player.setPosition(W_WINDOW_PX - 3 * BLOCK_SIZE - player.getLocalBounds().width, player.getPosition().y);
						playerPositions.push_back(std::make_pair(player.getPosition(), playerPositions.back().second + 1));
					}
				}
				else if (event.key.code == sf::Keyboard::S)
				{
					if (_window.hasFocus() && connectedToServer)
					{
						if (player.getPosition().y + player.getLocalBounds().height < H_WINDOW_PX - 3 * BLOCK_SIZE)
							player.setPosition(player.getPosition() + sf::Vector2f(0, 5));
						else player.setPosition(player.getPosition().x, H_WINDOW_PX - 3 * BLOCK_SIZE - player.getLocalBounds().height);
						playerPositions.push_back(std::make_pair(player.getPosition(), playerPositions.back().second + 1));
					}
				}
				//si se pulsa una de las flechas, ya nos hemos conectado al servidor y han pasado más de 200 milisegundos desde la última bala disparada, creamos una bala y la añadimos a los vectores bullets y bulletsToSend
				else if (event.key.code == sf::Keyboard::Left)
				{
					if (connectedToServer && time.asMilliseconds() - lastBulletShotTime.asMilliseconds() > BULLET_SHOOT_RATE)
					{
						Bullet newBullet;
						newBullet.direction = sf::Vector2f(-newBullet.speed, 0.0f);
						newBullet.shape = sf::CircleShape(BLOCK_SIZE/2);
						newBullet.shape.setPosition(player.getPosition() + sf::Vector2f(BLOCK_SIZE/2, BLOCK_SIZE/2));
						newBullet.shape.setFillColor(player.getFillColor());
						newBullet.bulletReference = time.asSeconds();
						bullets.push_back(newBullet);
						bulletsToSend.push(newBullet);
						lastBulletShotTime = clock.getElapsedTime();
					}
				}
				else if (event.key.code == sf::Keyboard::Up)
				{
					if (connectedToServer && time.asMilliseconds() - lastBulletShotTime.asMilliseconds() > BULLET_SHOOT_RATE)
					{
						Bullet newBullet;
						newBullet.direction = sf::Vector2f(0.0f, -newBullet.speed);
						newBullet.shape = sf::CircleShape(BLOCK_SIZE/2);
						newBullet.shape.setPosition(player.getPosition() + sf::Vector2f(BLOCK_SIZE / 2, BLOCK_SIZE / 2));
						newBullet.shape.setFillColor(player.getFillColor());
						newBullet.bulletReference = time.asSeconds();
						bullets.push_back(newBullet);
						bulletsToSend.push(newBullet);
						lastBulletShotTime = clock.getElapsedTime();
					}
				}
				else if (event.key.code == sf::Keyboard::Right)
				{
					if (connectedToServer && time.asMilliseconds() - lastBulletShotTime.asMilliseconds() > BULLET_SHOOT_RATE)
					{
						Bullet newBullet;
						newBullet.direction = sf::Vector2f(newBullet.speed, 0.0f);
						newBullet.shape = sf::CircleShape(BLOCK_SIZE/2);
						newBullet.shape.setPosition(player.getPosition() + sf::Vector2f(BLOCK_SIZE / 2, BLOCK_SIZE / 2));
						newBullet.shape.setFillColor(player.getFillColor());
						newBullet.bulletReference = time.asSeconds();
						bullets.push_back(newBullet);
						bulletsToSend.push(newBullet);
						lastBulletShotTime = clock.getElapsedTime();
					}
				}
				else if (event.key.code == sf::Keyboard::Down)
				{
					if (connectedToServer && time.asMilliseconds() - lastBulletShotTime.asMilliseconds() > BULLET_SHOOT_RATE)
					{
						Bullet newBullet;
						newBullet.direction = sf::Vector2f(0.0f, newBullet.speed);
						newBullet.shape = sf::CircleShape(BLOCK_SIZE/2);
						newBullet.shape.setPosition(player.getPosition() + sf::Vector2f(BLOCK_SIZE / 2, BLOCK_SIZE / 2));
						newBullet.shape.setFillColor(player.getFillColor());
						newBullet.bulletReference = time.asSeconds();
						bullets.push_back(newBullet);
						bulletsToSend.push(newBullet);
						lastBulletShotTime = clock.getElapsedTime();
					}
				}
				break;
			}
		}

		//actualizamos la posición de las balas y eliminamos aquellas que estén fuera de los límites del campo de juego
		for (int i = 0; i < bullets.size(); i++)
		{
			bullets[i].shape.setPosition(lerp(bullets[i].shape.getPosition(), bullets[i].shape.getPosition() + bullets[i].direction, 1.f));
			if (bullets[i].shape.getPosition().x < 2 * BLOCK_SIZE || bullets[i].shape.getPosition().y < 2 * BLOCK_SIZE || bullets[i].shape.getPosition().x >(W_WINDOW_PX - 4 * BLOCK_SIZE) || bullets[i].shape.getPosition().y >(H_WINDOW_PX - 4 * BLOCK_SIZE)) bullets.erase(bullets.begin() + i);
		}

		//si el otro jugador tiene que moverse, hacemos el lerp desde su posición actual hasta la primera posición del vector player2Positions, si ha llegado a esa posición la eliminamos del vector
		if (!player2Positions.empty())
		{
			player2.setPosition(lerp(player2.getPosition(), player2Positions.front().first, 1.f));
			if (player2.getPosition() == player2Positions.front().first) player2Positions.pop();
		}

		//si detectamos la recepción de un paquete por parte del servidor lo procesamos
		if (socket.receive(packet, server.ip, server.port) == sf::Socket::Done)
		{
			packet >> aux;
			command = (Commands)aux;
			switch (command)
			{
			case Commands::CHALLENGE:
			{
				std::string question;
				std::string answer;

				uint64_t recievedPlayerSalt;
				packet >> question >> recievedPlayerSalt;
				//si el salt recibido coincide con el de este jugador
				if (recievedPlayerSalt == playerInfo.playerSalt)
				{
					packet >> server.serverSalt;
					//hacemos el challenge y esperamos la respuesta
					std::cout << question << std::endl;
					std::cin >> answer;
					//enviamos la respuesta al server aplicando la pérdida de paquetes
					packet.clear();
					packet << static_cast<int32_t>(Commands::CHALLENGE_RESPONSE) << answer << (playerInfo.playerSalt & server.serverSalt);
					aux = GetRandomPacketPercentage();
					if (aux > LOST_PACKETS_PERCENTAGE)
					{
						socket.send(packet, SERVER_IP, SERVER_PORT);
						connectedToServer = true;
					}
					else
					{
						std::cout << "The packet has been lost" << std::endl;
					}
				}
				break;
			}
			case Commands::WELCOME:
			{
				uint64_t salts;
				packet >> salts;
				//si los salts coinciden con el de este jugador y el del server
				if (salts == (playerInfo.playerSalt & server.serverSalt))
				{
					system("CLS");
					int otherPlayers;
					int aux1, aux2;
					packet >> aux >> aux1 >> aux2 >> otherPlayers;
					//nos guardamos el color y posición recibidos
					switch (aux)
					{
					case 0:
					{
						player.setFillColor(sf::Color::Red);
						break;
					}
					case 1:
					{
						player.setFillColor(sf::Color::Blue);
						break;
					}
					case 2:
					{
						player.setFillColor(sf::Color::Green);
						break;
					}
					case 3:
					{
						player.setFillColor(sf::Color::Yellow);
						break;
					}
					default:
						break;
					}
					player.setPosition(aux1, aux2);
					playerPositions.push_back(std::make_pair(player.getPosition(), 0));

					//si hay más jugadores en la sala, nos guardamos su información
					if (otherPlayers != 0)
					{
						for (int i = 0; i < otherPlayers; i++)
						{
							PlayerInfo aux;
							packet >> aux.username >> aux.color >> aux.position.x >> aux.position.y;
							switch (aux.color)
							{
							case 0:
							{
								player2.setFillColor(sf::Color::Red);
								break;
							}
							case 1:
							{
								player2.setFillColor(sf::Color::Blue);
								break;
							}
							case 2:
							{
								player2.setFillColor(sf::Color::Green);
								break;
							}
							case 3:
							{
								player2.setFillColor(sf::Color::Yellow);
								break;
							}
							default:
								break;
							}
							player2.setPosition(aux.position.x, aux.position.y);
						}
						std::cout << "The game will start shortly" << std::endl;
						player2Connected = true;
					}
					//si no los hay, indicamos al jugador que estamos a la espera de más jugadores
					else
					{
						std::cout << "Waiting for players" << std::endl;
					}

					connectedToServer = true;
					//enviamos el comando acknowledge_welcome al servidor aplicando la pérdida de paquetes
					packet.clear();
					packet << static_cast<int32_t>(Commands::ACKNOWLEDGE_WELCOME) << (playerInfo.playerSalt & server.serverSalt);
					aux = GetRandomPacketPercentage();
					if (aux > LOST_PACKETS_PERCENTAGE)
					{
						socket.send(packet, SERVER_IP, SERVER_PORT);
					}
					else
					{
						std::cout << "Packet lost" << std::endl;
					}
				}
				break;
			}
			case Commands::NEW_PLAYER:
			{
				uint64_t salts;
				packet >> salts;
				//si los salts coinciden con el de este jugador y el del server
				if (salts == (playerInfo.playerSalt & server.serverSalt))
				{
					PlayerInfo aux;
					packet >> aux.color >> aux.username >> aux.position.x >> aux.position.y;
					//nos guardamos su información del nuevo jugador
					switch (aux.color)
					{
					case 0:
					{
						player2.setFillColor(sf::Color::Red);
						break;
					}
					case 1:
					{
						player2.setFillColor(sf::Color::Blue);
						break;
					}
					case 2:
					{
						player2.setFillColor(sf::Color::Green);
						break;
					}
					case 3:
					{
						player2.setFillColor(sf::Color::Yellow);
						break;
					}
					default:
						break;
					}
					player2.setPosition(aux.position.x, aux.position.y);
					std::cout << "The game will start shortly" << std::endl;
					player2Connected = true;
				}
				break;
			}
			case Commands::START:
			{
				uint64_t salts;
				float reference;
				packet >> salts;
				//si los salts coinciden con el de este jugador y el del server
				if (salts == (playerInfo.playerSalt & server.serverSalt))
				{
					packet >> reference;
					//enviar el comando acknowledge_start con el identificador de tiempo del mensaje y aplicar la pérdida de paquetes
					packet.clear();
					packet << static_cast<int32_t>(Commands::ACKNOWLEDGE_START) << (playerInfo.playerSalt & server.serverSalt) << reference;
					aux = GetRandomPacketPercentage();
					if (aux > LOST_PACKETS_PERCENTAGE)
					{
						socket.send(packet, SERVER_IP, SERVER_PORT);
					}
					else
					{
						std::cout << "The packet has been lost" << std::endl;
					}					
				}
				break;
			}
			case Commands::READY:
			{
				uint64_t salts;
				packet >> salts;
				//si los salts coinciden con el de este jugador y el del server, avisar del inicio de partida
				if (salts == (playerInfo.playerSalt & server.serverSalt))
				{
					std::cout << "The game has started" << std::endl;
				}
				break;
			}
			case Commands::VALIDATE_POSITION:
			{
				uint64_t salts;
				packet >> salts;
				//si los salts coinciden con el de este jugador y el del server
				if (salts == (playerInfo.playerSalt & server.serverSalt))
				{
					bool validated;
					packet >> aux >> validated;
					//si el servidor ha verificado la posicion enviada y ésta es más reciente que la última posición validada
					if (validated)
					{
						if (aux > lastPackedID)
						{
							//eliminamos todas las posiciones prévias a esta
							for (auto it = playerPositions.begin(); it != playerPositions.end();)
							{
								if (it->second < aux)
								{
									it = playerPositions.erase(it);
								}
								else break;
							}
						}
					}
					else
					{
						//si no la ha verificado, eliminamos todas las posiciones posteriores a la última posición validada y ponemos al jugador en esta posición
						for (auto it = playerPositions.begin(); it != playerPositions.end();)
						{
							if (it->second != aux)
							{
								it = playerPositions.erase(it);
							}
							else
							{
								player.setPosition(it->first);
								it++;
							}
						}
					}
					lastPackedID = aux;
				}
				break;
			}
			case Commands::UPDATE_POSITION:
			{
				uint64_t salts;
				packet >> salts;
				//si los salts coinciden con el de este jugador y el del server
				if (salts == (playerInfo.playerSalt & server.serverSalt))
				{
					packet >> aux;

					//si no tenemos guardada ninguna posición del otro jugador o la posición recibida es más reciente que la última posición guardada, nos guardamos la posición recibida
					if (player2Positions.empty() || aux > player2Positions.back().second)
					{
						float x, y;
						packet >> x >> y;
						player2Positions.push(std::make_pair(sf::Vector2f(x, y), aux));
					}
				}
				break;
			}
			case Commands::NEW_REMOTE_BULLET:
			{
				uint64_t salts;
				packet >> salts;
				//si los salts coinciden con el de este jugador y el del server
				if (salts == (playerInfo.playerSalt & server.serverSalt))
				{
					float x, y, xDir, yDir, bulletReference;
					packet >> x >> y >> xDir >> yDir >> bulletReference;
					bool bulletExists = false;
					//comprobamos si la bala ya está en el vector bulletsToConfirm o en el vector bullets
					for (int i = 0; i < bulletsToConfirm.size(); i++)
					{
						if (bulletsToConfirm[i].first.bulletReference == bulletReference) {
							bulletExists = true;
							break;
						}
					}
					for (int i = 0; i < bullets.size(); i++)
					{
						if (bullets[i].bulletReference == bulletReference) {
							bulletExists = true;
							break;
						}
					}
					//si no lo está, creamos la bala, la añadimos a los vectores bulletsToConfirm y bullets, y enviamos el acknowledge de la bala al servidor aplicando la pérdida de paquetes
					if (!bulletExists) {
						Bullet newBullet;
						newBullet.direction = sf::Vector2f(xDir, yDir);
						newBullet.shape.setRadius(BLOCK_SIZE/2);
						newBullet.shape.setPosition(x, y);
						newBullet.shape.setFillColor(player2.getFillColor());
						newBullet.bulletReference = bulletReference;
						bullets.push_back(newBullet);
						bulletsToConfirm.push_back(std::make_pair(newBullet, time));
						packet.clear();
						packet << static_cast<int32_t>(Commands::ACKNOWLEDGE_REMOTE_BULLET) << (playerInfo.playerSalt & server.serverSalt) << bulletReference;
						aux = GetRandomPacketPercentage();
						if (aux > LOST_PACKETS_PERCENTAGE)
						{
							socket.send(packet, SERVER_IP, SERVER_PORT);
						}
						else
						{
							std::cout << "Packet lost" << std::endl;
						}
					}
				}
				break;
			}
			case Commands::ACKNOWLEDGE_BULLET:
			{
				uint64_t salts;
				packet >> salts;
				//si los salts coinciden con el de este jugador y el del server
				if (salts == (playerInfo.playerSalt & server.serverSalt))
				{
					float bulletReference;
					packet >> bulletReference;
					//si la bala está en el vector bulletsToSend, la eliminamos
					if (!bulletsToSend.empty() && bulletsToSend.front().bulletReference == bulletReference)
					{
						bulletsToSend.pop();
					}
				}
				break;
			}
			case Commands::PLAY_AGAIN:
			{
				uint64_t salts;
				packet >> salts;
				//si los salts coinciden con el de este jugador y el del server
				if (salts == (playerInfo.playerSalt & server.serverSalt))
				{
					bool timeMessage;
					packet >> timeMessage;
					char answer;

					player.setFillColor(sf::Color::Transparent);
					player2.setFillColor(sf::Color::Transparent);
					player2Connected = false;
					playerPositions.clear();
					if (timeMessage)
					{
						system("CLS");
						std::cout << "Time's up!" << std::endl;
					}
					//preguntamos al jugador si quiere volver a jugar y esperamos su respuesta
					std::cout << "Do you want to play again? (y/n)" << std::endl;
					std::cin >> answer;
					while (answer != 'y' && answer != 'n')
					{
						std::cout << "Do you want to play again? (y/n)" << std::endl;
						std::cin >> answer;
					}

					//enviamos la respuesta al servidor aplicando la pérdida de paquetes
					packet.clear();
					packet << static_cast<int32_t>(Commands::PLAY_AGAIN_ANSWER) << (playerInfo.playerSalt & server.serverSalt) << (answer == 'y');
					aux = GetRandomPacketPercentage();
					if (aux > LOST_PACKETS_PERCENTAGE)
					{
						socket.send(packet, SERVER_IP, SERVER_PORT);
					}
					else
					{
						std::cout << "Packet lost" << std::endl;
					}
				}
				break;
			}
			case Commands::NEW_GAME:
			{
				uint64_t salts;
				packet >> salts;
				//si los salts coinciden con el de este jugador y el del server nos guardaremos la información siguiendo la misma lógica que en el case welcome, pero enviando el comando acknowledge_new_game en lugar de acknowledge_welcome
				if (salts == (playerInfo.playerSalt & server.serverSalt))
				{
					system("CLS");
					int otherPlayers;
					int aux1, aux2;
					packet >> aux >> aux1 >> aux2 >> otherPlayers;
					switch (aux)
					{
					case 0:
					{
						player.setFillColor(sf::Color::Red);
						break;
					}
					case 1:
					{
						player.setFillColor(sf::Color::Blue);
						break;
					}
					case 2:
					{
						player.setFillColor(sf::Color::Green);
						break;
					}
					case 3:
					{
						player.setFillColor(sf::Color::Yellow);
						break;
					}
					default:
						break;
					}
					player.setPosition(aux1, aux2);
					playerPositions.push_back(std::make_pair(player.getPosition(), 0));

					if (otherPlayers != 0)
					{
						for (int i = 0; i < otherPlayers; i++)
						{
							PlayerInfo aux;
							packet >> aux.username >> aux.color >> aux.position.x >> aux.position.y;
							switch (aux.color)
							{
							case 0:
							{
								player2.setFillColor(sf::Color::Red);
								break;
							}
							case 1:
							{
								player2.setFillColor(sf::Color::Blue);
								break;
							}
							case 2:
							{
								player2.setFillColor(sf::Color::Green);
								break;
							}
							case 3:
							{
								player2.setFillColor(sf::Color::Yellow);
								break;
							}
							default:
								break;
							}
							player2.setPosition(aux.position.x, aux.position.y);
						}
						std::cout << "The game will start shortly" << std::endl;
						player2Connected = true;
					}
					else
					{
						std::cout << "Waiting for players" << std::endl;
					}

					connectedToServer = true;
					packet.clear();
					packet << static_cast<int32_t>(Commands::ACKNOWLEDGE_NEW_GAME) << (playerInfo.playerSalt & server.serverSalt);
					aux = GetRandomPacketPercentage();
					if (aux > LOST_PACKETS_PERCENTAGE)
					{
						socket.send(packet, SERVER_IP, SERVER_PORT);
					}
					else
					{
						std::cout << "Packet lost" << std::endl;
					}
				}
				break;
			}
			case Commands::PLAYER_DISCONNECTED:
			{
				uint64_t salts;
				packet >> salts;
				//si los salts coinciden con el de este jugador y el del server
				if (salts == (playerInfo.playerSalt & server.serverSalt))
				{
					std::string name;
					packet >> name;

					//si este jugador no es el jugador que se ha desconectado
					if (name != playerInfo.username)
					{
						char answer;
						player.setFillColor(sf::Color::Transparent);
						player2.setFillColor(sf::Color::Transparent);
						player2Connected = false;
						playerPositions.clear();
						//informamos de la desconexión del otro jugador, preguntamos si quiere volver a jugar y enviamos la respuesta al server aplicando la pérdida de paquetes
						system("CLS");
						std::cout << name << " has disconnected from the server" << std::endl;
						std::cout << "Do you want to play again? (y/n)" << std::endl;
						std::cin >> answer;
						while (answer != 'y' && answer != 'n')
						{
							std::cout << "Do you want to play again? (y/n)" << std::endl;
							std::cin >> answer;
						}

						packet.clear();
						packet << static_cast<int32_t>(Commands::PLAY_AGAIN_ANSWER) << (playerInfo.playerSalt & server.serverSalt) << (answer == 'y');
						aux = GetRandomPacketPercentage();
						if (aux > LOST_PACKETS_PERCENTAGE)
						{
							socket.send(packet, SERVER_IP, SERVER_PORT);
						}
						else
						{
							std::cout << "Packet lost" << std::endl;
						}
					}
				}
				break;
			}
			case Commands::BYE_CLIENT:
			{
				uint64_t salts;
				packet >> salts;
				//si los salts coinciden con el de este jugador y el del server, informamos al jugador que se ha perdido la conexión con el servidor y hacemos el acknowledge_bye_client
				if (salts == (playerInfo.playerSalt & server.serverSalt))
				{
					system("CLS");
					std::cout << "Connection with server has been lost :(" << std::endl;
					packet.clear();
					packet << static_cast<int32_t>(Commands::ACKNOWLEDGE_BYE_CLIENT) << (playerInfo.playerSalt & server.serverSalt);
					socket.send(packet, SERVER_IP, SERVER_PORT);
					_window.close();
				}
				break;
			}
			default: break;
			}
			clientResponseTime = clock.getElapsedTime();
		}

		packet.clear();

		//si aún no estamos conectados al servidor y no recibimos nada por su parte en más de 2 segundos, reenviamos el comando hello aplicando la pérdida de paquetes
		if (!connectedToServer && time.asSeconds() - clientResponseTime.asSeconds() > WAIT_FOR_CONNECTION_TIME)
		{
			packet << static_cast<int32_t>(Commands::HELLO) << playerInfo.playerSalt << playerInfo.username;
			aux = GetRandomPacketPercentage();
			if (aux > LOST_PACKETS_PERCENTAGE)
			{
				socket.send(packet, SERVER_IP, SERVER_PORT);
			}
			else
			{
				std::cout << "Packet lost" << std::endl;
			}
			clientResponseTime = clock.getElapsedTime();
		}
		//si estamos conectados al servidor, han pasado más de 50 milisegundos desde la última posición enviada y la última posición enviada es diferente a la posición más reciente del jugador
		else if (connectedToServer && time.asMilliseconds() - positionTime.asMilliseconds() > POSITION_SEND_RATE && playerPositions.back().second != lastPackedID)
		{
			//enviamos la posición más reciente al servidor aplicando la pérdida de paquetes
			clientResponseTime = clock.getElapsedTime();
			positionTime = clock.getElapsedTime();
			packet << static_cast<int32_t>(Commands::NEW_POSITION) << (playerInfo.playerSalt & server.serverSalt) << playerPositions.back().second << player.getPosition().x << player.getPosition().y;
			aux = GetRandomPacketPercentage();
			if (aux > LOST_PACKETS_PERCENTAGE)
			{
				socket.send(packet, SERVER_IP, SERVER_PORT);
			}
			else
			{
				std::cout << "Packet lost" << std::endl;
			}
		}
		
		//si estamos conectados al servidor y aún hay balas que enviarle, enviamos la primera bala del vector bulletToSend al servidor aplicando la pérdida de paquetes
		if (connectedToServer && !bulletsToSend.empty())
		{
			packet.clear();
			packet << static_cast<int32_t>(Commands::NEW_BULLET) << (playerInfo.playerSalt & server.serverSalt) << bulletsToSend.front().shape.getPosition().x << bulletsToSend.front().shape.getPosition().y << bulletsToSend.front().direction.x << bulletsToSend.front().direction.y << bulletsToSend.front().bulletReference;
			aux = GetRandomPacketPercentage();
			if (aux > LOST_PACKETS_PERCENTAGE)
			{
				socket.send(packet, SERVER_IP, SERVER_PORT);
			}
			else
			{
				std::cout << "Packet lost" << std::endl;
			}
			clientResponseTime = clock.getElapsedTime();
		}

		//si estamos conectados al servidor y ha pasado más de un segundo desde que hemos recibido una bala del otro jugador, eliminamos esta bala del vecotr bulletsToConfirm
		if (connectedToServer && !bulletsToConfirm.empty())
		{
			for (int i = 0; i < bulletsToConfirm.size(); i++)
			{
				if (time.asSeconds() - bulletsToConfirm[i].second.asSeconds() > BULLET_ERASE_REF)
				{
					bulletsToConfirm.erase(bulletsToConfirm.begin() + i);
				}
			}
		}

		//si no recibimos nada por parte del servidor en más de 30 segundos, procedemos a hacer una desconexión limpia desde cliente
		if (connectedToServer && time.asSeconds() - clientResponseTime.asSeconds() > WAIT_BEFORE_DESCONNECTING_TIME)
		{
			system("CLS");
			std::cout << "Connection with server has been lost :(" << std::endl;
			packet.clear();
			packet << static_cast<int32_t>(Commands::BYE_SERVER) << (playerInfo.playerSalt & server.serverSalt);
			aux = GetRandomPacketPercentage();
			if (aux > LOST_PACKETS_PERCENTAGE)
			{
				socket.send(packet, SERVER_IP, SERVER_PORT);
			}
			else
			{
				std::cout << "Packet lost" << std::endl;
			}
		}

		playerIndicator.setPosition(player.getPosition() + sf::Vector2f(0.75f * BLOCK_SIZE, - 0.75f *BLOCK_SIZE));
		playerIndicator.setFillColor(player.getFillColor());

		_window.clear();

		//dibujamos el campo de juego
		for (int i = 0; i < W_WINDOW_PX / BLOCK_SIZE; i++)
		{
			for (int j = 0; j < H_WINDOW_PX / BLOCK_SIZE; j++)
			{
				if (i < 2 || i > W_WINDOW_PX / BLOCK_SIZE - 3 || j < 2 || j > H_WINDOW_PX / BLOCK_SIZE - 3)
				{
					shape.setFillColor(sf::Color(5, 75, 105, 255));
					shape.setPosition(sf::Vector2f(i * BLOCK_SIZE, j * BLOCK_SIZE));
				}
				else if (i < 3 || i > W_WINDOW_PX / BLOCK_SIZE - 4 || j < 3 || j > H_WINDOW_PX / BLOCK_SIZE - 4)
				{
					shape.setFillColor(sf::Color(60, 25, 20, 255));
					shape.setPosition(sf::Vector2f(i * BLOCK_SIZE, j * BLOCK_SIZE));
				}
				else
				{
					shape.setFillColor(sf::Color(10, 40, 45, 255));
					shape.setPosition(sf::Vector2f(i * BLOCK_SIZE, j * BLOCK_SIZE));
				}

				_window.draw(shape);
			}
		}

		//dibujamos las balas
		for (int i = 0; i < bullets.size(); i++)
			_window.draw(bullets[i].shape);

		//dibujamos el otro jugador, si está conectado
		if (player2Connected) _window.draw(player2);

		//dibujamos el jugador y su indicador
		_window.draw(player);
		_window.draw(playerIndicator);

		_window.display();
	}
	
	system("pause");
	return 0;
}
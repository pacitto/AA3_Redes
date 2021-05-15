#pragma once
#include <SFML\Graphics.hpp>

class PlayerInfo
{
public:
	std::string username;
	sf::Vector2i position;
	int color;
	int lives;
	uint64_t playerSalt;
	PlayerInfo();
	~PlayerInfo();
};
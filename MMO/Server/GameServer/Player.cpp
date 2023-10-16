#include "pch.h"
#include "Player.h"

Player::Player()
{
	playerInfo = new Protocol::PlayerInfo();
	saveInfo = new Protocol::SaveInfo();
}

Player::~Player()
{
	delete playerInfo;
	delete saveInfo;
}
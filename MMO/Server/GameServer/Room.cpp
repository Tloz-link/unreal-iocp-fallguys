#include "pch.h"
#include "Room.h"
#include "Player.h"
#include "GameSession.h"

RoomRef GRoom = make_shared<Room>();

Room::Room()
{
}

Room::~Room()
{
}

bool Room::HandleEnterPlayer(PlayerRef player)
{
	bool success = EnterPlayer(player);

	int32 count = _players.size() - 1;

	player->playerInfo->set_x(count * 130.0f);
	player->playerInfo->set_y(0.f);
	player->playerInfo->set_z(55.f);
	player->playerInfo->set_yaw(-90.f);

	player->saveInfo->set_x(count * 130.0f);
	player->saveInfo->set_y(0.f);
	player->saveInfo->set_z(55.f);

	//player->playerInfo->set_x(Utils::GetRandom(0.f, 500.f));
	//player->playerInfo->set_y(Utils::GetRandom(0.f, 500.f));
	//player->playerInfo->set_z(55.f);

	// 입장 사실을 신입 플레이어에게 알린다
	{
		Protocol::S_ENTER_GAME enterGamePkt;
		enterGamePkt.set_success(success);

		Protocol::PlayerInfo* playerInfo = new Protocol::PlayerInfo();
		playerInfo->CopyFrom(*player->playerInfo);
		enterGamePkt.set_allocated_player(playerInfo);

		SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(enterGamePkt);
		if (auto session = player->session.lock())
			session->Send(sendBuffer);
	}

	// 입장 사실을 다른 플레이어에게 알린다
	{
		Protocol::S_SPAWN spawnPkt;

		Protocol::PlayerInfo* playerInfo = spawnPkt.add_players();
		playerInfo->CopyFrom(*player->playerInfo);

		SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(spawnPkt);
		Broadcast(sendBuffer, player->playerInfo->object_id());
	}

	// 기존 입장한 플레이어 목록을 신입 플레이어한테 전송해준다
	{
		Protocol::S_SPAWN spawnPkt;

		for (auto& item : _players)
		{
			Protocol::PlayerInfo* playerInfo = spawnPkt.add_players();
			playerInfo->CopyFrom(*item.second->playerInfo);
		}

		SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(spawnPkt);
		if (auto session = player->session.lock())
			session->Send(sendBuffer);
	}

	return success;
}

bool Room::HandleLeavePlayer(PlayerRef player)
{
	if (player == nullptr)
		return false;

	const uint64 objectId = player->playerInfo->object_id();
	bool success = LeavePlayer(objectId);

	// 퇴장 사실을 퇴장하는 플레이어에게 알린다
	{
		Protocol::S_LEAVE_GAME leaveGamePkt;

		SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(leaveGamePkt);
		if (auto session = player->session.lock())
			session->Send(sendBuffer);
	}

	// 퇴장 사실을 알린다
	{
		Protocol::S_DESPAWN despawnPkt;

		despawnPkt.add_objedct_ids(objectId);

		SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(despawnPkt);
		Broadcast(sendBuffer);

		if (auto session = player->session.lock())
			session->Send(sendBuffer);
	}

	return success;
}

void Room::HandleMove(Protocol::C_MOVE pkt)
{
	const uint64 objectId = pkt.info().object_id();
	if (_players.find(objectId) == _players.end())
		return;

	// 요청한 좌표가 말이 되는지 validation을 해야 함
	PlayerRef& player = _players[objectId];
	player->playerInfo->CopyFrom(pkt.info());

	// 이동
	{
		Protocol::S_MOVE movePkt;
		{
			Protocol::PlayerInfo* info = movePkt.mutable_info();
			info->CopyFrom(pkt.info());
		}

		SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(movePkt);
		Broadcast(sendBuffer);
	}
}

void Room::HandleSave(PlayerRef player)
{
	Protocol::S_SAVE savePkt;

	savePkt.set_object_id(player->playerInfo->object_id());
	{
		Protocol::SaveInfo* info = savePkt.mutable_info();
		info->CopyFrom(*player->saveInfo);
	}

	SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(savePkt);
	Broadcast(sendBuffer);
}

RoomRef Room::GetRoomRef()
{
	return static_pointer_cast<Room>(shared_from_this());
}

bool Room::EnterPlayer(PlayerRef player)
{
	// 이미 있다면 문제가 있다
	if (_players.find(player->playerInfo->object_id()) != _players.end())
		return false;

	_players.insert(make_pair(player->playerInfo->object_id(), player));

	player->room.store(GetRoomRef());
	return true;
}

bool Room::LeavePlayer(uint64 objectId)
{
	// 없다면 문제가 있다
	if (_players.find(objectId) == _players.end())
		return false;

	PlayerRef player = _players[objectId];
	player->room.store(weak_ptr<Room>());

	_players.erase(objectId);

	return true;
}

void Room::Broadcast(SendBufferRef sendBuffer, uint64 exceptId)
{
	for (auto& item : _players)
	{
		PlayerRef player = item.second;
		if (player->playerInfo->object_id() == exceptId)
			continue;

		if (GameSessionRef session = player->session.lock())
			session->Send(sendBuffer);
	}
}

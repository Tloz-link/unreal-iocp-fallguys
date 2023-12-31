// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Protocol.pb.h"
#include "S1Character.generated.h"

UCLASS()
class S1_API AS1Character : public ACharacter
{
	GENERATED_BODY()

public:
	AS1Character();
	virtual ~AS1Character();

protected:
	virtual void BeginPlay();
	virtual void Tick(float DeltaTime) override;

public:
	bool IsMyPlayer();

	Protocol::MoveState GetMoveState() { return PlayerInfo->state(); }
	void SetMoveState(Protocol::MoveState State);

public:
	void SetPlayerPos(const Protocol::SaveInfo& Info);
	void SetPlayerInfo(const Protocol::PlayerInfo& Info);
	void SetDestInfo(const Protocol::PlayerInfo& Info);
	Protocol::PlayerInfo* GetPlayerInfo() { return PlayerInfo; }

private:
	void TickMove(float DeltaTime);
	void TickRotate(float DeltaTime);
	void UpdateLocation();
	void AdjustRemoteLocation(float DeltaTime);

protected:
	class Protocol::PlayerInfo* PlayerInfo; // 현재 위치
	class Protocol::PlayerInfo* DestInfo; // 목적지

protected:
	UPROPERTY(BlueprintReadOnly)
	float GroundSpeed = 0.0f;
};

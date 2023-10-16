// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/S1Character.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "S1MyPlayer.h"
#include "Kismet/KismetMathLibrary.h"

AS1Character::AS1Character()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 53.0f);

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 400.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	GetCharacterMovement()->bRunPhysicsWithNoController = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;

	PlayerInfo = new Protocol::PlayerInfo();
	DestInfo = new Protocol::PlayerInfo();
}

AS1Character::~AS1Character()
{
	delete PlayerInfo;
	delete DestInfo;
	PlayerInfo = nullptr;
	DestInfo = nullptr;
}

void AS1Character::BeginPlay()
{
	Super::BeginPlay();

	{
		FVector Location = GetActorLocation();
		DestInfo->set_x(Location.X);
		DestInfo->set_y(Location.Y);
		DestInfo->set_z(Location.Z);
		DestInfo->set_yaw(-90);

		SetMoveState(Protocol::MOVE_STATE_IDLE);
	}
}

void AS1Character::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateLocation();

	if (IsMyPlayer() == false)
	{
		TickMove(DeltaTime);
		TickRotate(DeltaTime);
	}
}

void AS1Character::UpdateLocation()
{
	FVector Location = GetActorLocation();
	FRotator Rotator = GetActorRotation();
	PlayerInfo->set_x(Location.X);
	PlayerInfo->set_y(Location.Y);
	PlayerInfo->set_z(Location.Z);
	PlayerInfo->set_yaw(Rotator.Yaw);
}

bool AS1Character::IsMyPlayer()
{
	return Cast<AS1MyPlayer>(this) != nullptr;
}

void AS1Character::SetMoveState(Protocol::MoveState State)
{
	if (PlayerInfo->state() == State)
		return;

	PlayerInfo->set_state(State);
}

void AS1Character::SetPlayerPos(const Protocol::SaveInfo& Info)
{
	SetActorLocation(FVector(Info.x(), Info.y(), Info.z()));
	UpdateLocation();
}

void AS1Character::SetPlayerInfo(const Protocol::PlayerInfo& Info)
{
	if (PlayerInfo->object_id() != 0)
	{
		assert(PlayerInfo->object_id() == Info.object_id());
	}

	PlayerInfo->CopyFrom(Info);

	FVector Location(Info.x(), Info.y(), Info.z());
	SetActorLocation(Location);

	FRotator Rotator(0, Info.yaw(), 0);
	SetActorRotation(Rotator);
}

void AS1Character::SetDestInfo(const Protocol::PlayerInfo& Info)
{
	if (PlayerInfo->object_id() != 0)
	{
		assert(PlayerInfo->object_id() == Info.object_id());
	}
	
	// Dest에 최종 상태 복사
	DestInfo->CopyFrom(Info);

	// 상태 처리
	if (Info.state() == Protocol::MOVE_STATE_JUMP)
		Super::Jump();
}

void AS1Character::TickMove(float DeltaTime)
{
	FVector Location = GetActorLocation();
	float Height = Location.Z;
	FVector2D Location2D = FVector2D(Location.X, Location.Y);
	FVector2D DestLocation2D = FVector2D(DestInfo->x(), DestInfo->y());
	FVector2D MoveDir2D = (DestLocation2D - Location2D);

	const float DistToDest = MoveDir2D.Length();
	MoveDir2D.Normalize();

	GroundSpeed = DestInfo->ground_speed();
	float MoveDist = GroundSpeed * DeltaTime;

	MoveDist = FMath::Min(MoveDist, DistToDest);

	const FVector2D NextLocation = Location2D + MoveDir2D * MoveDist;
	SetActorLocation(FVector(NextLocation.X, NextLocation.Y, Height));
}

void AS1Character::TickRotate(float DeltaTime)
{
	const FRotator DestRotator = FRotator(0, DestInfo->yaw(), 0);
	const FQuat NewRotation = FQuat::Slerp(GetActorRotation().Quaternion(), DestRotator.Quaternion(), 0.1f);
	SetActorRotation(FRotator(NewRotation));
}

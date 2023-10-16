// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/S1MyPlayer.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "S1.h"
#include "Kismet/KismetMathLibrary.h"

AS1MyPlayer::AS1MyPlayer()
{
	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)

	GetCharacterMovement()->bOrientRotationToMovement = true;
}

void AS1MyPlayer::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

// Input

void AS1MyPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {

		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AS1MyPlayer::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AS1MyPlayer::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &AS1MyPlayer::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AS1MyPlayer::Look);
	}
}

void AS1MyPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	CheckFall();
	TickMovePacket(DeltaTime);
	TickJumpPacket(DeltaTime);
}

void AS1MyPlayer::TickMovePacket(float DeltaTime)
{
	// Velocity
	FVector Velocity = GetCharacterMovement()->Velocity;
	Velocity.Z = 0;
	GroundSpeed = Velocity.Length();

	// Send 판정
	bool ForceSendPacket = false;

	if (LastDesiredInput != DesiredInput)
	{
		ForceSendPacket = true;
		LastDesiredInput = DesiredInput;
	}

	// State 정보
	if (DesiredInput == FVector2D::Zero())
		SetMoveState(Protocol::MOVE_STATE_IDLE);
	else
		SetMoveState(Protocol::MOVE_STATE_RUN);

	MovePacketSendTimer -= DeltaTime;

	if (MovePacketSendTimer <= 0 || ForceSendPacket)
	{
		MovePacketSendTimer = MOVE_PACKET_SEND_DELAY;

		Protocol::C_MOVE MovePkt;

		// 현재 위치 정보
		{
			Protocol::PlayerInfo* info = MovePkt.mutable_info();
			info->CopyFrom(*PlayerInfo);
			info->set_state(GetMoveState());
			info->set_ground_speed((GroundSpeed + LastGroundSpeed) / 2.0f);
		}

		SEND_PACKET(MovePkt);

		LastGroundSpeed = GroundSpeed;
	}
}

void AS1MyPlayer::TickJumpPacket(float DeltaTime)
{
	if (!bJump)
		return;

	JumpPacketSendTimer -= DeltaTime;

	if (JumpPacketSendTimer <= 0)
	{
		bJump = false;

		Protocol::C_MOVE JumpPkt;

		{
			Protocol::PlayerInfo* info = JumpPkt.mutable_info();
			info->CopyFrom(*PlayerInfo);
			info->set_state(Protocol::MOVE_STATE_JUMP);
		}

		SEND_PACKET(JumpPkt);
	}
}

void AS1MyPlayer::CheckFall()
{
	FVector Location = GetActorLocation();

	if (Location.Z < -700)
	{
		Protocol::C_SAVE SavePkt;
		SEND_PACKET(SavePkt);
	}
}

void AS1MyPlayer::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();
	
	if (Controller != nullptr)
	{
		// Move
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		FVector MoveDirection = FVector::ZeroVector;
		MoveDirection += ForwardDirection * MovementVector.Y;
		MoveDirection += RightDirection * MovementVector.X;
		MoveDirection.Normalize();
		AddMovementInput(MoveDirection, 1.0f);

		// Cache
		{
			DesiredInput = MovementVector;
			
			DesiredMoveDirection = MoveDirection;

			const FVector Location = GetActorLocation();
			const FRotator Rotator = UKismetMathLibrary::FindLookAtRotation(Location, Location + MoveDirection);
			DesiredYaw = Rotator.Yaw;
		}
	}
}

void AS1MyPlayer::Jump(const FInputActionValue& Value)
{
	Super::Jump();
	bJump = true;
	JumpPacketSendTimer = MOVE_PACKET_SEND_DELAY;
}

void AS1MyPlayer::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

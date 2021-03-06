// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlurParCharacter.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "MyCharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include<GameplayEffectTypes.h>
#include"MyAbilitySystemComponent.h"
#include"MyAttributeSet.h"
#include"BaseGameplayAbility.h"
#include "Net/UnrealNetwork.h"
#include "Components/BoxComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include"RacePlayerController.h"

//////////////////////////////////////////////////////////////////////////
// ABlurParCharacter

ABlurParCharacter::ABlurParCharacter(const class FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer.SetDefaultSubobjectClass<UMyCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetMesh(),FName("headSocket"));
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm


	//GAS Components
	AbilitySystemComp = CreateDefaultSubobject<UMyAbilitySystemComponent>(TEXT("AbilitySystemComp"));
	AbilitySystemComp->SetIsReplicated(true);
	AbilitySystemComp->SetReplicationMode(EGameplayEffectReplicationMode::Full);

	AttributeSet = CreateDefaultSubobject<UMyAttributeSet>(TEXT("AttributeSet"));


	//AbilitySystemComp->RegisterGameplayTagEvent(FGameplayTag::RequestGameplayTag(FName("Player.Debuff.Stun")), EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ABlurParCharacter::StunTagChanged);


	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)
}

UAbilitySystemComponent * ABlurParCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComp;
}

void ABlurParCharacter::InitializeAttributes()
{
	if (AbilitySystemComp && DefaultGameplayEffect)
	{
		FGameplayEffectContextHandle EffectContext = AbilitySystemComp->MakeEffectContext();
		EffectContext.AddSourceObject(this);


		FGameplayEffectSpecHandle SpecHandle = AbilitySystemComp->MakeOutgoingSpec(DefaultGameplayEffect, 1, EffectContext);

		if (SpecHandle.IsValid())
		{
			FActiveGameplayEffectHandle GEHandle = AbilitySystemComp->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}
}

void ABlurParCharacter::GiveAbilities()
{
	if (HasAuthority() && AbilitySystemComp)
	{
		for (TSubclassOf<UBaseGameplayAbility>& StartupAbility : DefaultAbilities)
		{

			AbilitySystemComp->GiveAbility(FGameplayAbilitySpec(StartupAbility, 1, static_cast<int32>(StartupAbility.GetDefaultObject()->AbilityInputID), this));


		}
	}
}

void ABlurParCharacter::PossessedBy(AController * NewController)
{
	Super::PossessedBy(NewController);

	//server GAS 
	AbilitySystemComp->InitAbilityActorInfo(this, this);

	InitializeAttributes();
	GiveAbilities();
}

void ABlurParCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	//Client GAS
	AbilitySystemComp->InitAbilityActorInfo(this, this);

	InitializeAttributes();

	if (AbilitySystemComp && InputComponent)
	{
		const FGameplayAbilityInputBinds Binds("Confirm", "Cancel", "EGASAbilityInputID", static_cast<int32>(EGASAbilityInputID::Confirm), static_cast<int32>(EGASAbilityInputID::Cancel));

		AbilitySystemComp->BindAbilityActivationToInputComponent(InputComponent, Binds);
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void ABlurParCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ABlurParCharacter::DoubleJump);
	//PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAxis("MoveForward", this, &ABlurParCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ABlurParCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &ABlurParCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &ABlurParCharacter::LookUpAtRate);


	
	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &ABlurParCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &ABlurParCharacter::TouchStopped);

	// VR headset functionality
	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &ABlurParCharacter::OnResetVR);
	
	PlayerInputComponent->BindAction("Sprint", IE_Pressed, this, &ABlurParCharacter::SprintStart);
	PlayerInputComponent->BindAction("Sprint", IE_Released, this, &ABlurParCharacter::SprintEnd);
	PlayerInputComponent->BindAction("Dash", IE_Pressed, this, &ABlurParCharacter::Dash);


	if (AbilitySystemComp && InputComponent)
	{
		const FGameplayAbilityInputBinds Binds("Confirm", "Cancel", "EGASAbilityInputID", static_cast<int32>(EGASAbilityInputID::Confirm), static_cast<int32>(EGASAbilityInputID::Cancel));

		AbilitySystemComp->BindAbilityActivationToInputComponent(InputComponent, Binds);
	}
	
}

ARacePlayerController * ABlurParCharacter::GetPlayerController()
{
	ARacePlayerController* PC = Cast<ARacePlayerController>(GetController());
	if (PC)
	{
		return PC;
	}
	return nullptr;
}


void ABlurParCharacter::BeginPlay()
{
	Super::BeginPlay();
	DisableInput(UGameplayStatics::GetPlayerController(GetWorld(), 0));
}

void ABlurParCharacter::OnResetVR()
{
	// If BlurPar is added to a project via 'Add Feature' in the Unreal Editor the dependency on HeadMountedDisplay in BlurPar.Build.cs is not automatically propagated
	// and a linker error will result.
	// You will need to either:
	//		Add "HeadMountedDisplay" to [YourProject].Build.cs PublicDependencyModuleNames in order to build successfully (appropriate if supporting VR).
	// or:
	//		Comment or delete the call to ResetOrientationAndPosition below (appropriate if not supporting VR)
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void ABlurParCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
		Jump();
		
}

void ABlurParCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
		StopJumping();
}

void ABlurParCharacter::SprintStart()
{
	if (bIsProjectActivated)
	{
		GetMyMovementComponent()->SetSprinting(true);
	}
	else
	{
		GetMyMovementComponent()->MaxWalkSpeed = 1200;
	}
}

void ABlurParCharacter::SprintEnd()
{
	if (bIsProjectActivated)
	{
		GetMyMovementComponent()->SetSprinting(false);
	}
	else
	{
		GetMyMovementComponent()->MaxWalkSpeed = 300;
	}
}

void ABlurParCharacter::Dash()
{
	GetMyMovementComponent()->DoDodge();
}

void ABlurParCharacter::DoubleJump()
{
	GetMyMovementComponent()->DoDoubleJump();
}



UMyCharacterMovementComponent * ABlurParCharacter::GetMyMovementComponent() const
{
	return static_cast<UMyCharacterMovementComponent*>(GetCharacterMovement());
}

void ABlurParCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void ABlurParCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void ABlurParCharacter::MoveForward(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void ABlurParCharacter::MoveRight(float Value)
{
	if ( (Controller != nullptr) && (Value != 0.0f) )
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
	
		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}

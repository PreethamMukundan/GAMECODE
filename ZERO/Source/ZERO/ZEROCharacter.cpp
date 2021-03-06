// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZEROCharacter.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include<GameplayEffectTypes.h>
#include"BaseChar_AbilitySystemComponent.h"
#include"BaseChar_AttributeSet.h"
#include"BaseChar_BaseGameplayAbility.h"
#include "Net/UnrealNetwork.h"
#include "Components/BoxComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/Widget.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include"FirstGameMode.h"
#include"FirstPlayerState.h"
#include"FirstPlayerController.h"
//////////////////////////////////////////////////////////////////////////
// AZEROCharacter

AZEROCharacter::AZEROCharacter()
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
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	AttackCollisionComp = CreateDefaultSubobject<UBoxComponent>(TEXT("AttackCollisonComp"));
	FName AttackSock = "Attack_Socket";
	/*AttackCollisionComp->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttackSock);*/
	AttackCollisionComp->SetupAttachment(GetMesh(), AttackSock);
	//AttackCollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AttackCollisionComp->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);
	
	//AttackCollisionComp->OnComponentHit.AddDynamic(this, &AZEROCharacter::OnHitAttack);
	//AttackCollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AZEROCharacter::AttackOnOverlapBegin);

	//GAS Components
	AbilitySystemComp = CreateDefaultSubobject<UBaseChar_AbilitySystemComponent>(TEXT("AbilitySystemComp"));
	AbilitySystemComp->SetIsReplicated(true);
	AbilitySystemComp->SetReplicationMode(EGameplayEffectReplicationMode::Full);

	AttributeSet = CreateDefaultSubobject<UBaseChar_AttributeSet>(TEXT("AttributeSet"));


	AbilitySystemComp->RegisterGameplayTagEvent(FGameplayTag::RequestGameplayTag(FName("Player.Debuff.Stun")), EGameplayTagEventType::NewOrRemoved).AddUObject(this, &AZEROCharacter::StunTagChanged);


	bUseControllerRotationYaw = true;
	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)


	ComboCount = 1;
	MaxComboCount = 3;
	TeamID = ETeamID::None;
}

UAbilitySystemComponent * AZEROCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComp;
}

void AZEROCharacter::InitializeAttributes()
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

void AZEROCharacter::GiveAbilities()
{
	if (HasAuthority() && AbilitySystemComp)
	{
		for (TSubclassOf<UBaseChar_BaseGameplayAbility>& StartupAbility : DefaultAbilities)
		{

			AbilitySystemComp->GiveAbility(FGameplayAbilitySpec(StartupAbility, 1, static_cast<int32>(StartupAbility.GetDefaultObject()->AbilityInputID), this));


		}
	}
}

void AZEROCharacter::PossessedBy(AController * NewController)
{
	Super::PossessedBy(NewController);

	//server GAS 
	AbilitySystemComp->InitAbilityActorInfo(this, this);

	InitializeAttributes();
	GiveAbilities();
}

void AZEROCharacter::OnRep_PlayerState()
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



void AZEROCharacter::HandleDeath()
{
	GetCharacterMovement()->MaxWalkSpeed = 0;
	if (MyHUD)
	{
		
			MyHUD->RemoveFromParent();
		
	}
	
	AFirstPlayerController* MyPC = Cast<AFirstPlayerController>(GetController());
	if (MyPC)
	{
		MyPC->RespawnStart();
	}
	AbilitySystemComp->CancelAllAbilities();

	Destroy();  
	
	
}

void AZEROCharacter::HandleGameOver()
{
	GetCharacterMovement()->MaxWalkSpeed = 0;
	if (MyHUD)
	{

		MyHUD->RemoveFromParent();

	}
	AbilitySystemComp->CancelAllAbilities();

	Destroy();

}

void AZEROCharacter::HandleKill()
{
	AFirstGameMode* MyGameMode = Cast<AFirstGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (MyGameMode)
	{
		MyGameMode->AddAndCheckIfKillCountReachedLimit(TeamID);
		UpdateKillCount(MyGameMode->TeamAKills, MyGameMode->TeamBKills);

	}
	AFirstPlayerState* MyPlayerState = Cast<AFirstPlayerState>(GetController()->PlayerState);
	if (MyPlayerState)
	{
		MyPlayerState->Kills += 1;

		
		FString TheFloatStr = FString::SanitizeFloat(MyPlayerState->Kills);
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Blue, *TheFloatStr);
	}
}

float AZEROCharacter::GetKillCount()
{
	AFirstGameMode* MyGameMode = Cast<AFirstGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (MyGameMode)
	{
		//MyGameMode->AddAndCheckIfKillCountReachedLimit(TeamID);
		UpdateKillCount(MyGameMode->TeamAKills, MyGameMode->TeamBKills);

	}
	AFirstPlayerState* MyPlayerState = Cast<AFirstPlayerState>(GetController()->PlayerState);
	if (MyPlayerState)
	{
		return MyPlayerState->Kills;
	}
	return 0.0;
}

void AZEROCharacter::UpdateKillCount(int AK, int BK)
{
	ATeamKillCount = AK;
	BTeamKillCount = BK;
}



//////////////////////////////////////////////////////////////////////////
// Input

void AZEROCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAxis("MoveForward", this, &AZEROCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AZEROCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AZEROCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AZEROCharacter::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &AZEROCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &AZEROCharacter::TouchStopped);

	// VR headset functionality
	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &AZEROCharacter::OnResetVR);

	if (AbilitySystemComp && InputComponent)
	{
		const FGameplayAbilityInputBinds Binds("Confirm", "Cancel", "EGASAbilityInputID", static_cast<int32>(EGASAbilityInputID::Confirm), static_cast<int32>(EGASAbilityInputID::Cancel));

		AbilitySystemComp->BindAbilityActivationToInputComponent(InputComponent, Binds);
	}
}

void AZEROCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AZEROCharacter, TeamID);
	DOREPLIFETIME(AZEROCharacter, ComboCount);

}


void AZEROCharacter::StunTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Cyan, "StunChange");
	if (NewCount > 0)
	{
		GetCharacterMovement()->MaxWalkSpeed = 0;
	}
	else
	{
		GetCharacterMovement()->MaxWalkSpeed = 600;
	}
}

void AZEROCharacter::IncrementComboCount()
{
	ComboCount++;
	if (ComboCount == MaxComboCount)
	{
		ComboCount = 1;
	}
}

void AZEROCharacter::SetTeamBeforeSpawn(ETeamID TID)
{
	TeamID = TID;
}

void AZEROCharacter::OnHitAttack(UPrimitiveComponent * HitComponent, AActor * OtherActor, UPrimitiveComponent * OtherComponent, FVector NormalImpulse, const FHitResult & Hit)
{
	GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, "REP.Hit");
	if (OtherActor != this && OtherActor != GetOwner())
	{

		//GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, "REP.Hit");
		
		AZEROCharacter* Villan = Cast<AZEROCharacter>(OtherActor);
		if (Villan)
		{
			//GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, "REP.Hit");
			FGameplayEventData Pay;
			Pay.Instigator = this;
			Pay.Target = Villan;
			FGameplayAbilityTargetDataHandle TDataHandle = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromActor(Villan);
			//FGameplayAbilityTargetData TData;
			Pay.TargetData = TDataHandle;

			FGameplayTag TagX = FGameplayTag::RequestGameplayTag(FName("Weapon.Projectile.Hit"));
			//GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, TagX.ToString());
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, TagX, Pay);

			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, "Hit");

		}
		else
		{
			
			//GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Yellow, "HitSomthingElse");
		}
	}
}

void AZEROCharacter::AttackOnOverlapBegin(UPrimitiveComponent * OverlappedComponent, AActor * OtherActor, UPrimitiveComponent * OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult)
{
	//GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, "REP.Hit");
	if (OtherActor != this && OtherActor != GetOwner())
	{

		//GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, "REP.Hit");

		AZEROCharacter* Villan = Cast<AZEROCharacter>(OtherActor);
		if (Villan)
		{
			//GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, "REP.Hit");
			FGameplayEventData Pay;
			Pay.Instigator = this;
			Pay.Target = Villan;
			FGameplayAbilityTargetDataHandle TDataHandle = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromActor(Villan);
			//FGameplayAbilityTargetData TData;
			Pay.TargetData = TDataHandle;

			FGameplayTag TagX = FGameplayTag::RequestGameplayTag(FName("Weapon.Projectile.Hit"));
			//GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, TagX.ToString());
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, TagX, Pay);

			//GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, "Hit");

		}
		else
		{

			//GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Yellow, "HitSomthingElse");
		}
	}
}

void AZEROCharacter::HandleCastEvent()
{
	FGameplayEventData Pay;
	Pay.Instigator = this;
	

	FGameplayTag TagX = FGameplayTag::RequestGameplayTag(FName());
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, TagX, Pay);
}

float AZEROCharacter::GetCurrentHealthAttributeFloat()
{
	return AttributeSet->GetCurrentHealth();
}

float AZEROCharacter::GetMaxHealthAttributeFloat()
{
	
	return AttributeSet->GetMaximumHealth();
}

ETeamID AZEROCharacter::GetTeamID()
{
	return TeamID;
}



void AZEROCharacter::BeginPlay()
{
	Super::BeginPlay();
	//AttackCollisionComp->OnComponentHit.AddDynamic(this, &AZEROCharacter::OnHitAttack);
	AttackCollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AZEROCharacter::AttackOnOverlapBegin);

	if (HUDWidget)
	{
		if (HasAuthority())
		{
			MyHUD = CreateWidget<UUserWidget>(UGameplayStatics::GetPlayerController(GetWorld(), 0), HUDWidget);
			if (MyHUD)
			{
				MyHUD->AddToViewport();
			}
		}
		if (IsLocallyControlled())
		{
			
			MyHUD = CreateWidget<UUserWidget>(UGameplayStatics::GetPlayerController(GetWorld(), 0), HUDWidget);
			if (MyHUD)
			{
				MyHUD->AddToViewport();
			}
		}
	}
}

void AZEROCharacter::OnResetVR()
{
	// If ZERO is added to a project via 'Add Feature' in the Unreal Editor the dependency on HeadMountedDisplay in ZERO.Build.cs is not automatically propagated
	// and a linker error will result.
	// You will need to either:
	//		Add "HeadMountedDisplay" to [YourProject].Build.cs PublicDependencyModuleNames in order to build successfully (appropriate if supporting VR).
	// or:
	//		Comment or delete the call to ResetOrientationAndPosition below (appropriate if not supporting VR)
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void AZEROCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
		Jump();
}

void AZEROCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
		StopJumping();
}

void AZEROCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AZEROCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AZEROCharacter::MoveForward(float Value)
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

void AZEROCharacter::MoveRight(float Value)
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

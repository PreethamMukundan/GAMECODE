// Fill out your copyright notice in the Description page of Project Settings.


#include "ANS_MeleeAttackAnimNotifyState.h"
#include"ZEROCharacter.h"
#include "Components/BoxComponent.h"

void UANS_MeleeAttackAnimNotifyState::NotifyBegin(USkeletalMeshComponent * MeshComp, UAnimSequenceBase * Animation, float TotalDuration)
{
	AZEROCharacter* MyPlayer = Cast<AZEROCharacter>(MeshComp->GetOwner());
	if (MyPlayer)
	{
		MyPlayer->AttackCollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		MyPlayer->AttackCollisionComp->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
		GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Blue, "NotifyStart");
	}
}
void UANS_MeleeAttackAnimNotifyState::NotifyEnd(USkeletalMeshComponent * MeshComp, UAnimSequenceBase * Animation)
{
	AZEROCharacter* MyPlayer = Cast<AZEROCharacter>(MeshComp->GetOwner());
	if (MyPlayer)
	{
		MyPlayer->AttackCollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MyPlayer->AttackCollisionComp->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
		GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, "NotifyEnd");
	}
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "BaseChar_AttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include"BaseChar_AbilitySystemComponent.h"
#include"ZEROCharacter.h"



UBaseChar_AttributeSet::UBaseChar_AttributeSet()
{

}
void UBaseChar_AttributeSet::PreAttributeChange(const FGameplayAttribute & Attribute, float & NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetMaxHealthAttribute())
	{
		AdjustAttributeForMaxChange(Health, MaxHealth, NewValue, GetHealthAttribute());
	}
}

void UBaseChar_AttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData & Data)
{
	Super::PostGameplayEffectExecute(Data);

	FGameplayEffectContextHandle Context = Data.EffectSpec.GetContext();
	UAbilitySystemComponent* Source = Context.GetOriginalInstigatorAbilitySystemComponent();
	const FGameplayTagContainer& SourceTags = *Data.EffectSpec.CapturedSourceTags.GetAggregatedTags();

	// Compute the delta between old and new, if it is available
	float DeltaValue = 0;
	if (Data.EvaluatedData.ModifierOp == EGameplayModOp::Type::Additive)
	{
		// If this was additive, store the raw delta value to be passed along later
		DeltaValue = Data.EvaluatedData.Magnitude;
	}

	// Get the Target actor, which should be our owner
	AActor* TargetActor = nullptr;
	AController* TargetController = nullptr;
	AZEROCharacter* TargetCharacter = nullptr;
	if (Data.Target.AbilityActorInfo.IsValid() && Data.Target.AbilityActorInfo->AvatarActor.IsValid())
	{
		TargetActor = Data.Target.AbilityActorInfo->AvatarActor.Get();
		TargetController = Data.Target.AbilityActorInfo->PlayerController.Get();
		TargetCharacter = Cast<AZEROCharacter>(TargetActor);
	}

	/*	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
		{
			// Get the Source actor
			AActor* SourceActor = nullptr;
			AController* SourceController = nullptr;
			ARPGCharacterBase* SourceCharacter = nullptr;
			if (Source && Source->AbilityActorInfo.IsValid() && Source->AbilityActorInfo->AvatarActor.IsValid())
			{
				SourceActor = Source->AbilityActorInfo->AvatarActor.Get();
				SourceController = Source->AbilityActorInfo->PlayerController.Get();
				if (SourceController == nullptr && SourceActor != nullptr)
				{
					if (APawn* Pawn = Cast<APawn>(SourceActor))
					{
						SourceController = Pawn->GetController();
					}
				}

				// Use the controller to find the source pawn
				if (SourceController)
				{
					SourceCharacter = Cast<ARPGCharacterBase>(SourceController->GetPawn());
				}
				else
				{
					SourceCharacter = Cast<ARPGCharacterBase>(SourceActor);
				}

				// Set the causer actor based on context if it's set
				if (Context.GetEffectCauser())
				{
					SourceActor = Context.GetEffectCauser();
				}
			}

			// Try to extract a hit result
			FHitResult HitResult;
			if (Context.GetHitResult())
			{
				HitResult = *Context.GetHitResult();
			}

			// Store a local copy of the amount of damage done and clear the damage attribute
			const float LocalDamageDone = GetDamage();
			SetDamage(0.f);

			if (LocalDamageDone > 0)
			{
				// Apply the health change and then clamp it
				const float OldHealth = GetHealth();
				SetHealth(FMath::Clamp(OldHealth - LocalDamageDone, 0.0f, GetMaxHealth()));

				if (TargetCharacter)
				{
					// This is proper damage
					TargetCharacter->HandleDamage(LocalDamageDone, HitResult, SourceTags, SourceCharacter, SourceActor);

					// Call for all health changes
					TargetCharacter->HandleHealthChanged(-LocalDamageDone, SourceTags);
				}
			}
		}*/
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		// Handle other health changes such as from healing or direct modifiers
		// First clamp it
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));

		if (TargetCharacter)
		{
			// Call for all health changes
			//TargetCharacter->HandleHealthChanged(DeltaValue, SourceTags);
		}
	}
}

void UBaseChar_AttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UBaseChar_AttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseChar_AttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseChar_AttributeSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseChar_AttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
}

void UBaseChar_AttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseChar_AttributeSet, Health, OldHealth);
}

void UBaseChar_AttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseChar_AttributeSet, MaxHealth, OldMaxHealth);
}

void UBaseChar_AttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldStamina)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseChar_AttributeSet, Stamina, OldStamina);
}

void UBaseChar_AttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseChar_AttributeSet, AttackPower, OldAttackPower);
}

void UBaseChar_AttributeSet::AdjustAttributeForMaxChange(FGameplayAttributeData & AffectedAttribute, const FGameplayAttributeData & MaxAttribute, float NewMaxValue, const FGameplayAttribute & AffectedAttributeProperty)
{
	UAbilitySystemComponent* AbilityComp = GetOwningAbilitySystemComponent();
	const float CurrentMaxValue = MaxAttribute.GetCurrentValue();
	if (!FMath::IsNearlyEqual(CurrentMaxValue, NewMaxValue) && AbilityComp)
	{
		// Change current value to maintain the current Val / Max percent
		const float CurrentValue = AffectedAttribute.GetCurrentValue();
		float NewDelta = (CurrentMaxValue > 0.f) ? (CurrentValue * NewMaxValue / CurrentMaxValue) - CurrentValue : NewMaxValue;

		AbilityComp->ApplyModToAttributeUnsafe(AffectedAttributeProperty, EGameplayModOp::Additive, NewDelta);
	}
}
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AssemblyModel.generated.h"

/**
 * The standard structure of a data "model" (MVC pattern) for reading by MovingXXXX actors, storing by UI controller/EPICS data feed
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStateChanged);

UCLASS(Abstract)
class GEMINISTARPLATINUM_API UAssemblyModel : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable) FOnStateChanged OnStateChanged;
protected:
	/// <summary>
	/// Returns and broadcasts a value clamped to given range
	/// </summary>
	/// <param name="Value">A float</param>
	/// <param name="Field">A pointer to a variable</param>
	/// <param name="Min">The minimum, float</param>
	/// <param name="Max">The maximum, float</param>
	/// <returns>The value when clamped to the range defined by min and max.</returns>
	float ClampAndStore(float Value, float& Field, float Min, float Max); 
};

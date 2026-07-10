// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnvironmentView.generated.h"

/**
 * View (MVC pattern) for UEnvironmentModel. Subscribes to OnStateChanged and forwards the model's
 * CurrentTime to Blueprint via OnTimeChanged. Intended to be subclassed in Blueprint (BP_EnvironmentView),
 * which holds a reference to the Celestial Vault sky actor and applies the time to it — Celestial Vault
 * is a content-only Blueprint plugin, so that last hop cannot be done in C++.
 */
UCLASS(Abstract, Blueprintable)
class GEMINISTARPLATINUM_API AEnvironmentView : public AActor
{
	GENERATED_BODY()

public:
	AEnvironmentView();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/// <summary>
	/// Bound to UEnvironmentModel::OnStateChanged. Reads the model's CurrentTime and raises OnTimeChanged.
	/// </summary>
	UFUNCTION() void HandleStateChanged();

	/// <summary>
	/// Implemented in the Blueprint child; applies NewTime to the Celestial Vault actor.
	/// </summary>
	/// <param name="NewTime">Validated site-local datetime from UEnvironmentModel::CurrentTime.</param>
	UFUNCTION(BlueprintImplementableEvent) void OnTimeChanged(FDateTime NewTime, bool TimeProgresses);
};

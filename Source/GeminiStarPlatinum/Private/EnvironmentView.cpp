// Fill out your copyright notice in the Description page of Project Settings.


#include "EnvironmentView.h"
#include "EnvironmentModel.h"

AEnvironmentView::AEnvironmentView()
{
	// Purely event-driven; no per-frame work
	PrimaryActorTick.bCanEverTick = false;
}

void AEnvironmentView::BeginPlay()
{
	Super::BeginPlay();
	if (UEnvironmentModel* Model = GetGameInstance()->GetSubsystem<UEnvironmentModel>())
	{
		Model->OnStateChanged.AddDynamic(this, &AEnvironmentView::HandleStateChanged);
		// Sync the sky to the model's current value at startup rather than waiting for the first edit
		HandleStateChanged();
	}
}

void AEnvironmentView::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The subsystem outlives level travel; remove the binding so it doesn't go stale
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UEnvironmentModel* Model = GameInstance->GetSubsystem<UEnvironmentModel>())
		{
			Model->OnStateChanged.RemoveDynamic(this, &AEnvironmentView::HandleStateChanged);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AEnvironmentView::HandleStateChanged()
{
	if (const UEnvironmentModel* Model = GetGameInstance()->GetSubsystem<UEnvironmentModel>())
	{
		OnTimeChanged(Model->CurrentTime);
	}
}

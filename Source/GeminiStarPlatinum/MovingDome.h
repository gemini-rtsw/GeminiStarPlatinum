#pragma once

#include "CoreMinimal.h"
#include "MovingThing.h"
#include "MovingDome.generated.h"

UCLASS()
class GEMINISTARPLATINUM_API AMovingDome : public AMovingThing
{
	GENERATED_BODY()

public:
    // Sets default values for this actor's properties
    AMovingDome();
    // Called every frame
    virtual void Tick(float DeltaTime) override;

    // Opens shutters and vents
    UPROPERTY(BlueprintReadWrite)
    bool bOpen = false;

    // Dome target rotation (roll)
    UPROPERTY(BlueprintReadWrite)
    float DomeTwistTarget;
    // Spring strength of dome target orientation (kg*cm*s^-2 or cm*s^-2)
    UPROPERTY(BlueprintReadWrite)
    float DomeTwistStrength;
    // Dome rotational velocity target (revolutions per second)
    UPROPERTY(BlueprintReadWrite)
    float DomeVelocityTarget;
    // Dome rotational target damping (kg*cm*s^-1 pr cm*s^-1)
    UPROPERTY(BlueprintReadWrite)
    float DomeVelocityDamping;
    // Threshold to activate dome orientation target spring
    UPROPERTY(BlueprintReadWrite)
    float DomeAngularThreshold;

    // Top shutter target rotation (pitch)
    UPROPERTY(BlueprintReadWrite)
    float TopSSwingTarget;
    // Spring strength of top shutter target orientation (kg*cm*s^-2 or cm*s^-2)
    UPROPERTY(BlueprintReadWrite)
    float TopSSwingStrength;
    // Top shutter rotational velocity target (revolutions per second)
    UPROPERTY(BlueprintReadWrite)
    float TopSVelocityTarget;
    // Top shutter rotational target damping (kg*cm*s^-1 or cm*s^-1)
    UPROPERTY(BlueprintReadWrite)
    float TopSVelocityDamping;
    // Threshold to activate top shutter orientation target spring
    UPROPERTY(BlueprintReadWrite)
    float TopSAngularThreshold;
    // Top shutter orientation limit offset
    UPROPERTY(BluePrintReadWrite)
    float TopSAngularOffset;
    // Top shutter orientation limit
    UPROPERTY(BluePrintReadWrite)
    float TopSAngularLimit;

    // Bottom shutter target rotation (pitch)
    UPROPERTY(BlueprintReadWrite)
    float BotSSwingTarget;
    // Spring strength of bottom shutter target orientation (kg*cm*s^-2 or cm*s^-2)
    UPROPERTY(BlueprintReadWrite)
    float BotSSwingStrength;
    // Bottom shutter rotational velocity target (revolutions per second)
    UPROPERTY(BlueprintReadWrite)
    float BotSVelocityTarget;
    // Bottom shutter rotational target damping (kg*cm*s^-1 or cm*s^-1)
    UPROPERTY(BlueprintReadWrite)
    float BotSVelocityDamping;
    // Threshold to activate bottom shutter orientation target spring
    UPROPERTY(BlueprintReadWrite)
    float BotSAngularThreshold;
    // Bottom shutter orientation limit offset
    UPROPERTY(BluePrintReadWrite)
    float BotSAngularOffset;
    // Bottom shutter orientation limit
    UPROPERTY(BluePrintReadWrite)
    float BotSAngularLimit;

    // Vents target position
    UPROPERTY(BlueprintReadWrite)
    float VentSlideTarget;
    // Spring strength of vents target position (kg*cm*s^-2 or cm*s^-2)
    UPROPERTY(BlueprintReadWrite)
    float VentSlideStrength;
    // Vents linear velocity target
    UPROPERTY(BlueprintReadWrite)
    float VentVelocityTarget;
    // Vents linear target damping (kg*cm*s^-1 or cm*s^-1)
    UPROPERTY(BlueprintReadWrite)
    float VentVelocityDamping;
    // Threshold to activate vents position target spring
    UPROPERTY(BlueprintReadWrite)
    float VentLinearThreshold;

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    // Root component
    UPROPERTY(VisibleAnywhere)
    USceneComponent* RootSceneComponent;
    // Base component
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* Base;
    // Dome component
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* Dome;
    // Top shutter component
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* TopS;
    // Bottom shutter component
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* BotS;
    // Top vent component
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* TopV;
    // Bottom vent component
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* BotV;
    // Silly component that rotates forever, tricking the actor to never sleep, which allows for slow rotations
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* FluxCapacitor;

    // Connects base to dome
    UPROPERTY(VisibleAnywhere)
    UPhysicsConstraintComponent* DomeConstraint;
    // Connects dome to top shutter
    UPROPERTY(VisibleAnywhere)
    UPhysicsConstraintComponent* TopSConstraint;
    // Connects dome to bottom shutter
    UPROPERTY(VisibleAnywhere)
    UPhysicsConstraintComponent* BotSConstraint;
    // Connects dome to top vent
    UPROPERTY(VisibleAnywhere)
    UPhysicsConstraintComponent* TopVConstraint;
    // Connects dome to bottom vent
    UPROPERTY(VisibleAnywhere)
    UPhysicsConstraintComponent* BotVConstraint;
    // Connects dome to flux capacitor
    UPROPERTY(VisibleAnywhere)
    UPhysicsConstraintComponent* FluxCapacitorConstraint;

    // Determine constraint linear drive each frame
    void SlideComponents();
};

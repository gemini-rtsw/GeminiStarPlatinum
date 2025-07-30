#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "DrawDebugHelpers.h"
#include "GeminiDome.generated.h"

UCLASS()
class GEMINISTARPLATINUM_API AGeminiDome : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AGeminiDome();
	// Called every frame
	virtual void Tick(float DeltaTime) override;

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

    // Drives will ignore mass and use acceleration instead
    UPROPERTY(BlueprintReadWrite)
    bool bAccelerationMode = true;

    // Lock ground in place
    void SetBaseLocked(bool bLocked);

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

    // Determine constraint twist drive each frame
    void TwistComponent(
        UPhysicsConstraintComponent* Constraint,
        float TwistTarget,
        float TwistStrength,
        float VelocityTarget,
        float VelocityDamping,
        float AngularThreshold
    );
    // Determine constraint swing drive each frame
    void SwingComponent(
        UPhysicsConstraintComponent* Constraint,
        float SwingTarget,
        float SwingStrength,
        float VelocityTarget,
        float VelocityDamping,
        float AngularThreshold
    );
    // Determine constraint linear drive each frame
    void SlideComponents(
        UPhysicsConstraintComponent* Constraint,
        float SlideTarget,
        float SlideStrength,
        float VelocityTarget,
        float VelocityDamping,
        float AngularThreshold
    );

    // Create new component connected to parent 
    UStaticMeshComponent* CreateMeshComponent(
        FName Name,
        USceneComponent* Parent,
        UStaticMesh* MeshAsset,
        FVector Location,
        FRotator Rotation,
        float Mass,
        FVector COMOffset
    );
    // Create new constraint connecting parent and child connected to parent
    UPhysicsConstraintComponent* CreateConstraintComponent(
        FName Name,
        USceneComponent* Parent,
        USceneComponent* Child,
        FVector Location,
        FRotator Rotation,
        bool bCanTwist,
        bool bCanSwing,
        bool bCanSlide,
        float SwingAngle,
        float SwingLimit
    );
};

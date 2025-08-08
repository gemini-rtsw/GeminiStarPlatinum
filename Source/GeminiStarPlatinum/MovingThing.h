#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "MovingThing.generated.h"

UCLASS()
class GEMINISTARPLATINUM_API AMovingThing : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMovingThing();
	// Called every frame
	virtual void Tick(float DeltaTime) override;

    // Drives will ignore mass and use acceleration instead
    UPROPERTY(BlueprintReadWrite)
    bool bAccelerationMode = true;

    // Angular offset of the Main COM relative to the pivot
    float PivotAngularOffset = 0.f;

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
        float AngularThreshold,
        float SwingOffset
    );

    // Lock ground in place
    void SetBaseLocked(UStaticMeshComponent* Base, bool bLocked);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

    // Calculate center of mass offset needed to balance the telescope at a certain angle
    void CalculateCOMOffset(UPhysicsConstraintComponent* Constraint, UStaticMeshComponent* Component1, UStaticMeshComponent* Component2);
};

#include "GeminiDome.h"

// Sets default values
AGeminiDome::AGeminiDome()
{
    // Set this actor to call Tick() every frame
    PrimaryActorTick.bCanEverTick = true;

    DomeTwistTarget = 90.f;
    DomeTwistStrength = 5e9f;
    DomeVelocityTarget = 0.003f;
    DomeVelocityDamping = 2e10f;
    DomeAngularThreshold = 1.5f;

    TopSSwingTarget = 83.f;
    TopSSwingStrength = 5e10f;
    TopSVelocityTarget = 0.016f;
    TopSVelocityDamping = 2e10f;
    TopSAngularThreshold = 0.5f;
    TopSAngularOffset = 38.f;
    TopSAngularLimit = 45.f; // Target range between -7 and 83 based on model (shorter range compared to reality)

    BotSSwingTarget = -13.f;
    BotSSwingStrength = 5e10f;
    BotSVelocityTarget = 0.01f;
    BotSVelocityDamping = 2e10f;
    BotSAngularThreshold = 0.5f;
    BotSAngularOffset = -8.25f;
    BotSAngularLimit = 4.75f; // Target range between -13 and 3.5 based on reality

    VentSlideTarget = 120.f;
    VentSlideStrength = 2e10f;
    VentVelocityTarget = 0.003f;
    VentVelocityDamping = 2e11f;
    VentLinearThreshold = 1.5f;

    const ConstructorHelpers::FObjectFinder<UStaticMesh> BaseMesh(TEXT("/Game/DomeModels/dome_base_bldg_vsp/StaticMeshes/dome_base_bldg_vsp.dome_base_bldg_vsp"));
    const ConstructorHelpers::FObjectFinder<UStaticMesh> DomeMesh(TEXT("/Game/DomeModels/dome+vent_gates_vsp/StaticMeshes/dome+vent_gates_vsp.dome+vent_gates_vsp"));
    const ConstructorHelpers::FObjectFinder<UStaticMesh> TopSMesh(TEXT("/Game/DomeModels/dome_top_shutter_vsp/StaticMeshes/dome_top_shutter_vsp.dome_top_shutter_vsp"));
    const ConstructorHelpers::FObjectFinder<UStaticMesh> BotSMesh(TEXT("/Game/DomeModels/dome_bot_shutter_vsp/StaticMeshes/dome_bot_shutter_vsp.dome_bot_shutter_vsp"));
    const ConstructorHelpers::FObjectFinder<UStaticMesh> TopVMesh(TEXT("/Game/DomeModels/dome_top_vent_vsp/StaticMeshes/dome_top_vent_vsp.dome_top_vent_vsp"));
    const ConstructorHelpers::FObjectFinder<UStaticMesh> BotVMesh(TEXT("/Game/DomeModels/dome_bot_vent_vsp/StaticMeshes/dome_bot_vent_vsp.dome_bot_vent_vsp"));
    const ConstructorHelpers::FObjectFinder<UStaticMesh> FluxCapacitorMesh(TEXT("/Engine/EngineMeshes/ParticleCube.ParticleCube"));
    const FRotator ZeroRot(0.f, 0.f, 0.f);

    RootSceneComponent = CreateDefaultSubobject<USceneComponent>("RootSceneComponent");
    RootComponent = RootSceneComponent;

    Base = CreateMeshComponent(
        "Base",                    // Name
        RootSceneComponent,        // Parent attachment that ensures new component rotates relative to parent
        BaseMesh.Object,           // Reference to mesh
        FVector::ZeroVector,       // Location relative to parent
        FRotator(0.f, 0.f, -90.f), // Rotation relative to parent
        1e34f,                     // Mass override (kg)
        FVector::ZeroVector        // Center of mass offsetting UE-generated COM
    );
    Base->SetWorldScale3D(FVector(1.f, 1.f, 1.f)); // Sets scale of entire dome, and children inheret scale because of SetupAttachment()
    SetBaseLocked(true);

    Dome = CreateMeshComponent(
        "Dome",
        Base,
        DomeMesh.Object,
        FVector::ZeroVector,
        ZeroRot,
        40.f,
        FVector::ZeroVector
    );
    TopS = CreateMeshComponent(
        "Top Shutter",
        Dome,
        TopSMesh.Object,
        FVector::ZeroVector,
        ZeroRot,
        10.f,
        FVector(0.f, 1900.f, 0.f)
    );
    BotS = CreateMeshComponent(
        "Bottom Shutter",
        Dome,
        BotSMesh.Object,
        FVector::ZeroVector,
        ZeroRot,
        20.f,
        FVector(0.f, 1900.f, 0.f)
    );
    TopV = CreateMeshComponent(
        "Top Vent",
        Dome,
        TopVMesh.Object,
        FVector::ZeroVector,
        ZeroRot,
        20.f,
        FVector::ZeroVector
    );
    BotV = CreateMeshComponent(
        "Bottom Vent",
        Dome,
        BotVMesh.Object,
        FVector::ZeroVector,
        ZeroRot,
        10.f,
        FVector::ZeroVector
    );
    FluxCapacitor = CreateMeshComponent(
        "Flux Capacitor",
        Dome,
        FluxCapacitorMesh.Object,
        FVector::ZeroVector,
        ZeroRot,
        0.f,
        FVector::ZeroVector
    );

    DomeConstraint = CreateConstraintComponent(
        "Dome Constraint",         // Name
        Base,                      // Parent attachment
        Dome,                      // Child attachment
        FVector::ZeroVector,       // Location relative to parent
        FRotator(0.f, 90.f, 90.f), // Rotation relative to parent (pitch, yaw, roll)
        true,                      // Twists?
        false,                     // Swings?
        false,                     // Slides?
        0.f,                       // Swing Angle
        0.f                        // Swing limit
    );
    TopSConstraint = CreateConstraintComponent(
        "Top Shutter Constraint",
        Dome,
        TopS,
        FVector(0.f, 1900.f, 0.f),
        FRotator(0.f, 90.f, 90.f),
        false,
        true,
        false,
        TopSAngularOffset,
        TopSAngularLimit
    );
    BotSConstraint = CreateConstraintComponent(
        "Bottom Shutter Constraint",
        Dome,
        BotS,
        FVector(0.f, 1900.f, 0.f),
        FRotator(0.f, 90.f, 90.f),
        false,
        true,
        false,
        BotSAngularOffset,
        BotSAngularLimit
    );
    TopVConstraint = CreateConstraintComponent(
        "Top Vent Constraint",
        Dome,
        TopV,
        FVector::ZeroVector,
        FRotator(0.f, 90.f, 90.f),
        false,
        false,
        true,
        0.f,
        0.f
    );
    BotVConstraint = CreateConstraintComponent(
        "Bottom Vent Constraint",
        Dome,
        BotV,
        FVector::ZeroVector,
        FRotator(0.f, 90.f, 90.f),
        false,
        false,
        true,
        0.f,
        0.f
    );
    FluxCapacitorConstraint = CreateConstraintComponent(
        "Flux Capacitor Constraint",
        Dome,
        FluxCapacitor,
        FVector::ZeroVector,
        ZeroRot,
        true,
        false,
        false,
        0.f,
        0.f
    );
    FluxCapacitorConstraint->SetAngularVelocityTarget(FVector(1.f, 0.f, 0.f)); // Set to always spin so model doesn't sleep


}

UStaticMeshComponent* AGeminiDome::CreateMeshComponent(
    FName Name,
    USceneComponent* Parent,
    UStaticMesh* MeshAsset,
    FVector Location,
    FRotator Rotation,
    float Mass,
    FVector COMOffset
) {
    auto* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
    Mesh->SetupAttachment(Parent);
    Mesh->SetStaticMesh(MeshAsset);

    Mesh->SetRelativeLocation(Location);
    Mesh->SetRelativeRotation(Rotation);

    Mesh->SetSimulatePhysics(true);
    Mesh->SetMassOverrideInKg(NAME_None, Mass);
    Mesh->SetCenterOfMass(COMOffset);

    Mesh->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
    Mesh->SetCollisionObjectType(ECC_PhysicsBody);
    Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);

    Mesh->SetLinearDamping(0.f);
    Mesh->SetAngularDamping(0.f);
    Mesh->SetUseCCD(true);
    Mesh->SetGyroscopicTorqueEnabled(true);
    Mesh->BodyInstance.PositionSolverIterationCount = 50;
    Mesh->BodyInstance.VelocitySolverIterationCount = 30;
    Mesh->BodyInstance.CustomSleepThresholdMultiplier = 0;

    return Mesh;
}

UPhysicsConstraintComponent* AGeminiDome::CreateConstraintComponent(
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
) {
    auto* Constraint = CreateDefaultSubobject<UPhysicsConstraintComponent>(Name);
    Constraint->SetupAttachment(Parent);

    auto* Prim1 = Cast<UPrimitiveComponent>(Parent);
    auto* Prim2 = Cast<UPrimitiveComponent>(Child);
    if (Prim1 && Prim2)
        Constraint->SetConstrainedComponents(Prim2, NAME_None, Prim1, NAME_None);

    Constraint->SetRelativeLocation(Location);
    Constraint->SetRelativeRotation(Rotation);

    Constraint->SetAngularSwing1Limit(ACM_Locked, 0.f); // Yaw
    Constraint->SetAngularSwing2Limit(bCanSwing ? ACM_Limited : ACM_Locked, SwingLimit); // Pitch
    Constraint->SetAngularTwistLimit(bCanTwist ? ACM_Free : ACM_Locked, 0.f); // Roll
    Constraint->ConstraintInstance.AngularRotationOffset = bCanTwist ? FRotator(0.f, 0.f, 0.f) : FRotator(SwingAngle, 0.f, 0.f);

    Constraint->SetAngularDriveMode(EAngularDriveMode::TwistAndSwing);
    Constraint->SetAngularDriveAccelerationMode(bAccelerationMode); // You can try setting to false, but the number needs to be VERY huge. The units are slightly obscure and require number to the 12th power (documentation says force, but it should be torque?)
    Constraint->SetAngularOrientationDrive(bCanSwing, bCanTwist);
    Constraint->SetAngularVelocityDrive(bCanSwing, bCanTwist);

    Constraint->SetDisableCollision(true);
    Constraint->SetProjectionEnabled(true);
    Constraint->SetProjectionParams(1.f, 1.f, 0.1f, 1.f);
    Constraint->ConstraintInstance.ProfileInstance.ConeLimit.bSoftConstraint = false;
    Constraint->ConstraintInstance.ProfileInstance.bEnableShockPropagation = true;
    Constraint->ConstraintInstance.ProfileInstance.bUseLinearJointSolver = false;
    Constraint->ConstraintInstance.ProfileInstance.bEnableMassConditioning = false;

    return Constraint;
}

// Called when the game starts or when spawned
void AGeminiDome::BeginPlay()
{
    Super::BeginPlay();
}

// Called every frame
void AGeminiDome::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    DrawDebugString(GetWorld(), Dome->GetCenterOfMass() + FVector(0.f, 0.f, -20.f), Dome->GetComponentRotation().ToString(), nullptr, FColor::Green, 0.f, true, 1.f);
    DrawDebugString(GetWorld(), TopS->GetCenterOfMass() + FVector(0.f, 0.f, -40.f), TopS->GetComponentRotation().ToString(), nullptr, FColor::Blue, 0.f, true, 1.f);
    DrawDebugString(GetWorld(), BotS->GetCenterOfMass() + FVector(0.f, 0.f, -60.f), BotS->GetComponentRotation().ToString(), nullptr, FColor::Yellow, 0.f, true, 1.f);

    TwistComponent(DomeConstraint, DomeTwistTarget, DomeTwistStrength, DomeVelocityTarget, DomeVelocityDamping, DomeAngularThreshold);
    SwingComponent(TopSConstraint, TopSSwingTarget, TopSSwingStrength, TopSVelocityTarget, TopSVelocityDamping, TopSAngularThreshold);
    SwingComponent(BotSConstraint, BotSSwingTarget, BotSSwingStrength, BotSVelocityTarget, BotSVelocityDamping, BotSAngularThreshold);
}

void AGeminiDome::TwistComponent(
    UPhysicsConstraintComponent* Constraint,
    float TwistTarget,
    float TwistStrength,
    float VelocityTarget,
    float VelocityDamping,
    float AngularThreshold
) {
    float RelativeTwist = TwistTarget - Constraint->GetCurrentTwist();

    if (FMath::Abs(RelativeTwist) > AngularThreshold)
    {
        Constraint->SetAngularVelocityTarget(FVector(FMath::Sign(RelativeTwist) * VelocityTarget, 0.f, 0.f));
        Constraint->SetAngularDriveParams(0.f, VelocityDamping, 0.f);
    }
    else
    {
        Constraint->SetAngularOrientationTarget(FRotator(0.f, 0.f, -TwistTarget));
        Constraint->SetAngularVelocityTarget(FVector::ZeroVector);
        Constraint->SetAngularDriveParams(TwistStrength, VelocityDamping, 0.f);
    }
    Constraint->SetAngularDriveAccelerationMode(bAccelerationMode);
}

void AGeminiDome::SwingComponent(
    UPhysicsConstraintComponent* Constraint,
    float SwingTarget,
    float SwingStrength,
    float VelocityTarget,
    float VelocityDamping,
    float AngularThreshold
) {
    float RelativeSwing = FMath::UnwindDegrees(45 - SwingTarget - Constraint->GetCurrentSwing2()); // I don't understand why UE is not consistent with rotations, but this works if range is 0-90
    DrawDebugString(GetWorld(), Constraint->GetComponentLocation(), FString::SanitizeFloat(RelativeSwing), nullptr, FColor::Red, 0.f, true, 1.f); // COM label

    if (FMath::Abs(RelativeSwing) > AngularThreshold)
    {
        Constraint->SetAngularVelocityTarget(FVector(0.f, FMath::Sign(RelativeSwing) * VelocityTarget, 0.f));
        Constraint->SetAngularDriveParams(0.f, VelocityDamping, 0.f);
    }
    else
    {
        Constraint->SetAngularOrientationTarget(FRotator(SwingTarget - 45, 0.f, 0.f));
        Constraint->SetAngularVelocityTarget(FVector::ZeroVector);
        Constraint->SetAngularDriveParams(SwingStrength, VelocityDamping, 0.f);
    }
    Constraint->SetAngularDriveAccelerationMode(bAccelerationMode);
}

void AGeminiDome::SetBaseLocked(bool bLocked)
{
    Base->BodyInstance.bLockXTranslation = bLocked;
    Base->BodyInstance.bLockYTranslation = bLocked;
    Base->BodyInstance.bLockZTranslation = bLocked;
    Base->BodyInstance.bLockXRotation = bLocked;
    Base->BodyInstance.bLockYRotation = bLocked;
    Base->BodyInstance.bLockZRotation = bLocked;
}

#include "MovingDome.h"

// Sets default values
AMovingDome::AMovingDome()
{
    // Set this actor to call Tick() every frame
    PrimaryActorTick.bCanEverTick = true;

	// Initialize properties and will only update when constructed
    DomeTwistTarget = -180.f;
    DomeTwistStrength = 4000.f;
    DomeVelocityTarget = 0.003f;
    DomeVelocityDamping = 2000.f;
    DomeAngularThreshold = 1.f;

    TopSSwingTarget = -7.f;
    TopSSwingStrength = 50000.f;
    TopSVelocityTarget = 0.01f;
    TopSVelocityDamping = 20000.f;
    TopSAngularThreshold = 2.f;
    TopSAngularOffset = 38.f;
    TopSAngularLimit = 45.f; // Target range between -7 and 83 based on model (shorter range compared to reality?)

    BotSSwingTarget = -3.5f;
    BotSSwingStrength = 1000.f;
    BotSVelocityTarget = 0.01f;
    BotSVelocityDamping = 1000.f;
    BotSAngularThreshold = 0.5f;
    BotSAngularOffset = -8.25f;
    BotSAngularLimit = 4.75f; // Target range between -13 and 3.5

    VentSlideTarget = 0.f;
    VentSlideStrength = 1000.f;
    VentVelocityTarget = 20.f;
    VentVelocityDamping = 1000.f;
    VentLinearThreshold = 5.f;

	// Set static meshes for components
    const ConstructorHelpers::FObjectFinder<UStaticMesh> BaseMesh(TEXT("/Game/DomeModels/dome_base_bldg_vsp/StaticMeshes/dome_base_bldg_vsp.dome_base_bldg_vsp"));
    const ConstructorHelpers::FObjectFinder<UStaticMesh> DomeMesh(TEXT("/Game/DomeModels/dome+vent_gates_vsp/StaticMeshes/dome+vent_gates_vsp.dome+vent_gates_vsp"));
    const ConstructorHelpers::FObjectFinder<UStaticMesh> TopSMesh(TEXT("/Game/DomeModels/dome_top_shutter_vsp/StaticMeshes/dome_top_shutter_vsp.dome_top_shutter_vsp"));
    const ConstructorHelpers::FObjectFinder<UStaticMesh> BotSMesh(TEXT("/Game/DomeModels/dome_bot_shutter_vsp/StaticMeshes/dome_bot_shutter_vsp.dome_bot_shutter_vsp"));
    const ConstructorHelpers::FObjectFinder<UStaticMesh> TopVMesh(TEXT("/Game/DomeModels/dome_top_vent_vsp/StaticMeshes/dome_top_vent_vsp.dome_top_vent_vsp"));
    const ConstructorHelpers::FObjectFinder<UStaticMesh> BotVMesh(TEXT("/Game/DomeModels/dome_bot_vent_vsp/StaticMeshes/dome_bot_vent_vsp.dome_bot_vent_vsp"));
    const ConstructorHelpers::FObjectFinder<UStaticMesh> FluxCapacitorMesh(TEXT("/Engine/EngineMeshes/ParticleCube.ParticleCube"));

    RootSceneComponent = CreateDefaultSubobject<USceneComponent>("RootSceneComponent");
    RootComponent = RootSceneComponent;

	// Any changes to the components will also affect the orientation of the constraints, so you may need to update the child constraints if you change the components (so +90 component pitch, -90 constraint pitch)
    Base = CreateMeshComponent(
        "Base",                    // Name
        RootSceneComponent,        // Parent attachment that ensures new component rotates relative to parent
        BaseMesh.Object,           // Reference to mesh
        FVector::ZeroVector,       // Location relative to parent
        FRotator(0.f, 0.f, -90.f), // Rotation relative to parent
        1e29f,                     // Mass override (kg)
        FVector::ZeroVector        // Center of mass offsetting UE-generated COM
    );
    Base->SetWorldScale3D(FVector(1.f, 1.f, 1.f)); // Sets scale of entire dome, and children inheret scale because of SetupAttachment()
    SetBaseLocked(Base, true);

    Dome = CreateMeshComponent(
        "Dome",
        Base,
        DomeMesh.Object,
        FVector::ZeroVector,
        FRotator(-70.f, 0.f, 0.f),
        4000.f,
        FVector::ZeroVector
    );
    TopS = CreateMeshComponent(
        "Top Shutter",
        Dome,
        TopSMesh.Object,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        100.f,
        FVector(0.f, 1900.f, 0.f)
    );
    BotS = CreateMeshComponent(
        "Bottom Shutter",
        Dome,
        BotSMesh.Object,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        20.f,
        FVector(0.f, 1900.f, 0.f)
    );
    TopV = CreateMeshComponent(
        "Top Vent",
        Dome,
        TopVMesh.Object,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        10.f,
        FVector::ZeroVector
    );
    BotV = CreateMeshComponent(
        "Bottom Vent",
        Dome,
        BotVMesh.Object,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        10.f,
        FVector::ZeroVector
    );
    FluxCapacitor = CreateMeshComponent(
        "Flux Capacitor",
        Dome,
        FluxCapacitorMesh.Object,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        0.f,
        FVector::ZeroVector
    );

    DomeConstraint = CreateConstraintComponent(
        "Dome Constraint",          // Name
        Base,                       // Parent attachment
        Dome,                       // Child attachment
        FVector::ZeroVector,        // Location relative to parent
        FRotator(0.f, 90.f, 160.f), // Rotation relative to parent (pitch, yaw, roll)
        true,                       // Twists?
        false,                      // Swings?
        false,                      // Slides?
        0.f,                        // Swing Angle
        0.f                         // Swing limit
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

    // NOTE: Vents were simplified to slide straight up and down, although the real telescope slightly slides outwards
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
        FRotator::ZeroRotator,
        true,
        false,
        false,
        0.f,
        0.f
    );
    FluxCapacitorConstraint->SetAngularDriveParams(0.f, 1.f, 0.f);
    FluxCapacitorConstraint->SetAngularVelocityTarget(FVector(1.f, 0.f, 0.f)); // Set to always spin so model doesn't sleep
}

// Called when the game starts or when spawned
void AMovingDome::BeginPlay()
{
    Super::BeginPlay();
}

// Called every frame
void AMovingDome::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

	// Update targets based on whether the dome is open or closed
    if (bOpen)
    {
		TopSSwingTarget = 83.f; // Yea the targets are calculated from Logan's numbers, but I don't remember how I got them or where the orignal numbers are
        BotSSwingTarget = -13.f;
        VentSlideTarget = 500.f;
    }
    else
    {
        TopSSwingTarget = -7.f;
        BotSSwingTarget = 3.5f;
        VentSlideTarget = 0.f;
    }

	// Update motion of components
    TwistComponent(DomeConstraint, DomeTwistTarget, DomeTwistStrength, DomeVelocityTarget, DomeVelocityDamping, DomeAngularThreshold);
    SwingComponent(TopSConstraint, TopSSwingTarget, TopSSwingStrength, TopSVelocityTarget, TopSVelocityDamping, TopSAngularThreshold, TopSAngularOffset);
    SwingComponent(BotSConstraint, BotSSwingTarget, BotSSwingStrength, BotSVelocityTarget, BotSVelocityDamping, BotSAngularThreshold, BotSAngularOffset);
    SlideComponents();
}

void AMovingDome::SlideComponents() {
    /*for (UPhysicsConstraintComponent* Constraint : {TopVConstraint, BotVConstraint})
    {
        UPrimitiveComponent* LocalParentComponent = nullptr;
        UPrimitiveComponent* ChildComponent = nullptr;

        FName ParentBoneName;
        FName ChildBoneName;

        Constraint->GetConstrainedComponents(ChildComponent, ChildBoneName, LocalParentComponent, ParentBoneName);

        float RelativeSlide = VentSlideTarget - ChildComponent->GetRelativeLocation().Y;
        if (FMath::Abs(RelativeSlide) > VentLinearThreshold)
        {
            Constraint->SetLinearVelocityTarget(FVector(FMath::Sign(RelativeSlide) * VentVelocityTarget, 0.f, 0.f));
            Constraint->SetLinearDriveParams(0.f, VentVelocityDamping, 0.f);
        }
        else
        {
            Constraint->SetLinearPositionTarget(FVector(VentSlideTarget, 0.f, 0.f));
            Constraint->SetLinearVelocityTarget(FVector::ZeroVector);
            Constraint->SetLinearDriveParams(VentSlideStrength, VentVelocityDamping, 0.f);
        }
    }*/

    // TODO: All hardcoded currenty, so it's not very flexible for changes, especially since the dome model is not pointing in the right direction :(
    //       Eventually, change the value from component's Z to something relative to the constraints
    float RelativeSlide = VentSlideTarget - TopV->GetComponentLocation().Z + Dome->GetComponentLocation().Z;
    if (FMath::Abs(RelativeSlide) > VentLinearThreshold)
    {
        TopVConstraint->SetLinearVelocityTarget(FVector(FMath::Sign(RelativeSlide) * VentVelocityTarget, 0.f, 0.f));
        TopVConstraint->SetLinearDriveParams(0.f, VentVelocityDamping, 0.f);
    }
    else
    {
        TopVConstraint->SetLinearPositionTarget(FVector(VentSlideTarget, 0.f, 0.f));
        TopVConstraint->SetLinearVelocityTarget(FVector::ZeroVector);
        TopVConstraint->SetLinearDriveParams(VentSlideStrength, VentVelocityDamping, 0.f);
    }

    RelativeSlide = -VentSlideTarget - BotV->GetComponentLocation().Z + Dome->GetComponentLocation().Z;
    if (FMath::Abs(RelativeSlide) > VentLinearThreshold)
    {
        BotVConstraint->SetLinearVelocityTarget(FVector(FMath::Sign(RelativeSlide) * VentVelocityTarget, 0.f, 0.f));
        BotVConstraint->SetLinearDriveParams(0.f, VentVelocityDamping, 0.f);
    }
    else
    {
        BotVConstraint->SetLinearPositionTarget(FVector(-VentSlideTarget, 0.f, 0.f));
        BotVConstraint->SetLinearVelocityTarget(FVector::ZeroVector);
        BotVConstraint->SetLinearDriveParams(VentSlideStrength, VentVelocityDamping, 0.f);
    }
}

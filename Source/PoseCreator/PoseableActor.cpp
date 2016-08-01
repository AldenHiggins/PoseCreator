// Fill out your copyright notice in the Description page of Project Settings.

#include "PoseCreator.h"
#include "PoseableActor.h"

// Sets default values
APoseableActor::APoseableActor(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BoneMesh(TEXT("/Game/PoseCreator/Meshes/FirstPersonProjectileMesh"));
	boneMesh = BoneMesh.Object;
}

// Called when the game starts or when spawned
void APoseableActor::BeginPlay()
{
	Super::BeginPlay();

	// Get the skeletal mesh for the poseable actor
	TArray<UPoseableMeshComponent*>  poseableMeshes;
	GetComponents(poseableMeshes);
	if (poseableMeshes.Num() > 0)
	{
		poseableMesh = poseableMeshes[0];
	}

	// Create reference points for each of the bones
	meshBoneInfo = poseableMesh->SkeletalMesh->Skeleton->GetReferenceSkeleton().GetRefBoneInfo();
	for (int boneIndex = 0; boneIndex < meshBoneInfo.Num(); boneIndex++)
	{
		UStaticMeshComponent *newBoneReference = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass(), this);
		newBoneReference->SetStaticMesh(boneMesh);
		newBoneReference->AttachParent = GetRootComponent();
		newBoneReference->SetRelativeScale3D(FVector(.01f, .01f, .01f));
		newBoneReference->SetWorldLocation(poseableMesh->GetBoneLocationByName(meshBoneInfo[boneIndex].Name, EBoneSpaces::WorldSpace));
		newBoneReference->RegisterComponentWithWorld(this->GetWorld());
		newBoneReference->SetRenderCustomDepth(true);
		newBoneReference->SetCustomDepthStencilValue(252);

		boneReferences.Add(newBoneReference);
	}
}

// Called every frame
void APoseableActor::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );

	//FVector currentBoneLocation = poseableMesh->GetBoneLocationByName(meshBoneInfo[2].Name, EBoneSpaces::WorldSpace);

	//FVector newBoneLocation = currentBoneLocation + FVector(0.0f, 0.0f, 0.01f);

	//poseableMesh->SetBoneLocationByName(meshBoneInfo[2].Name, newBoneLocation, EBoneSpaces::WorldSpace);

	//boneReferences[2]->SetWorldLocation(newBoneLocation);

	if (gripBeingPressed)
	{
		if (boneReferenceOverlapping)
		{
			FVector newLocation = selectionSphere->GetComponentLocation();

			// Reposition the bone reference to the location of the selection sphere
			overlappedBone->SetWorldLocation(newLocation);

			// Reposition the actual bone in the skeleton to the location of the selection sphere
			poseableMesh->SetBoneLocationByName(overlappedBoneName, newLocation, EBoneSpaces::WorldSpace);
		}
	}
}


void APoseableActor::overlapBoneReference(UStaticMeshComponent *overlappedBoneInput, UStaticMeshComponent *selectionSphereInput)
{
	boneReferenceOverlapping = false;
	overlappedBone = overlappedBoneInput;
	selectionSphere = selectionSphereInput;
	overlappedBone->SetCustomDepthStencilValue(254);
	selectionSphere->SetCustomDepthStencilValue(254);

	// Search for the name of the overlapped bone
	for (int boneIndex = 0; boneIndex < boneReferences.Num(); boneIndex++)
	{
		if (boneReferences[boneIndex] == overlappedBone)
		{
			overlappedBoneName = meshBoneInfo[boneIndex].Name;
			boneReferenceOverlapping = true;
		}
	}
}

void APoseableActor::endOverlapBoneReference(UStaticMeshComponent *overlappedBoneInput, UStaticMeshComponent *selectionSphereInput)
{
	boneReferenceOverlapping = false;
	overlappedBoneInput->SetCustomDepthStencilValue(252);
	selectionSphereInput->SetCustomDepthStencilValue(253);
}


void APoseableActor::gripPressed()
{
	gripBeingPressed = true;
}

void APoseableActor::gripReleased()
{
	gripBeingPressed = false;
}


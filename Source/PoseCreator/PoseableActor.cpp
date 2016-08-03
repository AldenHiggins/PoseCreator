// Fill out your copyright notice in the Description page of Project Settings.

#include "PoseCreator.h"
#include "PoseableActor.h"
#include "Kismet/KismetMathLibrary.h"

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

	// Update all of the bone reference locations
	for (int boneIndex = 0; boneIndex < boneReferences.Num(); boneIndex++)
	{
		FVector newReferenceLocation = poseableMesh->GetBoneLocationByName(meshBoneInfo[boneIndex].Name, EBoneSpaces::WorldSpace);
		boneReferences[boneIndex]->SetWorldLocation(newReferenceLocation);
	}

	// If the grip button is being held down move the overlapped bone to the location of the controller
	if (gripBeingPressed)
	{
		if (boneReferenceOverlappingRight)
		{
			//FVector newLocation = selectionSphereLeftHand->GetComponentLocation();

			//// Reposition the bone reference to the location of the selection sphere
			//overlappedBoneLeftHand->SetWorldLocation(newLocation);

			//// Reposition the actual bone in the skeleton to the location of the selection sphere
			//poseableMesh->SetBoneLocationByName(overlappedBoneNameLeftHand, newLocation, EBoneSpaces::WorldSpace);

			// Get the vector to the next bone
			FVector vectorToRightHand = selectionSphereRightHand->GetComponentLocation() - 
				poseableMesh->GetBoneLocationByName(meshBoneInfo[overlappedBoneRighttHandInfo.ParentIndex].Name, EBoneSpaces::WorldSpace);
			// Calculate the new rotation of the bone
			vectorToRightHand.Normalize();
			 
			float dotProduct = FVector::DotProduct(startingLeftToRightVector, vectorToRightHand);

			float angleDifference = FMath::Acos(dotProduct);

			FVector rotationAxis = FVector::CrossProduct(startingLeftToRightVector, vectorToRightHand);

			FRotator newRotation = FRotator(FQuat(rotationAxis, angleDifference));

			FRotator finalRotation = UKismetMathLibrary::ComposeRotators(startingBoneRotation, newRotation);

			poseableMesh->SetBoneRotationByName(meshBoneInfo[overlappedBoneRighttHandInfo.ParentIndex].Name, finalRotation, EBoneSpaces::WorldSpace);
		}
	}
}

void APoseableActor::overlapBoneReference(UStaticMeshComponent *overlappedBoneInput, UStaticMeshComponent *selectionSphereInput, bool leftHand)
{
	if (gripBeingPressed)
	{
		return;
	}

	// Handle the left hand overlapping a bone reference
	if (leftHand)
	{
		boneReferenceOverlappingLeft = false;
		overlappedBoneLeftHand = overlappedBoneInput;
		selectionSphereLeftHand = selectionSphereInput;

		overlappedBoneLeftHand->SetCustomDepthStencilValue(254);
		selectionSphereLeftHand->SetCustomDepthStencilValue(254);

		// Search for the name of the overlapped bone
		for (int boneIndex = 0; boneIndex < boneReferences.Num(); boneIndex++)
		{
			if (boneReferences[boneIndex] == overlappedBoneLeftHand)
			{
				overlappedBoneNameLeftHand = meshBoneInfo[boneIndex].Name;
				boneReferenceOverlappingLeft = true;
			}
		}
	}
	// Handle the right hand overlapping a bone reference...should refactor this out
	else
	{
		boneReferenceOverlappingRight = false;
		overlappedBoneRightHand = overlappedBoneInput;
		selectionSphereRightHand = selectionSphereInput;

		overlappedBoneRightHand->SetCustomDepthStencilValue(254);
		selectionSphereRightHand->SetCustomDepthStencilValue(254);

		// Search for the name of the overlapped bone
		for (int boneIndex = 0; boneIndex < boneReferences.Num(); boneIndex++)
		{
			if (boneReferences[boneIndex] == overlappedBoneRightHand)
			{
				overlappedBoneNameRightHand = meshBoneInfo[boneIndex].Name;
				overlappedBoneRighttHandInfo = meshBoneInfo[boneIndex];
				overlappedBoneParentName = meshBoneInfo[meshBoneInfo[boneIndex].ParentIndex].Name;
				boneReferenceOverlappingRight = true;
			}
		}
	}
}

void APoseableActor::endOverlapBoneReference(UStaticMeshComponent *overlappedBoneInput, UStaticMeshComponent *selectionSphereInput, bool leftHand)
{
	if (gripBeingPressed)
	{
		return;
	}

	if (leftHand)
	{
		boneReferenceOverlappingLeft = false;
	}
	else
	{
		boneReferenceOverlappingRight = false;
	}
	
	overlappedBoneInput->SetCustomDepthStencilValue(252);
	selectionSphereInput->SetCustomDepthStencilValue(253);
}


void APoseableActor::gripPressed()
{
	gripBeingPressed = true;

	if (boneReferenceOverlappingRight)
	{
		FName overlappedBoneParentName = meshBoneInfo[overlappedBoneRighttHandInfo.ParentIndex].Name;

		startingLeftToRightVector = overlappedBoneRightHand->GetComponentLocation() - 
			poseableMesh->GetBoneLocationByName(overlappedBoneParentName, EBoneSpaces::WorldSpace);
		startingLeftToRightVector.Normalize();

		startingBoneRotation = poseableMesh->GetBoneRotationByName(overlappedBoneParentName, EBoneSpaces::WorldSpace);
	}
}

void APoseableActor::gripReleased()
{
	gripBeingPressed = false;

	if (selectionSphereRightHand != nullptr)
	{
		selectionSphereRightHand->SetCustomDepthStencilValue(253);
	}
	
	if (selectionSphereLeftHand != nullptr)
	{
		selectionSphereLeftHand->SetCustomDepthStencilValue(253);
	}

	if (overlappedBoneRightHand != nullptr)
	{
		overlappedBoneRightHand->SetCustomDepthStencilValue(252);
	}

	if (overlappedBoneLeftHand != nullptr)
	{
		overlappedBoneLeftHand->SetCustomDepthStencilValue(252);
	}
}


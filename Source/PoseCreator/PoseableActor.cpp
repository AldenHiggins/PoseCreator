// Fill out your copyright notice in the Description page of Project Settings.

#include "PoseCreator.h"
#include "PoseableActor.h"
#include "Kismet/KismetMathLibrary.h"


#include "Animation/AnimSequence.h"


#include "AssetRegistryModule.h"

//#include "Factories/AnimSequenceFactory.h"

//#include "AnimationEditorUtils.h"
#include "../AssetTools/Public/AssetToolsModule.h"
#include "ModuleManager.h"
//#include "../AssetTools/Private/AssetTools.h"



#define BONE_REFERENCE_DEPTH 253
#define SELECTION_DEPTH 252
#define BONE_SELECTED_DEPTH 254

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
		UStaticMeshComponent *newBoneReference = NewObject<UStaticMeshComponent>(this);
		newBoneReference->SetStaticMesh(boneMesh);
		newBoneReference->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
		newBoneReference->SetRelativeScale3D(FVector(.01f, .01f, .01f));
		newBoneReference->SetWorldLocation(poseableMesh->GetBoneLocationByName(meshBoneInfo[boneIndex].Name, EBoneSpaces::WorldSpace));
		newBoneReference->RegisterComponentWithWorld(this->GetWorld());
		newBoneReference->SetRenderCustomDepth(true);
		newBoneReference->SetCustomDepthStencilValue(BONE_REFERENCE_DEPTH);

		boneReferences.Add(newBoneReference);
	}

	// Save out the current pose to later reset
	initialPose = saveCurrentBoneState();
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

	// Moving and rotating the entire skeletal mesh
	if (rightGripBeingPressed)
	{
		if (leftGripBeingPressed)
		{
			FVector fromLeftToRightHand = rightHandSelectionSphere->GetComponentLocation() - LeftHandSelectionSphere->GetComponentLocation();
			fromLeftToRightHand.Z = 0.0f;
			fromLeftToRightHand.Normalize();

			float dotProduct = FVector::DotProduct(fromLeftToRightHand, initialGripVectorBetweenControllers);
			float angleDifference = FMath::Acos(dotProduct);

			FVector rotationAxis = FVector::CrossProduct(initialGripVectorBetweenControllers, fromLeftToRightHand);

			FRotator newRotator = FRotator(FQuat(rotationAxis, angleDifference));

			FRotator finalRotation = UKismetMathLibrary::ComposeRotators(initialActorRotation, newRotator);

			poseableMesh->SetBoneRotationByName("root", finalRotation, EBoneSpaces::WorldSpace);

		}
		else
		{
			// If the grip button is being held down move the overlapped bone to the location of the controller
			FVector distanceVector = rightHandSelectionSphere->GetComponentLocation() - rightHandInitialGripPosition;

			rightHandInitialGripPosition = rightHandSelectionSphere->GetComponentLocation();

			this->SetActorLocation(this->GetActorLocation() + distanceVector);
		}
	}

	// Rotating the selected bone of the skeleton
	if (rightTriggerBeingPressed && boneReferenceOverlappingRight)
	{
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

		FRotator finalTrackpadRotation = FRotator(FQuat(finalRotation.Vector(), trackpadRotation));

		finalRotation = UKismetMathLibrary::ComposeRotators(finalRotation, finalTrackpadRotation);

		poseableMesh->SetBoneRotationByName(meshBoneInfo[overlappedBoneRighttHandInfo.ParentIndex].Name, finalRotation, EBoneSpaces::WorldSpace);
	}
}

void APoseableActor::resetSkeleton()
{
	saveCurrentPose();
	//changeBoneState(initialPose);
}

void APoseableActor::saveCurrentPose()
{
	FString Name;
	FString PackageName;
	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(poseableMesh->SkeletalMesh->Skeleton->GetOutermost()->GetName(), TEXT("_GeneratedAnimation"), PackageName, Name);
	UAnimationAsset* NewAsset = Cast<UAnimationAsset>(AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), UAnimSequence::StaticClass(), NULL));

	if (NewAsset)
	{
		NewAsset->SetSkeleton(poseableMesh->SkeletalMesh->Skeleton);
		NewAsset->MarkPackageDirty();
	}

	UAnimSequence* NewAnimSequence = Cast<UAnimSequence>(NewAsset);

	if (NewAnimSequence)
	{
		//bool bResult = NewAnimSequence->CreateAnimation(poseableMesh->SkeletalMesh);
		const FReferenceSkeleton& RefSkeleton = poseableMesh->SkeletalMesh->RefSkeleton;
		int32 NumBones = poseableMesh->SkeletalMesh->RefSkeleton.GetNum();
		NewAnimSequence->NumFrames = 1;

		NewAnimSequence->RawAnimationData.AddZeroed(NumBones);
		NewAnimSequence->AnimationTrackNames.AddUninitialized(NumBones);

		const TArray<FTransform>& LocalAtoms = poseableMesh->LocalAtoms;

		check(LocalAtoms.Num() == NumBones);

		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			NewAnimSequence->AnimationTrackNames[BoneIndex] = RefSkeleton.GetBoneName(BoneIndex);

			FRawAnimSequenceTrack& RawTrack = NewAnimSequence->RawAnimationData[BoneIndex];

			RawTrack.PosKeys.Add(LocalAtoms[BoneIndex].GetTranslation());
			RawTrack.RotKeys.Add(LocalAtoms[BoneIndex].GetRotation());
			RawTrack.ScaleKeys.Add(LocalAtoms[BoneIndex].GetScale3D());
		}

		// refresh TrackToskeletonMapIndex
		//NewAnimSequence->RefreshTrackMapFromAnimTrackNames();

		NewAnimSequence->TrackToSkeletonMapTable.Empty();

		const USkeleton * MySkeleton = NewAnimSequence->GetSkeleton();
		NewAnimSequence->TrackToSkeletonMapTable.AddUninitialized(NumBones);

		bool bNeedsFixing = false;
		const int32 NumTracks = NewAnimSequence->AnimationTrackNames.Num();
		for (int32 I = NumTracks - 1; I >= 0; --I)
		{
			int32 BoneTreeIndex = RefSkeleton.FindBoneIndex(NewAnimSequence->AnimationTrackNames[I]);
			if (BoneTreeIndex == INDEX_NONE)
			{
				////////NewAnimSequence->RemoveTrack(I);

				if (NewAnimSequence->RawAnimationData.IsValidIndex(I))
				{
					NewAnimSequence->RawAnimationData.RemoveAt(I);
					NewAnimSequence->AnimationTrackNames.RemoveAt(I);
					NewAnimSequence->TrackToSkeletonMapTable.RemoveAt(I);

					check(NewAnimSequence->RawAnimationData.Num() == NewAnimSequence->AnimationTrackNames.Num()
						&& NewAnimSequence->AnimationTrackNames.Num() == NewAnimSequence->TrackToSkeletonMapTable.Num());
				}

			}
			else
			{
				NewAnimSequence->TrackToSkeletonMapTable[I].BoneTreeIndex = BoneTreeIndex;
			}
		}

		// should recreate track map
		NewAnimSequence->PostProcessSequence();

		FAssetRegistryModule::AssetCreated(NewAsset);
	}
}

TArray<FBoneInfo> APoseableActor::saveCurrentBoneState()
{
	TArray<FBoneInfo> savedBoneInfo;

	for (int boneIndex = 0; boneIndex < meshBoneInfo.Num(); boneIndex++)
	{
		FBoneInfo newBoneInfo;
		newBoneInfo.name = meshBoneInfo[boneIndex].Name;
		newBoneInfo.position = poseableMesh->GetBoneLocationByName(meshBoneInfo[boneIndex].Name, EBoneSpaces::WorldSpace);
		newBoneInfo.rotation = poseableMesh->GetBoneRotationByName(meshBoneInfo[boneIndex].Name, EBoneSpaces::WorldSpace);

		savedBoneInfo.Add(newBoneInfo);
	}

	return savedBoneInfo;
}

void APoseableActor::changeBoneState(TArray<FBoneInfo> newPose)
{
	for (int boneIndex = 0; boneIndex < newPose.Num(); boneIndex++)
	{
		poseableMesh->SetBoneLocationByName(newPose[boneIndex].name, newPose[boneIndex].position, EBoneSpaces::WorldSpace);
		poseableMesh->SetBoneRotationByName(newPose[boneIndex].name, newPose[boneIndex].rotation, EBoneSpaces::WorldSpace);
	}
}

void APoseableActor::overlapBoneReference(UStaticMeshComponent *overlappedBoneInput, UStaticMeshComponent *selectionSphereInput, bool leftHand)
{
	if (rightTriggerBeingPressed)
	{
		return;
	}

	// Handle the left hand overlapping a bone reference
	if (leftHand)
	{
		boneReferenceOverlappingLeft = false;
		overlappedBoneLeftHand = overlappedBoneInput;
		selectionSphereLeftHand = selectionSphereInput;

		overlappedBoneLeftHand->SetCustomDepthStencilValue(BONE_SELECTED_DEPTH);
		selectionSphereLeftHand->SetCustomDepthStencilValue(BONE_SELECTED_DEPTH);

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

		overlappedBoneRightHand->SetCustomDepthStencilValue(BONE_SELECTED_DEPTH);
		selectionSphereRightHand->SetCustomDepthStencilValue(BONE_SELECTED_DEPTH);

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
	if (rightTriggerBeingPressed)
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
	
	overlappedBoneInput->SetCustomDepthStencilValue(BONE_REFERENCE_DEPTH);
	selectionSphereInput->SetCustomDepthStencilValue(SELECTION_DEPTH);
}

void APoseableActor::gripPressed(UStaticMeshComponent *selectionSphere, bool leftHand)
{
	if (leftHand)
	{
		leftGripBeingPressed = true;

		LeftHandSelectionSphere = selectionSphere;
	}
	else
	{
		rightGripBeingPressed = true;

		rightHandSelectionSphere = selectionSphere;
		rightHandInitialGripPosition = selectionSphere->GetComponentLocation();
	}

	// If both grips are being pressed keep track of the vector between them to later rotate the poseable mesh
	if (leftGripBeingPressed && rightGripBeingPressed)
	{
		initialGripVectorBetweenControllers = rightHandSelectionSphere->GetComponentLocation() - LeftHandSelectionSphere->GetComponentLocation();
		initialGripVectorBetweenControllers.Z = 0;
		initialGripVectorBetweenControllers.Normalize();
		initialActorRotation = poseableMesh->GetBoneRotationByName("root", EBoneSpaces::WorldSpace);
	}
}

void APoseableActor::gripReleased(bool leftHand)
{
	if (leftHand)
	{
		if (rightGripBeingPressed)
		{
			rightGripBeingPressed = false;
		}
		leftGripBeingPressed = false;
	}
	else
	{
		rightGripBeingPressed = false;
	}
}

void APoseableActor::triggerPressed(UStaticMeshComponent *selectionSphere, bool leftHand)
{
	if (leftHand)
	{
		return;
	}
	else
	{
		rightTriggerBeingPressed = true;
		rightHandSelectionSphere = selectionSphere;
		trackpadRotation = 0.0f;

		if (boneReferenceOverlappingRight)
		{
			FName overlappedBoneParentName = meshBoneInfo[overlappedBoneRighttHandInfo.ParentIndex].Name;

			startingLeftToRightVector = overlappedBoneRightHand->GetComponentLocation() -
				poseableMesh->GetBoneLocationByName(overlappedBoneParentName, EBoneSpaces::WorldSpace);
			startingLeftToRightVector.Normalize();

			startingBoneRotation = poseableMesh->GetBoneRotationByName(overlappedBoneParentName, EBoneSpaces::WorldSpace);
		}
	}
}

void APoseableActor::triggerReleased(bool leftHand)
{
	if (leftHand)
	{
		return;
	}
	else
	{
		rightTriggerBeingPressed = false;

		trackpadRotation = 0.0f;

		if (selectionSphereRightHand != nullptr)
		{
			selectionSphereRightHand->SetCustomDepthStencilValue(SELECTION_DEPTH);
		}

		if (selectionSphereLeftHand != nullptr)
		{
			selectionSphereLeftHand->SetCustomDepthStencilValue(SELECTION_DEPTH);
		}

		if (overlappedBoneRightHand != nullptr)
		{
			overlappedBoneRightHand->SetCustomDepthStencilValue(BONE_REFERENCE_DEPTH);
		}

		if (overlappedBoneLeftHand != nullptr)
		{
			overlappedBoneLeftHand->SetCustomDepthStencilValue(BONE_REFERENCE_DEPTH);
		}
	}
}

void APoseableActor::rotateBoneAroundAxis(float rotationRadians)
{
	if (rightTriggerBeingPressed && boneReferenceOverlappingRight)
	{
		trackpadRotation += rotationRadians;
	}
}


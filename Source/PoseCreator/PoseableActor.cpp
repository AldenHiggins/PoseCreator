// Fill out your copyright notice in the Description page of Project Settings.

#include "PoseCreator.h"
#include "PoseableActor.h"
#include "Kismet/KismetMathLibrary.h"
#include "Animation/AnimSequence.h"
#include "AssetRegistryModule.h"
#include "../AssetTools/Public/AssetToolsModule.h"
#include "ModuleManager.h"

// Some hard coded depth values to color the highlights of elements differently
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
	TArray<UPoseableMeshComponent*> poseableMeshes;
	GetComponents(poseableMeshes);
	if (poseableMeshes.Num() > 0)
	{
		poseableMesh = poseableMeshes[0];
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("No poseable mesh component found on poseable actor, please add one!"));
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

	// Save out the first pose as the initial keyframe
	FKeyFrame firstKeyFrame;
	firstKeyFrame.boneTransforms = saveCurrentBoneState(true);
	firstKeyFrame.keyFrameTime = 0.0f;
	keyFrames.Add(firstKeyFrame);
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
		// Rotate the whole mesh
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
		// Move the whole mesh based on the movement of the right controller
		else
		{
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
		// Add in the trackpad rotation to get that final axis
		FRotator finalTrackpadRotation = FRotator(FQuat(finalRotation.Vector(), trackpadRotation));
		finalRotation = UKismetMathLibrary::ComposeRotators(finalRotation, finalTrackpadRotation);

		poseableMesh->SetBoneRotationByName(meshBoneInfo[overlappedBoneRighttHandInfo.ParentIndex].Name, finalRotation, EBoneSpaces::WorldSpace);
	}
}

void APoseableActor::resetSkeleton()
{
	// TODO: Figure out if I just want to take the first keyframe here or clear all of them out??
}

///////////////////////////////////////////////////////////
//////////////////       CONTROLS     /////////////////////
///////////////////////////////////////////////////////////

void APoseableActor::overlapBoneReference(UStaticMeshComponent *overlappedBoneInput, UStaticMeshComponent *selectionSphereInput, bool leftHand)
{
	if (rightTriggerBeingPressed)
	{
		return;
	}

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

void APoseableActor::endOverlapBoneReference(UStaticMeshComponent *overlappedBoneInput, UStaticMeshComponent *selectionSphereInput, bool leftHand)
{
	if (rightTriggerBeingPressed)
	{
		return;
	}

	boneReferenceOverlappingRight = false;

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
	// If the left hand trigger is pressed, add a keyframe to the animation we will save out
	if (leftHand)
	{
		animationPoses.Add(saveCurrentBoneState(true));

		FKeyFrame newKeyFrame;
		newKeyFrame.boneTransforms = saveCurrentBoneState(true);
		newKeyFrame.keyFrameTime = currentAnimationTime;

		for (int keyFrameIndex = 0; keyFrameIndex < keyFrames.Num(); keyFrameIndex++)
		{
			if (keyFrames[keyFrameIndex].keyFrameTime == currentAnimationTime)
			{
				UE_LOG(LogTemp, Warning, TEXT("Overwriting other keyframe instead of adding a new one!!!"));

				keyFrames[keyFrameIndex].boneTransforms = newKeyFrame.boneTransforms;
				return;
			}
		}

		keyFrames.Add(newKeyFrame);
		return;
	}
	// If the right trigger is pressed, check to see if a bone is selected and if so allow that bone to be rotated
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

///////////////////////////////////////////////////////////
//////////////////   SAVING ANIMATION  ////////////////////
///////////////////////////////////////////////////////////

void APoseableActor::saveCurrentPose()
{
	if (animationPoses.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No animation poses recorded yet, can't save out an animation!!!"));
		return;
	}

	// Generate an animation asset
	FString Name;
	FString PackageName;
	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(poseableMesh->SkeletalMesh->Skeleton->GetOutermost()->GetName(), TEXT("_GeneratedAnimation"), PackageName, Name);
	UAnimationAsset* NewAsset = Cast<UAnimationAsset>(AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName),
		UAnimSequence::StaticClass(), NULL));

	if (!NewAsset)
	{
		UE_LOG(LogTemp, Warning, TEXT("Animation could not be created!!"));
		return;
	}

	// Assign a skeletal mesh to the animation
	NewAsset->SetSkeleton(poseableMesh->SkeletalMesh->Skeleton);
	NewAsset->MarkPackageDirty();

	UAnimSequence* NewAnimSequence = Cast<UAnimSequence>(NewAsset);

	if (!NewAnimSequence)
	{
		UE_LOG(LogTemp, Warning, TEXT("Animation could not be converted to an anim sequence!!"));
		return;
	}

	// Get some data we'll need to generate the animation sequence
	const FReferenceSkeleton& RefSkeleton = poseableMesh->SkeletalMesh->RefSkeleton;
	int32 NumBones = poseableMesh->SkeletalMesh->RefSkeleton.GetNum();
	const TArray<FTransform>& LocalAtoms = poseableMesh->LocalAtoms;

	// Initialize some data for the animation sequence
	NewAnimSequence->NumFrames = animationPoses.Num();
	NewAnimSequence->SequenceLength = animationPoses.Num();
	NewAnimSequence->RawAnimationData.AddZeroed(NumBones);
	NewAnimSequence->AnimationTrackNames.AddUninitialized(NumBones);

	// Sanity check
	check(LocalAtoms.Num() == NumBones);

	// Write out the raw animation data
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		NewAnimSequence->AnimationTrackNames[BoneIndex] = RefSkeleton.GetBoneName(BoneIndex);

		FRawAnimSequenceTrack& RawTrack = NewAnimSequence->RawAnimationData[BoneIndex];

		// For each keyframe right out the data for this bone
		for (int32 keyframeIndex = 0; keyframeIndex < animationPoses.Num(); keyframeIndex++)
		{
			RawTrack.PosKeys.Add(animationPoses[keyframeIndex][BoneIndex].position);
			RawTrack.RotKeys.Add(animationPoses[keyframeIndex][BoneIndex].rotation.Quaternion());
			RawTrack.ScaleKeys.Add(LocalAtoms[BoneIndex].GetScale3D());
		}
	}

	// Empty this out, not sure if it's needed or not
	NewAnimSequence->TrackToSkeletonMapTable.Empty();
	NewAnimSequence->TrackToSkeletonMapTable.AddUninitialized(NumBones);

	// Get rid of all of the animation track data, not sure if this is required
	const int32 NumTracks = NewAnimSequence->AnimationTrackNames.Num();
	for (int32 I = NumTracks - 1; I >= 0; --I)
	{
		int32 BoneTreeIndex = RefSkeleton.FindBoneIndex(NewAnimSequence->AnimationTrackNames[I]);
		if (BoneTreeIndex == INDEX_NONE)
		{
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

	// Should recreate track map
	NewAnimSequence->PostProcessSequence();

	// Temporary includes to prevent the compiler from getting rid of the reference animation sequence
	referenceAnimationSequence->PostProcessSequence();
	referenceAnimationSequence->MarkPackageDirty();

	// Flag UE4 that the animation has been created so that it will be recognized/able to be saved
	FAssetRegistryModule::AssetCreated(NewAsset);
}

TArray<FBoneInfo> APoseableActor::saveCurrentBoneState(bool worldSpace)
{
	TArray<FBoneInfo> savedBoneInfo;

	for (int boneIndex = 0; boneIndex < meshBoneInfo.Num(); boneIndex++)
	{
		FBoneInfo newBoneInfo;
		newBoneInfo.name = meshBoneInfo[boneIndex].Name;

		if (worldSpace)
		{
			newBoneInfo.position = poseableMesh->GetBoneLocationByName(meshBoneInfo[boneIndex].Name, EBoneSpaces::WorldSpace);
			newBoneInfo.rotation = poseableMesh->GetBoneRotationByName(meshBoneInfo[boneIndex].Name, EBoneSpaces::WorldSpace);
		}
		else
		{
			newBoneInfo.position = poseableMesh->LocalAtoms[boneIndex].GetTranslation();
			newBoneInfo.rotation = FRotator(poseableMesh->LocalAtoms[boneIndex].GetRotation());
		}

		savedBoneInfo.Add(newBoneInfo);
	}

	return savedBoneInfo;
}

///////////////////////////////////////////////////////////
////////////////// ANIMATION UTILITIES ////////////////////
///////////////////////////////////////////////////////////

void APoseableActor::changeBoneState(TArray<FBoneInfo> newPose)
{
	for (int boneIndex = 0; boneIndex < newPose.Num(); boneIndex++)
	{
		poseableMesh->SetBoneRotationByName(newPose[boneIndex].name, newPose[boneIndex].rotation, EBoneSpaces::WorldSpace);
	}
}

void APoseableActor::setCurrentAnimationTime(float newAnimationTime)
{
	currentAnimationTime = newAnimationTime;

	if (animationPoses.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No animation poses saved!!!"));
		return;
	}

	// Find the two frames to interpolate between for this time in the animation
	FKeyFrame previousFrame;
	FKeyFrame nextFrame;
	bool nextFrameFound = findPreviousAndNextKeyframes(currentAnimationTime, previousFrame, nextFrame);

	// If the next frame can't be found just play the first frame
	if (!nextFrameFound)
	{
		changeBoneState(previousFrame.boneTransforms);
		return;
	}

	// Find out how much we need to interpolate these guys
	float timeDifference = nextFrame.keyFrameTime - previousFrame.keyFrameTime;
	float timePastFirstFrame = currentAnimationTime - previousFrame.keyFrameTime;

	if (timeDifference == 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Found two frames with the same time!"));
		return;
	}

	// Get the interpolated pose between the two keyframes and change the bone state to reflect that
	TArray<FBoneInfo> interpolatedPose = intepolateTwoPoses(timePastFirstFrame / timeDifference, previousFrame.boneTransforms, nextFrame.boneTransforms);
	changeBoneState(interpolatedPose);
}


TArray<FBoneInfo> APoseableActor::intepolateTwoPoses(float percentageOfSecondPose, TArray<FBoneInfo> firstPose, TArray<FBoneInfo> secondPose)
{
	if (firstPose.Num() != secondPose.Num())
	{
		UE_LOG(LogTemp, Error, TEXT("The two poses to interpolate don't have the same number of bones"));
		return firstPose;
	}

	// Check to see if any interpolation is needed
	if (percentageOfSecondPose == 0.0f)
	{
		return firstPose;
	}
	else if (percentageOfSecondPose == 1.0f)
	{
		return secondPose;
	}

	// The new pose that will be generated
	TArray<FBoneInfo> interpolatedPose;

	for (int boneIndex = 0; boneIndex < firstPose.Num(); boneIndex++)
	{
		FBoneInfo firstPoseBone = firstPose[boneIndex];
		FBoneInfo secondPoseBone = secondPose[boneIndex];

		FTransform firstBoneTransform(firstPoseBone.rotation, firstPoseBone.position);
		FTransform secondBoneTransform(secondPoseBone.rotation, secondPoseBone.position);

		ScalarRegister firstBlendWeight(1.0f - percentageOfSecondPose);
		ScalarRegister secondBlendWeight(percentageOfSecondPose);

		firstBoneTransform = firstBoneTransform * firstBlendWeight;
		firstBoneTransform.AccumulateWithShortestRotation(secondBoneTransform, secondBlendWeight);

		FBoneInfo newBonePose;
		newBonePose.name = firstPoseBone.name;
		newBonePose.position = firstBoneTransform.GetLocation();
		newBonePose.rotation = FRotator(firstBoneTransform.GetRotation());

		interpolatedPose.Add(newBonePose);
	}

	return interpolatedPose;
}

bool APoseableActor::findPreviousAndNextKeyframes(float timeToCheck, FKeyFrame &previousKeyFrame, FKeyFrame &nextKeyFrame)
{
	if (keyFrames.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No keyframes saved!!!"));
		return false;
	}

	FKeyFrame previousFrame = keyFrames[0];
	FKeyFrame nextFrame;
	nextFrame.keyFrameTime = 10000.0f;

	for (int keyFrameIndex = 0; keyFrameIndex < keyFrames.Num(); keyFrameIndex++)
	{
		FKeyFrame *thisKeyFrame = &keyFrames[keyFrameIndex];

		if (thisKeyFrame->keyFrameTime >= currentAnimationTime)
		{
			if (nextFrame.keyFrameTime > thisKeyFrame->keyFrameTime)
			{
				nextFrame = *thisKeyFrame;
			}
		}
		else if (thisKeyFrame->keyFrameTime < currentAnimationTime)
		{
			if (previousFrame.keyFrameTime < thisKeyFrame->keyFrameTime)
			{
				previousFrame = *thisKeyFrame;
			}
		}
	}

	previousKeyFrame = previousFrame;
	nextKeyFrame = nextFrame;

	if (nextFrame.keyFrameTime == 10000.0f)
	{
		return false;
	}
	else
	{
		return true;
	}
}
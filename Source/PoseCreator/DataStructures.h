#pragma once

#include "PoseCreator.h"
#include "DataStructures.generated.h"

USTRUCT()
struct FBoneInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FRotator rotation;
	UPROPERTY()
	FVector position;
	UPROPERTY()
	FName name;
};
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PoseCreator : ModuleRules
{
	public PoseCreator(ReadOnlyTargetRules Target) : base(Target)
    {
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });

        //PrivateDependencyModuleNames.AddRange(new string[] { "AssetTools" });

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "AssetTools"
            }
        );

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "AssetTools"
            }
        );
    }
}

// Copyright 2020 NeuralVFX, Inc. All Rights Reserved.

#include "Asset.h"
#include "Helpers.h"
#include "Runtime/LevelSequence/Public/LevelSequenceActor.h"
#include "Runtime/LevelSequence/Public/LevelSequence.h"
#include "MovieScene.h"
#include "Engine/StaticMesh.h"
#include "Engine/ObjectLibrary.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h" 
#include "Tracks/MovieScene3DTransformTrack.h"


UAsset::UAsset()
{
}


UAsset::~UAsset()
{
}


void UAsset::Init(ALevelSequenceActor* CurrentSequencer, FString NewSceneName)
{
	SceneName = NewSceneName;
	Sequencer = CurrentSequencer;

	World = Sequencer->GetWorld();
	Type = "Abstract";
}



FGuid UAsset::GetGuid()
{
	FGuid Guid = UHelpers::GetGuidFromSequencer(Sequencer, SceneName);
	return Guid;
}


//AActor* UAsset::GetActor()
//{
//	FGuid Guid = GetGuid();
//	AActor* Actor = UHelpers::GetActorFromSequencer(Sequencer,Guid);
//
//	return Actor;
//}

AActor* UAsset::GetActor()
{
	FGuid Guid = GetGuid();
	ULevelSequence* Sequence = UHelpers::GetSequence(Sequencer);

	if (Sequence && Sequencer)
	{
		// Use the modern UE 5 Universal Object Locator resolve parameters
		UE::UniversalObjectLocator::FResolveParams ResolveParams(Sequencer->GetWorld());
		TArray<UObject*, TInlineAllocator<1>> BoundObjs;

		// Query the Sequencer for what is *actually* bound to this GUID right now
		Sequence->LocateBoundObjects(Guid, ResolveParams, BoundObjs);

		if (BoundObjs.Num() > 0)
		{
			return Cast<AActor>(BoundObjs[0]);
		}
	}

	return nullptr;
}

void UAsset::Load()
{

}


void UAsset::Unload()
{
	if (IsLoaded())
	{
		FGuid Guid = GetGuid();
		ULevelSequence* Sequence = UHelpers::GetSequence(Sequencer);

		if (Sequence && Sequence->GetMovieScene())
		{
			// 1. Unbind BEFORE you Remove (Fixes the Sequencer API crash)
			Sequence->UnbindPossessableObjects(Guid);
			Sequence->GetMovieScene()->RemovePossessable(Guid);
		}

		// 2. Safely check the Weak Pointer. 
		// If the Actor is still alive, this returns true. If it was already deleted, it safely skips.
		if (CachedActor.IsValid())
		{
			CachedActor.Get()->Destroy();
			CachedActor.Reset(); // Clear the weak pointer out
		}

		// Make sure Guid is no longer valid
		Guid.Invalidate();
	}
}



bool UAsset::IsLoaded()
{
	// Check sequencer to see if object is loaded in scene
	FGuid Guid = GetGuid();

	return Guid.IsValid();
}
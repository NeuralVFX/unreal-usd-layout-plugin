// Copyright 2020 NeuralVFX, Inc. All Rights Reserved.

#include "USDLayout.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Helpers.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Runtime/LevelSequence/Public/LevelSequenceActor.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlayer.h"
#include "Misc/QualifiedFrameTime.h"



bool UUSDLayout::WriteUSDLayoutData(FString USDFilePath, TArray<AStaticMeshActor*> ActorArray, ALevelSequenceActor* Sequencer)
{
   
	std::string FilePathStr = TCHAR_TO_UTF8(*USDFilePath);
	pxr::UsdStageRefPtr Stage = pxr::UsdStage::CreateNew(FilePathStr);
	if (!Stage)
	{
		return false;
	}
	Stage->DefinePrim(pxr::SdfPath("/Root/StaticMeshes"), pxr::TfToken("Scope"));
	Stage->DefinePrim(pxr::SdfPath("/Root/AnimatedStaticMeshes"), pxr::TfToken("Scope"));
	Stage->DefinePrim(pxr::SdfPath("/Root/SkeletalMeshes"), pxr::TfToken("Scope"));
	Stage->DefinePrim(pxr::SdfPath("/Root/Camera"), pxr::TfToken("Scope"));

	// Loop through actors and create structs for JSON
	for (AStaticMeshActor* Actor : ActorArray)
	{

		    
			FGuid Guid = UHelpers::GetGuidFromSequencer(Sequencer, UKismetSystemLibrary::GetDisplayName(Actor));
			UMovieScene3DTransformSection* CurrentTranSection = UHelpers::GetTransformSection(Sequencer,Guid);
			// Check if actor has animation and record data accordingly
			if (CurrentTranSection)
			{
				pxr::UsdPrim Prim = MakeAnimatedStaticMesh(Sequencer, Actor, Guid, Stage);

			}
			else
			{
				pxr::UsdPrim Prim = MakeStaticMesh(Actor, Stage);
			}
	}

	Stage->GetRootLayer()->Save();
	
	return true;
}


pxr::UsdStageRefPtr UUSDLayout::ReadUSDLayoutData(FString USDFilePath)
{
	std::string FilePathStr = TCHAR_TO_UTF8(*USDFilePath);
	pxr::UsdStageRefPtr Stage = pxr::UsdStage::Open(FilePathStr);

	return Stage;
	

}


pxr::UsdPrim  UUSDLayout::MakeStaticMesh(AStaticMeshActor* Actor, pxr::UsdStageRefPtr Stage)
{
	// Extract scene name and content name
	std::string PathName = TCHAR_TO_UTF8(*Actor->GetStaticMeshComponent()->GetStaticMesh()->GetPathName());
	std::string SimpleName = TCHAR_TO_UTF8(*UKismetSystemLibrary::GetDisplayName(Actor));

	// Define the prim on the stage
	std::string PrimPath = "/Root/StaticMeshes/" + SimpleName;
	pxr::UsdPrim Prim = Stage->DefinePrim(pxr::SdfPath(PrimPath), pxr::TfToken("Xform"));

	// Create custom attributes
	Prim.CreateAttribute(pxr::TfToken("unrealAssetPath"), pxr::SdfValueTypeNames->String).Set(PathName);
	Prim.CreateAttribute(pxr::TfToken("unrealSceneName"), pxr::SdfValueTypeNames->String).Set(SimpleName);
	// Extract transformation How does this work? matrix
	FMatrix UETransformMatrix = Actor->GetTransform().ToMatrixWithScale();

	// Convert Unreal FMatrix (float) to OpenUSD GfMatrix4d (double)
	pxr::GfMatrix4d UsdMatrix(
		UETransformMatrix.M[0][0], UETransformMatrix.M[0][1], UETransformMatrix.M[0][2], UETransformMatrix.M[0][3],
		UETransformMatrix.M[1][0], UETransformMatrix.M[1][1], UETransformMatrix.M[1][2], UETransformMatrix.M[1][3],
		UETransformMatrix.M[2][0], UETransformMatrix.M[2][1], UETransformMatrix.M[2][2], UETransformMatrix.M[2][3],
		UETransformMatrix.M[3][0], UETransformMatrix.M[3][1], UETransformMatrix.M[3][2], UETransformMatrix.M[3][3]
	);

	// 1. Create and set the matrix xformOp attribute
	pxr::UsdAttribute MatrixAttr = Prim.CreateAttribute(pxr::TfToken("xformOp:transform"), pxr::SdfValueTypeNames->Matrix4d);
	MatrixAttr.Set(UsdMatrix);

	// 2. Tell USD which transform operations to evaluate (xformOpOrder)
	pxr::UsdAttribute OrderAttr = Prim.CreateAttribute(pxr::TfToken("xformOpOrder"), pxr::SdfValueTypeNames->TokenArray);
	OrderAttr.Set(pxr::VtTokenArray{ pxr::TfToken("xformOp:transform") });


	return Prim;
}


pxr::UsdPrim UUSDLayout::MakeAnimatedStaticMesh(class ALevelSequenceActor* Sequencer, AStaticMeshActor* Actor, FGuid Guid, pxr::UsdStageRefPtr Stage)
{

	std::string PathName = TCHAR_TO_UTF8(*Actor->GetStaticMeshComponent()->GetStaticMesh()->GetPathName());
	std::string SimpleName = TCHAR_TO_UTF8(*UKismetSystemLibrary::GetDisplayName(Actor));



	// Define the prim on the stage
	std::string PrimPath = "/Root/AnimatedStaticMeshes/" + SimpleName;
	pxr::UsdPrim Prim = Stage->DefinePrim(pxr::SdfPath(PrimPath), pxr::TfToken("Xform"));

	// Create custom attributes
	Prim.CreateAttribute(pxr::TfToken("unrealAssetPath"), pxr::SdfValueTypeNames->String).Set(PathName);
	Prim.CreateAttribute(pxr::TfToken("unrealSceneName"), pxr::SdfValueTypeNames->String).Set(SimpleName);
	pxr::UsdAttribute MatrixAttr = Prim.CreateAttribute(pxr::TfToken("xformOp:transform"), pxr::SdfValueTypeNames->Matrix4d);

	pxr::UsdAttribute OrderAttr = Prim.CreateAttribute(pxr::TfToken("xformOpOrder"), pxr::SdfValueTypeNames->TokenArray);
	pxr::VtArray<pxr::TfToken> XformOpOrder = { pxr::TfToken("xformOp:transform") };
	OrderAttr.Set(XformOpOrder);

	// 1. Get the sequence player and save its current position so we don't mess up the Editor UI
	ULevelSequencePlayer* SequencePlayer = Sequencer->GetSequencePlayer();

	// 1. Get the MovieScene from your Sequencer
	ULevelSequence* Sequence = Sequencer->GetSequence();
	UMovieScene* MovieScene = Sequence->GetMovieScene();

	// 3. Get the global playback bounds in raw Ticks
	TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();

	// Get transform channels
	// Get transformation track from sequencer
	UMovieScene3DTransformSection* CurrentTranSection = UHelpers::GetTransformSection(Sequencer, Guid);
	const FMovieSceneChannelProxy& channelProxy = CurrentTranSection->GetChannelProxy();
	auto ChannelArray = channelProxy.GetChannels<FMovieSceneDoubleChannel>();

	// 4. Convert the raw Ticks into standard Display Frames so we can loop nicely (e.g., Frame 0 to 150)
	FFrameNumber StartFrame = UHelpers::GetFrameNumberTick(Sequencer, CurrentTranSection->GetInclusiveStartFrame().Value, true);
	FFrameNumber EndFrame = UHelpers::GetFrameNumberTick(Sequencer, CurrentTranSection->GetExclusiveEndFrame().Value, true);
	// Loop through frames
	for (int i = StartFrame.Value; i < EndFrame.Value; i++)
	{
		FFrameNumber Frame = UHelpers::GetFrameNumberTick(Sequencer, i, false);


		float tx, ty, tz, rx, ry, rz, sx, sy, sz;

		if (ChannelArray.Num() >= 9)
		{
			ChannelArray[0]->Evaluate(Frame, tx);
			ChannelArray[1]->Evaluate(Frame, ty);
			ChannelArray[2]->Evaluate(Frame, tz);

			ChannelArray[3]->Evaluate(Frame, rx);
			ChannelArray[4]->Evaluate(Frame, ry);
			ChannelArray[5]->Evaluate(Frame, rz);

			ChannelArray[6]->Evaluate(Frame, sx);
			ChannelArray[7]->Evaluate(Frame, sy);
			ChannelArray[8]->Evaluate(Frame, sz);
		}

		// 1. Build Vector, Rotator, and Scale components
		FVector Translation(tx, ty, tz);
		FRotator Rotation(ry, rz, rx); // Note: verify if your channels store Rotator (Pitch, Yaw, Roll) or Vector-based angles
		FVector Scale(sx, sy, sz);

		// 2. Construct the FTransform
		FTransform LocalTransform(Rotation, Translation, Scale);
		FMatrix UETransformMatrix = LocalTransform.ToMatrixWithScale();

		// Convert Unreal FMatrix (float) to OpenUSD GfMatrix4d (double)
		pxr::GfMatrix4d UsdMatrix(
			UETransformMatrix.M[0][0], UETransformMatrix.M[0][1], UETransformMatrix.M[0][2], UETransformMatrix.M[0][3],
			UETransformMatrix.M[1][0], UETransformMatrix.M[1][1], UETransformMatrix.M[1][2], UETransformMatrix.M[1][3],
			UETransformMatrix.M[2][0], UETransformMatrix.M[2][1], UETransformMatrix.M[2][2], UETransformMatrix.M[2][3],
			UETransformMatrix.M[3][0], UETransformMatrix.M[3][1], UETransformMatrix.M[3][2], UETransformMatrix.M[3][3]
		);

		// 1. Create and set the matrix xformOp attribute
		MatrixAttr.Set(UsdMatrix, i);
	}

	// 5. Restore the Sequencer to its original time so the Editor timeline doesn't get left at the end of the clip
	return Prim;
}

// Copyright 2020 NeuralVFX, Inc. All Rights Reserved.

#include "AnimatedSkelMeshAsset.h"
#include "Helpers.h"
#include "Runtime/LevelSequence/Public/LevelSequenceActor.h"
#include "Runtime/LevelSequence/Public/LevelSequence.h"
#include "MovieScene.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "AssetImportTask.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "Misc/Paths.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"

void UAnimatedSkelMeshAsset::Init(ALevelSequenceActor* CurrentSequencer, pxr::UsdStageRefPtr NewStage, pxr::UsdPrim NewPrim)
{
    pxr::UsdAttribute AssetPathAttr = NewPrim.GetAttribute(pxr::TfToken("unrealAssetPath"));
    std::string AssetPathStr;
    AssetPathAttr.Get(&AssetPathStr);

    pxr::UsdAttribute SceneNameAttr = NewPrim.GetAttribute(pxr::TfToken("unrealSceneName"));
    std::string SceneNameStr;
    SceneNameAttr.Get(&SceneNameStr);

    pxr::UsdAttribute TransformAttr = NewPrim.GetAttribute(pxr::TfToken("xformOp:transform"));
    pxr::GfMatrix4d UsdMatrix;
    TransformAttr.Get(&UsdMatrix);

    /*FMatrix UETransformMatrix;
    UETransformMatrix.M[0][0] = UsdMatrix[0][0]; UETransformMatrix.M[0][1] = UsdMatrix[0][1]; UETransformMatrix.M[0][2] = UsdMatrix[0][2]; UETransformMatrix.M[0][3] = UsdMatrix[0][3];
    UETransformMatrix.M[1][0] = UsdMatrix[1][0]; UETransformMatrix.M[1][1] = UsdMatrix[1][1]; UETransformMatrix.M[1][2] = UsdMatrix[1][2]; UETransformMatrix.M[1][3] = UsdMatrix[1][3];
    UETransformMatrix.M[2][0] = UsdMatrix[2][0]; UETransformMatrix.M[2][1] = UsdMatrix[2][1]; UETransformMatrix.M[2][2] = UsdMatrix[2][2]; UETransformMatrix.M[2][3] = UsdMatrix[2][3];
    UETransformMatrix.M[3][0] = UsdMatrix[3][0]; UETransformMatrix.M[3][1] = UsdMatrix[3][1]; UETransformMatrix.M[3][2] = UsdMatrix[3][2]; UETransformMatrix.M[3][3] = UsdMatrix[3][3];
    */
    SceneName = UTF8_TO_TCHAR(SceneNameStr.c_str());
    ContentName = UTF8_TO_TCHAR(AssetPathStr.c_str());
    Sequencer = CurrentSequencer;
    World = Sequencer ? Sequencer->GetWorld() : nullptr;
    Type = "Animated SkelMesh";
    CutName = UHelpers::MakePrettyContentName(ContentName);

    //Transform = FTransform(UETransformMatrix);
    Stage = NewStage;
    Prim = NewPrim;
}

void UAnimatedSkelMeshAsset::Load()
{
    if (!IsLoaded())
    {
        ULevelSequence* Sequence = UHelpers::GetSequence(Sequencer);
        if (Sequence)
        {
            // 1. Spawn Actor & Apply Transform
            ASkeletalMeshActor* SpawnedActor = World->SpawnActor<ASkeletalMeshActor>();
            SpawnedActor->SetActorLabel(SceneName);
            //SpawnedActor->SetActorTransform(Transform);

            CachedActor = Cast<AActor>(SpawnedActor);

            // 2. Load Mesh and Extract Skeleton
            USkeletalMesh* SkelMeshAsset = Cast<USkeletalMesh>(StaticLoadObject(USkeletalMesh::StaticClass(), nullptr, *ContentName));
            USkeleton* TargetSkeleton = nullptr;

            if (SkelMeshAsset)
            {
                SpawnedActor->GetSkeletalMeshComponent()->SetSkeletalMesh(SkelMeshAsset);
                SpawnedActor->GetSkeletalMeshComponent()->BoundsScale = 900000.0f; // Mirroring your Python bounds scaling

                TargetSkeleton = SkelMeshAsset->GetSkeleton();
            }

            // 3. Sequencer Setup
            FGuid Guid = Sequence->MovieScene->AddPossessable(SpawnedActor->GetActorLabel(), SpawnedActor->GetClass());
            Sequence->BindPossessableObject(Guid, *SpawnedActor, SpawnedActor->GetWorld());

            UMovieSceneSkeletalAnimationTrack* AnimTrack = Cast<UMovieSceneSkeletalAnimationTrack>(Sequence->MovieScene->AddTrack(UMovieSceneSkeletalAnimationTrack::StaticClass(), Guid));
            UMovieSceneSection* Section = AnimTrack->CreateNewSection();
            UMovieSceneSkeletalAnimationSection* AnimSection = Cast<UMovieSceneSkeletalAnimationSection>(Section);
            AnimTrack->AddSection(*AnimSection);

            // 4. Construct FBX Path & Import
            FString StageFilePath = UTF8_TO_TCHAR(Stage->GetRootLayer()->GetRealPath().c_str());
            FString StageDir = FPaths::GetPath(StageFilePath);
            FString FbxAnimPath = FPaths::Combine(StageDir, TEXT("skel_anim"), SceneName + TEXT(".fbx"));
            FString AnimDestFolder = TEXT("/Game/Anim");

            UAnimSequence* ImportedAnim = ImportFbxAnimation(FbxAnimPath, AnimDestFolder, SceneName + TEXT("_Anim"), TargetSkeleton);

            // 5. Apply Animation to Sequencer
            if (ImportedAnim)
            {
                AnimSection->Params.Animation = ImportedAnim;

                FFrameNumber StartFrame = Sequence->MovieScene->GetPlaybackRange().GetLowerBoundValue();
                FFrameNumber EndFrame = StartFrame + FFrameNumber(FMath::RoundToInt32(ImportedAnim->GetPlayLength() * Sequence->MovieScene->GetTickResolution().AsDecimal()));
                AnimSection->SetRange(TRange<FFrameNumber>(StartFrame, EndFrame));
                AnimSection->SetIsActive(true);
            }
        }
    }
}

UAnimSequence* UAnimatedSkelMeshAsset::ImportFbxAnimation(const FString& FilePath, const FString& DestPath, const FString& AnimName, USkeleton* TargetSkeleton)
{
    FString FullAssetPath = FString::Printf(TEXT("%s/%s.%s"), *DestPath, *AnimName, *AnimName);

    UAnimSequence* ExistingAnim = Cast<UAnimSequence>(StaticLoadObject(UAnimSequence::StaticClass(), nullptr, *FullAssetPath));
    if (ExistingAnim)
    {
        return ExistingAnim;
    }

    if (!FPaths::FileExists(FilePath))
    {
        UE_LOG(LogTemp, Warning, TEXT("FBX Animation file not found: %s"), *FilePath);
        return nullptr;
    }

    // Mirroring factory.ImportUI from Python
    UFbxImportUI* ImportUI = NewObject<UFbxImportUI>();

    // CRITICAL FIX: Stop Unreal from overriding our import type just because it sees geometry in the FBX
    ImportUI->bAutomatedImportShouldDetectType = false;

    ImportUI->bIsObjImport = false;
    ImportUI->MeshTypeToImport = FBXIT_Animation;
    ImportUI->Skeleton = TargetSkeleton;
    ImportUI->bImportAnimations = true;
    ImportUI->bImportMesh = false;
    ImportUI->bImportMaterials = false;
    ImportUI->bImportTextures = false;

    UFbxFactory* FbxFactory = NewObject<UFbxFactory>();
    FbxFactory->ImportUI = ImportUI;

    UAssetImportTask* ImportTask = NewObject<UAssetImportTask>();
    ImportTask->Filename = FilePath;
    ImportTask->DestinationPath = DestPath;
    ImportTask->DestinationName = AnimName;
    ImportTask->bAutomated = true;
    ImportTask->bSave = false;
    ImportTask->bReplaceExisting = true;

    ImportTask->Factory = FbxFactory;
    ImportTask->Options = ImportUI;

    TArray<UAssetImportTask*> ImportTasks;
    ImportTasks.Add(ImportTask);

    FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
    AssetToolsModule.Get().ImportAssetTasks(ImportTasks);

    if (ImportTask->GetObjects().Num() > 0)
    {
        return Cast<UAnimSequence>(ImportTask->GetObjects()[0]);
    }

    return nullptr;
}
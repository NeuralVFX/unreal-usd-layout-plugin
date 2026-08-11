#pragma once

#include "CoreMinimal.h"
#include "Asset.h" 

// OpenUSD Includes
#include "USDIncludesStart.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/attribute.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/usd/sdf/types.h"
#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/vt/array.h"
#include "USDIncludesEnd.h"

#include "AnimatedSkelMeshAsset.generated.h"

// Forward declarations
class ASkeletalMeshActor;
class UAnimSequence;
class USkeleton;

UCLASS()
class LAYOUTEDITOR_API UAnimatedSkelMeshAsset : public UAsset
{
    GENERATED_BODY()

public:
    using UAsset::Init;

    void Init(ALevelSequenceActor* CurrentSequencer, pxr::UsdStageRefPtr NewStage, pxr::UsdPrim NewPrim);

    virtual void Load() override;

protected:
    UAnimSequence* ImportFbxAnimation(const FString& FilePath, const FString& DestPath, const FString& AnimName, USkeleton* TargetSkeleton);

private:
    FTransform Transform;

    pxr::UsdStageRefPtr Stage;
    pxr::UsdPrim Prim;
};
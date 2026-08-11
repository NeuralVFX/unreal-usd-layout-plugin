// Copyright 2020 NeuralVFX, Inc. All Rights Reserved.

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

#include "Camera.generated.h"

class ACineCameraActor;

UCLASS()
class LAYOUTEDITOR_API UCamera : public UAsset
{
	GENERATED_BODY()

public:

	// 1. Silence the hidden base function warning
	using UAsset::Init;

	virtual void Init(ALevelSequenceActor* CurrentSequencer, pxr::UsdStageRefPtr NewStage, pxr::UsdPrim NewPrim);
	
	virtual void Load() override;
	virtual void Unload() override; // <--- Add this override

	// Spawns the ACineCameraActor and applies the static USD filmback properties
	ACineCameraActor* MakeActor();

	FGuid GetCompGuid(); // <--- Add your helper function here

	pxr::UsdPrim Prim;
	pxr::UsdStageRefPtr Stage;
};
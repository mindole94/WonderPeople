// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneImporter.h"

#include "DatasmithRuntimeUtils.h"

#include "DatasmithAreaLightActor.h"
#include "DatasmithSceneFactory.h"
#include "DatasmithUtils.h"
#include "IDatasmithSceneElements.h"

#include "Components/ChildActorComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponent.h"
#include "Components/LightmassPortalComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Light.h"
#include "Engine/PointLight.h"
#include "Engine/SkyLight.h"
#include "Engine/SpotLight.h"
#include "Engine/TextureLightProfile.h"
#include "Engine/World.h"

#include "Lightmass/LightmassPortal.h"
#include "Math/Quat.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace DatasmithRuntime
{
	extern const FString TexturePrefix;
	extern const FString MaterialPrefix;
	extern const FString MeshPrefix;

	extern void RenameObject(UObject* Object, const TCHAR* DesiredName);

	/** Helper method to set up the properties common to all types of light components */
	void SetupLightComponent(FActorData& ActorData, IDatasmithLightActorElement* LightElement);

	USceneComponent* ImportAreaLightComponent(FActorData& ActorData, IDatasmithAreaLightElement* AreaLightElement, USceneComponent* Parent);

	USceneComponent* CreateComponent(FActorData& ActorData, UClass* Class, USceneComponent* Parent);

	template<typename T>
	T* CreateComponent(FActorData& ActorData, USceneComponent* Parent)
	{
		return Cast<T>(CreateComponent(ActorData, T::StaticClass(), Parent));
	}

	template<typename T>
	T* CreateActor(UWorld* World)
	{
		return Cast<T>(World->SpawnActor(T::StaticClass(), nullptr, nullptr));
	}

	bool FSceneImporter::ProcessLightActorData(FActorData& ActorData, IDatasmithLightActorElement* LightActorElement)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FSceneImporter::ProcessLightActorData);

		if (ActorData.HasState(EAssetState::Processed))
		{
			return true;
		}

		if (LightActorElement->GetUseIes() && FCString::Strlen(LightActorElement->GetIesTexturePathName()) > 0)
		{
			if (FSceneGraphId* ElementIdPtr = AssetElementMapping.Find(TexturePrefix + LightActorElement->GetIesTexturePathName()))
			{
				FActionTaskFunction AssignTextureFunc = [this](UObject* Object, const FReferencer& Referencer) -> EActionResult::Type
				{
					return this->AssignProfileTexture(Referencer, Cast<UTextureLightProfile>(Object));
				};

				ProcessTextureData(*ElementIdPtr);
				ActorData.AssetId = *ElementIdPtr;

				AddToQueue(EQueueTask::NonAsyncQueue, { AssignTextureFunc, *ElementIdPtr, { EDataType::Actor, ActorData.ElementId, 0 } });
			}
		}

		FActionTaskFunction CreateLightFunc = [this](UObject* Object, const FReferencer& Referencer) -> EActionResult::Type
		{
			return this->CreateLightComponent(Referencer.GetId());
		};

		AddToQueue(EQueueTask::NonAsyncQueue, { CreateLightFunc, { EDataType::Actor, ActorData.ElementId, 0 } });
		TasksToComplete |= EWorkerTask::LightComponentCreate;

		ActorData.SetState(EAssetState::Processed);

		return true;
	}

	EActionResult::Type FSceneImporter::AssignProfileTexture(const FReferencer& Referencer, UTextureLightProfile* TextureProfile)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FSceneImporter::AssignProfileTexture);

		if (TextureProfile == nullptr)
		{
			ensure(Referencer.Type == (uint8)EDataType::Actor);
			return EActionResult::Failed;
		}

		const FSceneGraphId ActorId = Referencer.GetId();
		ensure(ActorDataList.Contains(ActorId));

		FActorData& ActorData = ActorDataList[ActorId];

		if (!ActorData.HasState(EAssetState::Completed))
		{
			return EActionResult::Retry;
		}

		ActionCounter.Increment();

		if (ULightComponent* LightComponent = ActorData.GetObject<ULightComponent>())
		{
			LightComponent->IESTexture = TextureProfile;
		}
		else if (UChildActorComponent* ChildActorComponent = ActorData.GetObject<UChildActorComponent>())
		{
			if (ADatasmithAreaLightActor* LightShapeActor = Cast< ADatasmithAreaLightActor >(ChildActorComponent->GetChildActor()))
			{
				LightShapeActor->IESTexture = TextureProfile;
			}
		}
		else
		{
			ensure(false);
			return EActionResult::Failed;
		}

		return EActionResult::Succeeded;
	}

	EActionResult::Type FSceneImporter::CreateLightComponent(FSceneGraphId ActorId)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FSceneImporter::CreateLightComponent);

		FActorData& ActorData = ActorDataList[ActorId];

		IDatasmithLightActorElement* LightElement = static_cast<IDatasmithLightActorElement*>(Elements[ActorId].Get());

		USceneComponent* LightComponent = ActorData.GetObject<USceneComponent>();

		if ( LightElement->IsA(EDatasmithElementType::AreaLight) )
		{
			IDatasmithAreaLightElement* AreaLightElement = static_cast<IDatasmithAreaLightElement*>(LightElement);
			LightComponent = ImportAreaLightComponent( ActorData, AreaLightElement, RootComponent.Get() );
		}
		else if ( LightElement->IsA( EDatasmithElementType::LightmassPortal ) )
		{
			if (LightComponent == nullptr)
			{
				if (ImportOptions.BuildHierarchy != EBuildHierarchyMethod::None && !LightElement->IsAComponent())
				{
					ALightmassPortal* Actor = CreateActor<ALightmassPortal>(RootComponent->GetOwner()->GetWorld());
					LightComponent = Actor->GetPortalComponent();
				}
				else
				{
					LightComponent = CreateComponent< ULightmassPortalComponent >(ActorData, RootComponent.Get());
				}
			}
		}
		else if ( LightElement->IsA( EDatasmithElementType::DirectionalLight ) )
		{
			if (LightComponent == nullptr)
			{
				if (ImportOptions.BuildHierarchy != EBuildHierarchyMethod::None && !LightElement->IsAComponent())
				{
					ADirectionalLight* Actor = CreateActor<ADirectionalLight>(RootComponent->GetOwner()->GetWorld());
					LightComponent = Actor->GetLightComponent();
				}
				else
				{
					LightComponent = CreateComponent< UDirectionalLightComponent >(ActorData, RootComponent.Get());
				}
			}
		}
		else if ( LightElement->IsA( EDatasmithElementType::SpotLight ) )
		{
			USpotLightComponent* SpotLightComponent = Cast<USpotLightComponent>(LightComponent);

			if (SpotLightComponent == nullptr)
			{
				if (ImportOptions.BuildHierarchy != EBuildHierarchyMethod::None && !LightElement->IsAComponent())
				{
					ASpotLight* Actor = CreateActor<ASpotLight>(RootComponent->GetOwner()->GetWorld());
					SpotLightComponent = Cast<USpotLightComponent>(Actor->GetLightComponent());
				}
				else
				{
					SpotLightComponent = CreateComponent< USpotLightComponent >(ActorData, RootComponent.Get());
				}
			}

			if (SpotLightComponent)
			{
				IDatasmithSpotLightElement* SpotLightElement = static_cast<IDatasmithSpotLightElement*>(LightElement);

				SpotLightComponent->InnerConeAngle = SpotLightElement->GetInnerConeAngle();
				SpotLightComponent->OuterConeAngle = SpotLightElement->GetOuterConeAngle();
			}

			LightComponent = SpotLightComponent;
		}
		else if ( LightElement->IsA( EDatasmithElementType::PointLight ) )
		{
			UPointLightComponent* PointLightComponent = Cast<UPointLightComponent>(LightComponent);

			if (PointLightComponent == nullptr)
			{
				if (ImportOptions.BuildHierarchy != EBuildHierarchyMethod::None && !LightElement->IsAComponent())
				{
					APointLight* Actor = CreateActor<APointLight>(RootComponent->GetOwner()->GetWorld());
					PointLightComponent = Cast<UPointLightComponent>(Actor->GetLightComponent());
				}
				else
				{
					PointLightComponent = CreateComponent< UPointLightComponent >(ActorData, RootComponent.Get());
				}
			}

			if (PointLightComponent)
			{
				IDatasmithPointLightElement* PointLightElement = static_cast<IDatasmithPointLightElement*>(LightElement);

				switch ( PointLightElement->GetIntensityUnits() )
				{
				case EDatasmithLightUnits::Candelas:
					PointLightComponent->IntensityUnits = ELightUnits::Candelas;
					break;
				case EDatasmithLightUnits::Lumens:
					PointLightComponent->IntensityUnits = ELightUnits::Lumens;
					break;
				default:
					PointLightComponent->IntensityUnits = ELightUnits::Unitless;
					break;
				}

				if ( PointLightElement->GetSourceRadius() > 0.f )
				{
					PointLightComponent->SourceRadius = PointLightElement->GetSourceRadius();
				}

				if ( PointLightElement->GetSourceLength() > 0.f )
				{
					PointLightComponent->SourceLength = PointLightElement->GetSourceLength();
				}

				if ( PointLightElement->GetAttenuationRadius() > 0.f )
				{
					PointLightComponent->AttenuationRadius = PointLightElement->GetAttenuationRadius();
				}
			}

			LightComponent = PointLightComponent;
		}

		ActorData.Object = LightComponent;

		SetupLightComponent( ActorData, LightElement );

		FinalizeComponent(ActorData);

		ActorData.AddState(EAssetState::Completed);

		return LightComponent ? EActionResult::Succeeded : EActionResult::Failed;
	}

	USceneComponent* CreateComponent(FActorData& ActorData, UClass* Class, USceneComponent* Parent)
	{
		USceneComponent* SceneComponent = ActorData.GetObject<USceneComponent>();

		if (SceneComponent == nullptr)
		{
			SceneComponent = NewObject< USceneComponent >(Parent->GetOwner(), Class, NAME_None);
			if (SceneComponent == nullptr)
			{
				return nullptr;
			}

			SceneComponent->SetMobility(EComponentMobility::Movable);

			SceneComponent->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform);
			SceneComponent->RegisterComponentWithWorld(Parent->GetOwner()->GetWorld());

			ActorData.Object = TWeakObjectPtr<UObject>(SceneComponent);
		}

		if (SceneComponent->GetAttachParent() != Parent)
		{
			SceneComponent->Rename(nullptr, Parent->GetOwner(), REN_NonTransactional | REN_DontCreateRedirectors);
			SceneComponent->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform);
		}

		return SceneComponent;
	}

	EDatasmithAreaLightActorType GetLightActorTypeForLightType( const EDatasmithAreaLightType LightType )
	{
		EDatasmithAreaLightActorType LightActorType = EDatasmithAreaLightActorType::Point;

		switch ( LightType )
		{
		case EDatasmithAreaLightType::Spot:
			LightActorType = EDatasmithAreaLightActorType::Spot;
			break;

		case EDatasmithAreaLightType::Point:
			LightActorType = EDatasmithAreaLightActorType::Point;
			break;

		case EDatasmithAreaLightType::IES_DEPRECATED:
			LightActorType = EDatasmithAreaLightActorType::Point;
			break;

		case EDatasmithAreaLightType::Rect:
			LightActorType = EDatasmithAreaLightActorType::Rect;
			break;
		}

		return LightActorType;
	}

	USceneComponent* ImportAreaLightComponent( FActorData& ActorData, IDatasmithAreaLightElement* AreaLightElement, USceneComponent* Parent )
	{
		FSoftObjectPath LightShapeBlueprintRef = FSoftObjectPath( TEXT("/DatasmithContent/Datasmith/DatasmithArealight.DatasmithArealight") );
		UBlueprint* LightShapeBlueprint = Cast< UBlueprint >( LightShapeBlueprintRef.TryLoad() );

		if ( LightShapeBlueprint )
		{
			UChildActorComponent* ChildActorComponent = ActorData.GetObject<UChildActorComponent>();

			if (ChildActorComponent == nullptr)
			{
				ChildActorComponent = CreateComponent< UChildActorComponent >(ActorData, Parent);

				ChildActorComponent->SetChildActorClass( TSubclassOf< AActor > ( LightShapeBlueprint->GeneratedClass ) );
				ChildActorComponent->CreateChildActor();
			}

			ADatasmithAreaLightActor* LightShapeActor = Cast< ADatasmithAreaLightActor >( ChildActorComponent->GetChildActor() );

			if ( LightShapeActor )
			{
				RenameObject(LightShapeActor, AreaLightElement->GetName());
#if WITH_EDITOR
				LightShapeActor->SetActorLabel( AreaLightElement->GetLabel() );
#endif

				LightShapeActor->UnregisterAllComponents( true );

				LightShapeActor->LightType = GetLightActorTypeForLightType( AreaLightElement->GetLightType() );
				LightShapeActor->LightShape = (EDatasmithAreaLightActorShape)AreaLightElement->GetLightShape();
				LightShapeActor->Dimensions = FVector2D( AreaLightElement->GetLength(), AreaLightElement->GetWidth() );
				LightShapeActor->Color = AreaLightElement->GetColor();
				LightShapeActor->Intensity = AreaLightElement->GetIntensity();
				LightShapeActor->IntensityUnits = (ELightUnits)AreaLightElement->GetIntensityUnits();

				if ( AreaLightElement->GetUseTemperature() )
				{
					LightShapeActor->Temperature = AreaLightElement->GetTemperature();
				}

				if ( AreaLightElement->GetUseIes() )
				{
					LightShapeActor->bUseIESBrightness = AreaLightElement->GetUseIesBrightness();
					LightShapeActor->IESBrightnessScale = AreaLightElement->GetIesBrightnessScale();
					LightShapeActor->Rotation = AreaLightElement->GetIesRotation().Rotator();
				}

				if ( AreaLightElement->GetSourceRadius() > 0.f )
				{
					LightShapeActor->SourceRadius = AreaLightElement->GetSourceRadius();
				}

				if ( AreaLightElement->GetSourceLength() > 0.f )
				{
					LightShapeActor->SourceLength = AreaLightElement->GetSourceLength();
				}

				if ( AreaLightElement->GetAttenuationRadius() > 0.f )
				{
					LightShapeActor->AttenuationRadius = AreaLightElement->GetAttenuationRadius();
				}

				LightShapeActor->RegisterAllComponents();

				LightShapeActor->RerunConstructionScripts();

				return ChildActorComponent;
			}
		}

		return nullptr;
	}

	void SetupLightComponent( FActorData& ActorData, IDatasmithLightActorElement* LightElement )
	{
		if ( ULightComponent* LightComponent = ActorData.GetObject<ULightComponent>() )
		{
			// Light component is using its visibility property to indicate if it is active or not
			LightElement->SetVisibility(LightElement->IsEnabled());

			LightComponent->Intensity = LightElement->GetIntensity();
			LightComponent->CastShadows = true;
			LightComponent->LightColor = LightElement->GetColor().ToFColor( true );
			LightComponent->bUseTemperature = LightElement->GetUseTemperature();
			LightComponent->Temperature = LightElement->GetTemperature();

			// #ue_datasmithruntime: material function not supported yet
			//if ( LightElement->GetLightFunctionMaterial().IsValid() )
			//{
			//	FString BaseName = LightElement->GetLightFunctionMaterial()->GetName();
			//	FString MaterialName = FPaths::Combine( MaterialsFolderPath, BaseName + TEXT(".") + BaseName );
			//	UMaterialInterface* Material = Cast< UMaterialInterface >( FSoftObjectPath( *MaterialName ).TryLoad() );

			//	if ( Material )
			//	{
			//		LightComponent->LightFunctionMaterial = Material;
			//	}
			//}

			if (UPointLightComponent* PointLightComponent = Cast<UPointLightComponent>(LightComponent))
			{
				if ( LightElement->GetUseIes() ) // For IES lights that are not area lights, the IES rotation should be baked into the light transform
				{
					PointLightComponent->bUseIESBrightness = LightElement->GetUseIesBrightness();
					PointLightComponent->IESBrightnessScale = LightElement->GetIesBrightnessScale();

					const FQuat Rotation = LightElement->GetRotation() * LightElement->GetIesRotation();

					ActorData.WorldTransform = FTransform( Rotation, LightElement->GetTranslation(), LightElement->GetScale() );
				}
			}

			LightComponent->UpdateColorAndBrightness();
		}
	}

} // End of namespace DatasmithRuntime
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneVectorSection.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "MovieSceneTracksComponentTypes.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "Tracks/MovieSceneVectorTrack.h"
#include "UObject/SequencerObjectVersion.h"
#include "UObject/StructOnScope.h"

/* FMovieSceneVectorKeyStruct interface
 *****************************************************************************/

#if WITH_EDITOR
struct FVectorSectionEditorData
{
	FVectorSectionEditorData(int32 NumChannels)
	{
		MetaData[0].SetIdentifiers("Vector.X", FCommonChannelData::ChannelX);
		MetaData[0].SortOrder = 0;
		MetaData[0].Color = FCommonChannelData::RedChannelColor;
		MetaData[0].bCanCollapseToTrack = false;

		MetaData[1].SetIdentifiers("Vector.Y", FCommonChannelData::ChannelY);
		MetaData[1].SortOrder = 1;
		MetaData[1].Color = FCommonChannelData::GreenChannelColor;
		MetaData[1].bCanCollapseToTrack = false;

		MetaData[2].SetIdentifiers("Vector.Z", FCommonChannelData::ChannelZ);
		MetaData[2].SortOrder = 2;
		MetaData[2].Color = FCommonChannelData::BlueChannelColor;
		MetaData[2].bCanCollapseToTrack = false;

		MetaData[3].SetIdentifiers("Vector.W", FCommonChannelData::ChannelW);
		MetaData[3].SortOrder = 3;
		MetaData[3].bCanCollapseToTrack = false;

		ExternalValues[0].OnGetExternalValue = [NumChannels](UObject& InObject, FTrackInstancePropertyBindings* Bindings) { return ExtractChannelX(InObject, Bindings, NumChannels); };
		ExternalValues[1].OnGetExternalValue = [NumChannels](UObject& InObject, FTrackInstancePropertyBindings* Bindings) { return ExtractChannelY(InObject, Bindings, NumChannels); };
		ExternalValues[2].OnGetExternalValue = [NumChannels](UObject& InObject, FTrackInstancePropertyBindings* Bindings) { return ExtractChannelZ(InObject, Bindings, NumChannels); };
		ExternalValues[3].OnGetExternalValue = [NumChannels](UObject& InObject, FTrackInstancePropertyBindings* Bindings) { return ExtractChannelW(InObject, Bindings, NumChannels); };
	}

	static FVector4 GetPropertyValue(UObject& InObject, FTrackInstancePropertyBindings& Bindings, int32 NumChannels)
	{
		if (NumChannels == 2)
		{
			FVector2D Vector = Bindings.GetCurrentValue<FVector2D>(InObject);
			return FVector4(Vector.X, Vector.Y, 0.f, 0.f);
		}
		else if (NumChannels == 3)
		{
			FVector Vector = Bindings.GetCurrentValue<FVector>(InObject);
			return FVector4(Vector.X, Vector.Y, Vector.Z, 0.f);
		}
		else
		{
			return Bindings.GetCurrentValue<FVector4>(InObject);
		}
	}
	
	static TOptional<float> ExtractChannelX(UObject& InObject, FTrackInstancePropertyBindings* Bindings, int32 NumChannels)
	{
		return Bindings ? GetPropertyValue(InObject, *Bindings, NumChannels).X : TOptional<float>();
	}
	static TOptional<float> ExtractChannelY(UObject& InObject, FTrackInstancePropertyBindings* Bindings, int32 NumChannels)
	{
		return Bindings ? GetPropertyValue(InObject, *Bindings, NumChannels).Y : TOptional<float>();
	}
	static TOptional<float> ExtractChannelZ(UObject& InObject, FTrackInstancePropertyBindings* Bindings, int32 NumChannels)
	{
		return Bindings ? GetPropertyValue(InObject, *Bindings, NumChannels).Z : TOptional<float>();
	}
	static TOptional<float> ExtractChannelW(UObject& InObject, FTrackInstancePropertyBindings* Bindings, int32 NumChannels)
	{
		return Bindings ? GetPropertyValue(InObject, *Bindings, NumChannels).W : TOptional<float>();
	}

	FMovieSceneChannelMetaData      MetaData[4];
	TMovieSceneExternalValue<float> ExternalValues[4];

};
#endif

void FMovieSceneVectorKeyStructBase::PropagateChanges(const FPropertyChangedEvent& ChangeEvent)
{
	KeyStructInterop.Apply(Time);
}


/* UMovieSceneVectorSection structors
 *****************************************************************************/

UMovieSceneVectorSection::UMovieSceneVectorSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ChannelsUsed = 0;
	bSupportsInfiniteRange = true;

	EvalOptions.EnableAndSetCompletionMode
		(GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToRestoreState ? 
			EMovieSceneCompletionMode::KeepState : 
			GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToProjectDefault ? 
			EMovieSceneCompletionMode::RestoreState : 
			EMovieSceneCompletionMode::ProjectDefault);
	BlendType = EMovieSceneBlendType::Absolute;
}

void UMovieSceneVectorSection::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		RecreateChannelProxy();
	}
}

void UMovieSceneVectorSection::PostEditImport()
{
	Super::PostEditImport();

	RecreateChannelProxy();
}

void UMovieSceneVectorSection::RecreateChannelProxy()
{
	FMovieSceneChannelProxyData Channels;

	check(ChannelsUsed <= UE_ARRAY_COUNT(Curves));

#if WITH_EDITOR

	const FVectorSectionEditorData EditorData(ChannelsUsed);
	for (int32 Index = 0; Index < ChannelsUsed; ++Index)
	{
		Channels.Add(Curves[Index], EditorData.MetaData[Index], EditorData.ExternalValues[Index]);
	}

#else

	for (int32 Index = 0; Index < ChannelsUsed; ++Index)
	{
		Channels.Add(Curves[Index]);
	}

#endif

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(MoveTemp(Channels));
}

TSharedPtr<FStructOnScope> UMovieSceneVectorSection::GetKeyStruct(TArrayView<const FKeyHandle> KeyHandles)
{
	TSharedPtr<FStructOnScope> KeyStruct;
	if (ChannelsUsed == 2)
	{
		KeyStruct = MakeShareable(new FStructOnScope(FMovieSceneVector2DKeyStruct::StaticStruct()));
	}
	else if (ChannelsUsed == 3)
	{
		KeyStruct = MakeShareable(new FStructOnScope(FMovieSceneVectorKeyStruct::StaticStruct()));
	}
	else if (ChannelsUsed == 4)
	{
		KeyStruct = MakeShareable(new FStructOnScope(FMovieSceneVector4KeyStruct::StaticStruct()));
	}

	if (KeyStruct.IsValid())
	{
		FMovieSceneVectorKeyStructBase* Struct = (FMovieSceneVectorKeyStructBase*)KeyStruct->GetStructMemory();
		for (int32 Index = 0; Index < ChannelsUsed; ++Index)
		{
			Struct->KeyStructInterop.Add(FMovieSceneChannelValueHelper(ChannelProxy->MakeHandle<FMovieSceneFloatChannel>(Index), Struct->GetPropertyChannelByIndex(Index), KeyHandles));
		}

		Struct->KeyStructInterop.SetStartingValues();
		Struct->Time = Struct->KeyStructInterop.GetUnifiedKeyTime().Get(0);
	}

	return KeyStruct;
}

bool UMovieSceneVectorSection::PopulateEvaluationFieldImpl(const TRange<FFrameNumber>& EffectiveRange, const FMovieSceneEvaluationFieldEntityMetaData& InMetaData, FMovieSceneEntityComponentFieldBuilder* OutFieldBuilder)
{
	FMovieScenePropertyTrackEntityImportHelper::PopulateEvaluationField(*this, EffectiveRange, InMetaData, OutFieldBuilder);
	return true;
}

void UMovieSceneVectorSection::ImportEntityImpl(UMovieSceneEntitySystemLinker* EntityLinker, const FEntityImportParams& Params, FImportedEntity* OutImportedEntity)
{
	using namespace UE::MovieScene;

	if (ChannelsUsed <= 0)
	{
		return;
	}
	else if (!Curves[0].HasAnyData() && !Curves[1].HasAnyData() && !Curves[2].HasAnyData() && !Curves[3].HasAnyData())
	{
		return;
	}

	const FBuiltInComponentTypes* Components = FBuiltInComponentTypes::Get();
	const FMovieSceneTracksComponentTypes* TracksComponents = FMovieSceneTracksComponentTypes::Get();

	FPropertyTrackEntityImportHelper(TracksComponents->Vector)
		.AddConditional(Components->FloatChannel[0], &Curves[0], ChannelsUsed > 0 && Curves[0].HasAnyData())
		.AddConditional(Components->FloatChannel[1], &Curves[1], ChannelsUsed > 1 && Curves[1].HasAnyData())
		.AddConditional(Components->FloatChannel[2], &Curves[2], ChannelsUsed > 2 && Curves[2].HasAnyData())
		.AddConditional(Components->FloatChannel[3], &Curves[3], ChannelsUsed > 3 && Curves[3].HasAnyData())
		.Commit(this, Params, OutImportedEntity);
}

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "RHI.h"
#include "RHIDefinitions.h"
#include "RHIResources.h"
#include "RHICommandList.h"

class FRHICommandListImmediate;
struct FDisplayClusterConfigurationProjection;

#if WITH_EDITORONLY_DATA
class UDisplayClusterConfigurationViewport;
class FTextureRenderTargetResource;
class IDetailCategoryBuilder;
class IDetailLayoutBuilder;
class UMeshComponent;
#endif

/**
 * nDisplay projection policy
 */
class IDisplayClusterProjectionPolicy
{
public:
	virtual ~IDisplayClusterProjectionPolicy() = default;

public:
	/**
	* Return projection policy name
	*/
	virtual const FString& GetId() const = 0;

	/**
	* Return projection policy type
	*/
	virtual const FString GetTypeId() const = 0;

	/**
	* Return projection policy configuration
	*/
	virtual const TMap<FString, FString>& GetParameters() const = 0;

	/**
	* Send projection policy game thread data to render thread proxy
	* called once per frame from FDisplayClusterViewportManager::FinalizeNewFrame
	*/
	virtual void UpdateProxyData(class IDisplayClusterViewport* InViewport)
	{ }

	/**
	* Called each time a new game level starts
	*
	* @param InViewport - a owner viewport
	*/
	virtual bool HandleStartScene(class IDisplayClusterViewport* InViewport) = 0;

	/**
	* Called when current level is going to be closed (i.e. before loading a new map)
	*
	* @param InViewport - a owner viewport
	*/
	virtual void HandleEndScene(class IDisplayClusterViewport* InViewport) = 0;

	// Handle request for additional render targetable resource inside viewport api for projection policy
	virtual bool ShouldUseAdditionalTargetableResource() const
	{ 
		return false; 
	}

	/**
	* Returns true if the policy supports input mip-textures.
	* Use a mip texture for smoother deformation on curved surfaces.
	*
	* @return - true, if mip-texture is supported by the policy implementation
	*/
	virtual bool ShouldUseSourceTextureWithMips() const
	{
		return false;
	}

	// This policy can support ICVFX rendering
	virtual bool ShouldSupportICVFX() const
	{
		return false;
	}

	// Return true, if camera projection visible for this viewport geometry
	// ICVFX Perforamance : if camera frame not visible on this node, disable render for this camera
	virtual bool IsCameraProjectionVisible(const FRotator& InViewRotation, const FVector& InViewLocation, const FMatrix& InProjectionMatrix)
	{
		return true;
	}

	/**
	* Check projection policy settings changes
	*
	* @param InConfigurationProjectionPolicy - new settings
	*
	* @return - True if found changes
	*/
	virtual bool IsConfigurationChanged(const struct FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy) const = 0;

	/**
	* @param ViewIdx           - Index of view that is being processed for this viewport
	* @param InOutViewLocation - (in/out) View location with ViewOffset (i.e. left eye pre-computed location)
	* @param InOutViewRotation - (in/out) View rotation
	* @param ViewOffset        - Offset applied ot a camera location that gives us InOutViewLocation (i.e. right offset in world to compute right eye location)
	* @param WorldToMeters     - Current world scale (units (cm) in meter)
	* @param NCP               - Distance to the near clipping plane
	* @param FCP               - Distance to the far  clipping plane
	*
	* @return - True if success
	*/
	virtual bool CalculateView(class IDisplayClusterViewport* InViewport, const uint32 InContextNum, FVector& InOutViewLocation, FRotator& InOutViewRotation, const FVector& ViewOffset, const float WorldToMeters, const float NCP, const float FCP) = 0;

	/**
	* @param ViewIdx      - Index of view that is being processed for this viewport
	* @param OutPrjMatrix - (out) projection matrix
	*
	* @return - True if success
	*/
	virtual bool GetProjectionMatrix(class IDisplayClusterViewport* InViewport, const uint32 InContextNum, FMatrix& OutPrjMatrix) = 0;

	/**
	* Returns if a policy provides warp&blend feature
	*
	* @return - True if warp&blend operations are supported by the policy implementation
	*/
	virtual bool IsWarpBlendSupported()
	{
		return false;
	}

	/**
	* Initializing the projection policy logic for the current frame before applying warp blending. Called if IsWarpBlendSupported() returns true
	*
	* @param RHICmdList      - RHI commands
	* @param InViewportProxy - viewport proxy
	*
	*/
	virtual void BeginWarpBlend_RenderThread(FRHICommandListImmediate& RHICmdList, const class IDisplayClusterViewportProxy* InViewportProxy)
	{ }

	/**
	* Performs warp&blend. Called if IsWarpBlendSupported() returns true
	*
	* @param RHICmdList      - RHI commands
	* @param InViewportProxy - viewport proxy
	*
	*/
	virtual void ApplyWarpBlend_RenderThread(FRHICommandListImmediate& RHICmdList, const class IDisplayClusterViewportProxy* InViewportProxy)
	{ }

	/**
	* Completing the projection policy logic for the current frame after applying warp blending. Called if IsWarpBlendSupported() returns true
	*
	* @param ViewIdx      - Index of view that is being processed for this viewport
	* @param RHICmdList   - RHI commands
	* @param SrcTexture   - Source texture
	* @param ViewportRect - Region of the SrcTexture to perform warp&blend operations
	*
	*/
	virtual void EndWarpBlend_RenderThread(FRHICommandListImmediate& RHICmdList, const class IDisplayClusterViewportProxy* InViewportProxy)
	{ }

#if WITH_EDITOR
	/**
	* Ask projection policy instance if it has any mesh based preview
	*
	* @return - True if mesh based preview is available
	*/
	virtual bool HasPreviewMesh()
	{
		return false;
	}

	/**
	* Build preview mesh
	*
	* @param InViewport - Projection specific parameters.
	* @param bOutIsRootActorComponent - return true, if used custom root actor component. return false, if created unique temporary component
	*/
	virtual class UMeshComponent* GetOrCreatePreviewMeshComponent(class IDisplayClusterViewport* InViewport, bool& bOutIsRootActorComponent)
	{
		return nullptr;
	}
#endif
};

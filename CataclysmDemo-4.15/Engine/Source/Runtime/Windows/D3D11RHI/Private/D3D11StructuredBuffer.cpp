// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "D3D11RHIPrivate.h"

FStructuredBufferRHIRef FD3D11DynamicRHI::RHICreateStructuredBuffer(uint32 Stride,uint32 Size,uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	// Explicitly check that the size is nonzero before allowing CreateStructuredBuffer to opaquely fail.
	check(Size > 0);
	// Check for values that will cause D3D calls to fail
	check(Size / Stride > 0 && Size % Stride == 0);

	D3D11_BUFFER_DESC Desc;
	ZeroMemory( &Desc, sizeof( D3D11_BUFFER_DESC ) );
	Desc.ByteWidth = Size;
	Desc.Usage = (InUsage & BUF_AnyDynamic) ? D3D11_USAGE_DYNAMIC : ((InUsage & BUF_Staging) ? D3D11_USAGE_STAGING : D3D11_USAGE_DEFAULT);// CATACLYSM Extend RHI
	Desc.BindFlags = 0;

	if(InUsage & BUF_ShaderResource)
	{
		// Setup bind flags so we can create a view to read from the buffer in a shader.
		Desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
	}

	if (InUsage & BUF_UnorderedAccess)
	{
		// Setup bind flags so we can create a writeable UAV to the buffer
		Desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
	}

	if (InUsage & BUF_StreamOutput)
	{
		Desc.BindFlags |= D3D11_BIND_STREAM_OUTPUT;
	}

	Desc.CPUAccessFlags = (InUsage & BUF_AnyDynamic) ? D3D11_CPU_ACCESS_WRITE : ((InUsage & BUF_Staging) ? D3D11_CPU_ACCESS_READ : 0);// CATACLYSM Extend RHI
	Desc.MiscFlags = 0;

	if (InUsage & BUF_DrawIndirect)
	{
		Desc.MiscFlags |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
	}
	else
	{
		if (InUsage & BUF_ByteAddressBuffer)
		{
			Desc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
		}
		else
		{
			// BEGIN CATACLYSM Extend RHI
			if (!(InUsage & BUF_Staging))
			{
				Desc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
			}
			// END CATACLYSM Extend RHI
		}
	}

	Desc.StructureByteStride = Stride;

	if (FPlatformProperties::SupportsFastVRAMMemory())
	{
		if (InUsage & BUF_FastVRAM)
		{
			FFastVRAMAllocator::GetFastVRAMAllocator()->AllocUAVBuffer(Desc);
		}
	}

	// If a resource array was provided for the resource, create the resource pre-populated
	D3D11_SUBRESOURCE_DATA InitData;
	D3D11_SUBRESOURCE_DATA* pInitData = NULL;
	if(CreateInfo.ResourceArray)
	{
		check(Size == CreateInfo.ResourceArray->GetResourceDataSize());
		InitData.pSysMem = CreateInfo.ResourceArray->GetResourceData();
		InitData.SysMemPitch = Size;
		InitData.SysMemSlicePitch = 0;
		pInitData = &InitData;
	}

	TRefCountPtr<ID3D11Buffer> StructuredBufferResource;
	VERIFYD3D11RESULT_EX(Direct3DDevice->CreateBuffer(&Desc,pInitData,StructuredBufferResource.GetInitReference()), Direct3DDevice);

	UpdateBufferStats(StructuredBufferResource, true);

	if(CreateInfo.ResourceArray)
	{
		// Discard the resource array's contents.
		CreateInfo.ResourceArray->Discard();
	}

	return new FD3D11StructuredBuffer(StructuredBufferResource,Stride,Size,InUsage);
}

void* FD3D11DynamicRHI::RHILockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI,uint32 Offset,uint32 Size,EResourceLockMode LockMode)
{
	FD3D11StructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);
	
	// If this resource is bound to the device, unbind it
	ConditionalClearShaderResource(StructuredBuffer);

	// Determine whether the Structured buffer is dynamic or not.
	D3D11_BUFFER_DESC Desc;
	StructuredBuffer->Resource->GetDesc(&Desc);
	const bool bIsDynamic = (Desc.Usage == D3D11_USAGE_DYNAMIC);
	const bool bIsStaging = (Desc.Usage == D3D11_USAGE_STAGING);// CATACLYSM Extend RHI

	FD3D11LockedKey LockedKey(StructuredBuffer->Resource);
	FD3D11LockedData LockedData;

	if(bIsDynamic)
	{
		check(LockMode == RLM_WriteOnly);

		// If the buffer is dynamic, map its memory for writing.
		D3D11_MAPPED_SUBRESOURCE MappedSubresource;
		VERIFYD3D11RESULT_EX(Direct3DDeviceIMContext->Map(StructuredBuffer->Resource,0,D3D11_MAP_WRITE_DISCARD,0,&MappedSubresource), Direct3DDevice);
		LockedData.SetData(MappedSubresource.pData);
		LockedData.Pitch = MappedSubresource.RowPitch;
	}
	else
	{
		if(LockMode == RLM_ReadOnly)
		{
			// BEGIN CATACLYSM Extend RHI
			if (bIsStaging)
			{
				D3D11_MAPPED_SUBRESOURCE MappedSubresource;
				VERIFYD3D11RESULT(Direct3DDeviceIMContext->Map(StructuredBuffer->Resource,0,D3D11_MAP_READ,0,&MappedSubresource));
				LockedData.SetData(MappedSubresource.pData);
				LockedData.Pitch = MappedSubresource.RowPitch;
			}
			else
			{
				// If the static buffer is being locked for reading, create a staging buffer.
				D3D11_BUFFER_DESC StagingBufferDesc;
				ZeroMemory( &StagingBufferDesc, sizeof( D3D11_BUFFER_DESC ) );
				StagingBufferDesc.ByteWidth = Size;
				StagingBufferDesc.Usage = D3D11_USAGE_STAGING;
				StagingBufferDesc.BindFlags = 0;
				StagingBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
				StagingBufferDesc.MiscFlags = 0;
				TRefCountPtr<ID3D11Buffer> StagingStructuredBuffer;
				VERIFYD3D11RESULT_EX(Direct3DDevice->CreateBuffer(&StagingBufferDesc,NULL,StagingStructuredBuffer.GetInitReference()), Direct3DDevice);
				LockedData.StagingResource = StagingStructuredBuffer;

				// Copy the contents of the Structured buffer to the staging buffer.
				Direct3DDeviceIMContext->CopyResource(StagingStructuredBuffer,StructuredBuffer->Resource);

				// Map the staging buffer's memory for reading.
				D3D11_MAPPED_SUBRESOURCE MappedSubresource;
				VERIFYD3D11RESULT_EX(Direct3DDeviceIMContext->Map(StagingStructuredBuffer,0,D3D11_MAP_READ,0,&MappedSubresource), Direct3DDevice);
				LockedData.SetData(MappedSubresource.pData);
				LockedData.Pitch = MappedSubresource.RowPitch;
			}
			// END CATACLYSM Extend RHI
		}
		else
		{
			// If the static buffer is being locked for writing, allocate memory for the contents to be written to.
			LockedData.AllocData(Desc.ByteWidth);
			LockedData.Pitch = Desc.ByteWidth;
		}
	}

	// Add the lock to the lock map.
	OutstandingLocks.Add(LockedKey,LockedData);

	// Return the offset pointer
	return (void*)((uint8*)LockedData.GetData() + Offset);
}

void FD3D11DynamicRHI::RHIUnlockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI)
{
	FD3D11StructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);

	// Determine whether the Structured buffer is dynamic or not.
	D3D11_BUFFER_DESC Desc;
	StructuredBuffer->Resource->GetDesc(&Desc);
	const bool bIsDynamic = (Desc.Usage == D3D11_USAGE_DYNAMIC);
	const bool bIsStaging = (Desc.Usage == D3D11_USAGE_STAGING);// CATACLYSM Extend RHI

	// Find the outstanding lock for this VB.
	FD3D11LockedKey LockedKey(StructuredBuffer->Resource);
	FD3D11LockedData* LockedData = OutstandingLocks.Find(LockedKey);
	check(LockedData);

	if(bIsDynamic)
	{
		// If the VB is dynamic, its memory was mapped directly; unmap it.
		Direct3DDeviceIMContext->Unmap(StructuredBuffer->Resource,0);
	}
	else
	{
		// BEGIN CATACLYSM Extend RHI
		if(bIsStaging)
		{
			Direct3DDeviceIMContext->Unmap(StructuredBuffer->Resource,0);
		}
		else
		{
			// If the static VB lock involved a staging resource, it was locked for reading.
			if(LockedData->StagingResource)
			{
				// Unmap the staging buffer's memory.
				ID3D11Buffer* StagingBuffer = (ID3D11Buffer*)LockedData->StagingResource.GetReference();
				Direct3DDeviceIMContext->Unmap(StagingBuffer,0);
			}
			else 
			{
				// Copy the contents of the temporary memory buffer allocated for writing into the VB.
				Direct3DDeviceIMContext->UpdateSubresource(StructuredBuffer->Resource,LockedKey.Subresource,NULL,LockedData->GetData(),LockedData->Pitch,0);

				// Check the copy is finished before freeing...
				Direct3DDeviceIMContext->Flush();

				// Free the temporary memory buffer.
				LockedData->FreeData();
			}
		}
		// END CATACLYSM Extend RHI
	}

	// Remove the FD3D11LockedData from the lock map.
	// If the lock involved a staging resource, this releases it.
	OutstandingLocks.Remove(LockedKey);
}
// BEGIN CATACLYSM Extend RHI
void FD3D11DynamicRHI::RHIUpdateStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI, const FUpdateTextureRegion2D& UpdateRegion,uint32 SourcePitch,const uint8* SourceData)
{
    FD3D11StructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);

    D3D11_BOX DestBox =
	{
		UpdateRegion.DestX,                      UpdateRegion.DestY,                       0,
        UpdateRegion.DestX + UpdateRegion.Width, UpdateRegion.DestY + UpdateRegion.Height, 1
	};

	Direct3DDeviceIMContext->UpdateSubresource(StructuredBuffer->Resource, 0, &DestBox, SourceData, SourcePitch, 0);
}

void FD3D11DynamicRHI::RHICopyStructuredBuffer(FStructuredBufferRHIParamRef SourceBufferRHI, FStructuredBufferRHIParamRef DestBufferRHI)
{
	FD3D11StructuredBuffer* SourceBuffer = ResourceCast(SourceBufferRHI);
	FD3D11StructuredBuffer* DestBuffer = ResourceCast(DestBufferRHI);

	D3D11_BUFFER_DESC SourceBufferDesc;
	SourceBuffer->Resource->GetDesc(&SourceBufferDesc);
	
	D3D11_BUFFER_DESC DestBufferDesc;
	DestBuffer->Resource->GetDesc(&DestBufferDesc);

	check(SourceBufferDesc.ByteWidth == DestBufferDesc.ByteWidth);

	Direct3DDeviceIMContext->CopyResource(DestBuffer->Resource,SourceBuffer->Resource);

	GPUProfilingData.RegisterGPUWork(1);
}

void FD3D11DynamicRHI::RHICopyStructureCount(FUnorderedAccessViewRHIParamRef SourceUAVRHI, FStructuredBufferRHIParamRef DestBufferRHI)
{
	FD3D11UnorderedAccessView* SourceUAV = ResourceCast(SourceUAVRHI);
	FD3D11StructuredBuffer* DestBuffer = ResourceCast(DestBufferRHI);

	Direct3DDeviceIMContext->CopyStructureCount(DestBuffer->Resource, 0, SourceUAV->View);

	GPUProfilingData.RegisterGPUWork(1);
}
// END CATACLYSM Extend RHI

#pragma once

enum class ERHIResourceType
{
	RHI_NoneType,
	RHI_BufferType,
	RHI_IndexBufferType,
	RHI_TextureType,
};

class RHIResource {
public:
	RHIResource(ERHIResourceType Type):ResourceType(Type)
	{
	};
	virtual ~RHIResource()
	{
	};
private:
	ERHIResourceType ResourceType;
};
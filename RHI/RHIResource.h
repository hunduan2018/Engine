
#pragma once

class RHIResource {
public:
	RHIResource()
	{
	};
	virtual ~RHIResource()
	{
	};
	// Prevent copying
	RHIResource(const RHIResource&) = delete;
	RHIResource& operator=(const RHIResource&) = delete;
	// Allow moving
	RHIResource(RHIResource&&) = default;
	RHIResource& operator=(RHIResource&&) = default;
	// Virtual method to release the resource
	virtual void Release() = 0;

	// Create RHIResource
	virtual RHIResource* Create() = 0;
};
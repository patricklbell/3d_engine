#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include "Jolt/Physics/Body/BodyCreationSettings.h"

#include "physics.hpp"
#include "editor.hpp"
#include "entities.hpp"
#include "level.hpp"

#include <iostream>
#include <thread>

using namespace JPH;
using namespace JPH::literals;
JPH_SUPPRESS_WARNINGS

namespace physics {
    TempAllocatorImpl* temp_allocator = nullptr;
    JobSystemThreadPool* job_system = nullptr;
    PhysicsSystem* system = nullptr;
	BPLayerInterfaceImpl broad_phase_layer_interface;
	ObjectVsBroadPhaseLayerFilterImpl object_vs_broadphase_layer_filter;
	ObjectLayerPairFilterImpl object_vs_object_layer_filter;

}
using namespace physics;

// Callback for traces, connect this to your own trace function if you have one
static void TraceImpl(const char *inFMT, ...)
{ 
	// Format the message
	va_list list;
	va_start(list, inFMT);
	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), inFMT, list);
	va_end(list);

    pushInfoMessage(buffer);
}

#ifdef JPH_ENABLE_ASSERTS

// Callback for asserts, connect this to your own assert handler if you have one
static bool AssertFailedImpl(const char *inExpression, const char *inMessage, const char *inFile, uint inLine)
{ 
	// Print to the TTY
    pushInfoMessage(std::string(inFile) + ":" + std::to_string(inLine) + ": (" + inExpression + ") " + (inMessage != nullptr? inMessage : ""));

	// Breakpoint
	return true;
};

#endif // JPH_ENABLE_ASSERTS

void initPhysics() {
    // Register allocation hook
    RegisterDefaultAllocator();

    // Install callbacks
    Trace = TraceImpl;
    JPH_IF_ENABLE_ASSERTS(AssertFailed = AssertFailedImpl;)

    // Create a factory
    Factory::sInstance = new Factory();

    // Register all Jolt physics types
    RegisterTypes();


    temp_allocator = new TempAllocatorImpl(10 * 1024 * 1024);

    // We need a job system that will execute physics jobs on multiple threads. Typically
    // you would implement the JobSystem interface yourself and let Jolt Physics run on top
    // of your own job scheduler. JobSystemThreadPool is an example implementation.
    job_system = new JobSystemThreadPool(cMaxPhysicsJobs, cMaxPhysicsBarriers, thread::hardware_concurrency() - 1);
}

void cleanupPhysics() {
    // Unregisters all types with the factory and cleans up the default material
    UnregisterTypes();

    // Destroy the factory
    delete Factory::sInstance;
    Factory::sInstance = nullptr;

    delete physics::system;
    delete job_system;
    delete temp_allocator;
}

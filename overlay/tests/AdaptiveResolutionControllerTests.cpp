#include "../src/Features/Upscaling/AdaptiveResolutionController.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

namespace
{
	Adaptive80::Settings BalancedSettings()
	{
		return {
			.targetFrameTimeMs = 25.0f,
			.minScale = 0.52f,
			.maxScale = 0.70f,
			.emergencyMinScale = 0.44f,
			.attackScalePerSecond = 0.80f,
			.recoveryScalePerSecond = 0.04f,
			.gpuHeadroom = 0.90f,
			.cpuGuard = true
		};
	}

	Adaptive80::Output Step(
		Adaptive80::Controller& controller,
		const Adaptive80::Settings& settings,
		float frameTimeMs,
		float gpuTimeMs,
		bool gpuTimeValid = true)
	{
		return controller.Update(settings, {
			.frameTimeMs = frameTimeMs,
			.gpuTimeMs = gpuTimeMs,
			.gpuReferenceFrameTimeMs = frameTimeMs,
			.deltaSeconds = std::clamp(frameTimeMs / 1000.0f, 0.002f, 0.10f),
			.gpuTimeValid = gpuTimeValid,
			.paused = false
		});
	}
}

int main()
{
	const auto settings = BalancedSettings();

	// The 22-27 ms target band must not oscillate the resolution.
	{
		Adaptive80::Controller controller;
		controller.Reset(0.60f);
		for (int i = 0; i < 120; ++i)
			Step(controller, settings, 25.0f, 21.0f);
		const auto output = controller.GetOutput();
		assert(std::abs(output.scale - 0.60f) < 0.001f);
		assert(output.state == Adaptive80::State::Target);
	}

	// A GPU-bound 35 ms workload should reduce scale and return near the target.
	{
		Adaptive80::Controller controller;
		controller.Reset(0.70f);
		float frameTimeMs = 35.0f;
		for (int i = 0; i < 180; ++i) {
			const float scale = controller.GetOutput().scale;
			const float gpuTimeMs = 35.0f * (scale * scale) / (0.70f * 0.70f);
			frameTimeMs = std::max(10.0f, gpuTimeMs);
			Step(controller, settings, frameTimeMs, gpuTimeMs);
		}
		const auto output = controller.GetOutput();
		assert(output.boundState == Adaptive80::BoundState::Gpu);
		assert(output.scale < 0.69f);
		assert(output.scale >= settings.minScale - 0.001f);
		assert(frameTimeMs <= 28.0f);
	}

	// A CPU-bound 50 ms workload must preserve quality instead of collapsing scale.
	{
		Adaptive80::Controller controller;
		controller.Reset(0.70f);
		for (int i = 0; i < 90; ++i)
			Step(controller, settings, 50.0f, 12.0f);
		const auto output = controller.GetOutput();
		assert(output.boundState == Adaptive80::BoundState::Cpu || output.cpuGuardActive);
		assert(output.scale >= 0.69f);
	}

	// The response probe also protects quality when GPU timing is temporarily unavailable.
	{
		Adaptive80::Controller controller;
		controller.Reset(0.70f);
		for (int i = 0; i < 90; ++i)
			Step(controller, settings, 50.0f, 0.0f, false);
		const auto output = controller.GetOutput();
		assert(output.boundState == Adaptive80::BoundState::Cpu || output.cpuGuardActive);
		assert(output.scale >= 0.68f);
	}

	// A misleading high GPU timestamp must not defeat the response probe. If a
	// scale reduction produces no frametime improvement, CPU Guard restores quality.
	{
		Adaptive80::Controller controller;
		controller.Reset(0.70f);
		for (int i = 0; i < 90; ++i)
			Step(controller, settings, 50.0f, 45.0f);
		const auto output = controller.GetOutput();
		assert(output.boundState == Adaptive80::BoundState::Cpu || output.cpuGuardActive);
		assert(output.scale >= 0.68f);
	}

	// A severe GPU drop may enter emergency scale, then must recover quality slowly.
	{
		Adaptive80::Controller controller;
		controller.Reset(0.70f);
		float frameTimeMs = 100.0f;
		for (int i = 0; i < 120; ++i) {
			const float scale = controller.GetOutput().scale;
			const float gpuTimeMs = 100.0f * (scale * scale) / (0.70f * 0.70f);
			frameTimeMs = std::max(8.0f, gpuTimeMs);
			Step(controller, settings, frameTimeMs, gpuTimeMs);
		}
		const float rescuedScale = controller.GetOutput().scale;
		assert(rescuedScale <= settings.minScale + 0.001f);
		assert(rescuedScale >= settings.emergencyMinScale - 0.001f);
		assert(frameTimeMs < 65.0f);

		for (int i = 0; i < 650; ++i) {
			const float scale = controller.GetOutput().scale;
			const float gpuTimeMs = 14.0f * (scale * scale) / (0.70f * 0.70f);
			Step(controller, settings, std::max(8.0f, gpuTimeMs), gpuTimeMs);
		}
		assert(controller.GetOutput().scale > 0.68f);
	}

	std::cout << "AdaptiveResolutionController tests passed\n";
	return 0;
}

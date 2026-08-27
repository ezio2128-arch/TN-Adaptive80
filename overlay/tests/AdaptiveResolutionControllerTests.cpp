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
			.preferredFrameTimeMs = 1000.0f / 48.0f,
			.minScale = 0.52f,
			.maxScale = 0.70f,
			.emergencyMinScale = 0.44f,
			.attackScalePerSecond = 0.80f,
			.recoveryScalePerSecond = 0.04f,
			.gpuHeadroom = 0.90f,
			.resolutionStep = 0.04f,
			.holdSeconds = 0.50f,
			.targetHoldSeconds = 0.90f,
			.pressureQualificationSeconds = 0.45f,
			.cpuGuard = true
		};
	}

	Adaptive80::Output Step(
		Adaptive80::Controller& controller,
		const Adaptive80::Settings& settings,
		float frameTimeMs,
		float gpuTimeMs,
		bool gpuTimeValid = true,
		float gpuReferenceFrameTimeMs = -1.0f)
	{
		if (gpuReferenceFrameTimeMs <= 0.0f)
			gpuReferenceFrameTimeMs = frameTimeMs;
		return controller.Update(settings, {
			.frameTimeMs = frameTimeMs,
			.gpuTimeMs = gpuTimeMs,
			.gpuReferenceFrameTimeMs = gpuReferenceFrameTimeMs,
			.deltaSeconds = std::clamp(frameTimeMs / 1000.0f, 0.002f, 0.10f),
			.gpuTimeValid = gpuTimeValid,
			.paused = false
		});
	}
}

int main()
{
	const auto settings = BalancedSettings();

	// 40 FPS is a floor, not a ceiling: 42-48 FPS should keep performance reserve
	// instead of immediately spending it on quality recovery.
	{
		Adaptive80::Controller controller;
		controller.Reset(0.58f);
		int changes = 0;
		for (int i = 0; i < 240; ++i) {
			auto out = Step(controller, settings, 22.5f, 19.5f);
			changes += out.scaleChanged ? 1 : 0;
		}
		assert(changes == 0);
		assert(std::abs(controller.GetOutput().scale - 0.58f) < 0.001f);
	}

	// Genuine excess performance (>~50 FPS) may recover quality, slowly.
	{
		Adaptive80::Controller controller;
		controller.Reset(0.54f);
		for (int i = 0; i < 500; ++i)
			Step(controller, settings, 16.5f, 14.0f);
		assert(controller.GetOutput().scale > 0.58f);
	}

	// Camera/streaming transient observed by the user: pre-FG ~44 ms, GPU ~18 ms.
	// Must classify CPU/unknown and must not reduce render scale.
	{
		Adaptive80::Controller controller;
		controller.Reset(0.60f);
		for (int i = 0; i < 80; ++i)
			Step(controller, settings, 44.0f, 18.0f);
		const auto out = controller.GetOutput();
		assert(out.boundState == Adaptive80::BoundState::Cpu || out.cpuGuardActive);
		assert(out.scale >= 0.599f);
	}

	// Delayed GPU timestamp from an earlier fast frame must not promote GPU-bound
	// during a sudden current 44 ms camera spike.
	{
		Adaptive80::Controller controller;
		controller.Reset(0.60f);
		for (int i = 0; i < 12; ++i)
			Step(controller, settings, 20.0f, 18.0f, true, 20.0f);
		for (int i = 0; i < 18; ++i)
			Step(controller, settings, 44.0f, 18.0f, true, 20.0f);
		const auto out = controller.GetOutput();
		assert(out.boundState != Adaptive80::BoundState::Gpu);
		assert(out.scale >= 0.599f);
	}

	// Short GPU spike must not resize until pressure is sustained for the new
	// qualification interval.
	{
		Adaptive80::Controller controller;
		controller.Reset(0.70f);
		int changes = 0;
		for (int i = 0; i < 8; ++i) { // ~280 ms at 35 ms/frame, below 450 ms qualification
			auto out = Step(controller, settings, 35.0f, 33.0f);
			changes += out.scaleChanged ? 1 : 0;
		}
		assert(changes == 0);
	}

	// Sustained true GPU bottleneck eventually qualifies discrete resize events.
	{
		Adaptive80::Controller controller;
		controller.Reset(0.70f);
		int changes = 0;
		for (int i = 0; i < 240; ++i) {
			const float scale = controller.GetOutput().scale;
			const float gpuTimeMs = 35.0f * (scale * scale) / (0.70f * 0.70f);
			const float frameTimeMs = std::max(gpuTimeMs, 10.0f);
			auto out = Step(controller, settings, frameTimeMs, gpuTimeMs);
			changes += out.scaleChanged ? 1 : 0;
		}
		const auto out = controller.GetOutput();
		assert(out.boundState == Adaptive80::BoundState::Gpu || out.boundState == Adaptive80::BoundState::Mixed || out.state == Adaptive80::State::Target);
		assert(out.scale < 0.69f);
		assert(out.scale >= settings.minScale - 0.001f);
		assert(changes > 0 && changes < 10);
	}

	// Mixed bottleneck: shed only GPU work down to normal minimum, not emergency.
	{
		Adaptive80::Controller controller;
		controller.Reset(0.70f);
		for (int i = 0; i < 180; ++i)
			Step(controller, settings, 52.0f, 32.0f);
		const auto out = controller.GetOutput();
		assert(out.boundState == Adaptive80::BoundState::Mixed || out.boundState == Adaptive80::BoundState::Cpu);
		assert(out.scale >= settings.minScale - 0.001f);
	}

	// Fixed scale remains churn-free.
	{
		auto fixed = settings;
		fixed.minScale = fixed.maxScale = fixed.emergencyMinScale = 0.52f;
		Adaptive80::Controller controller;
		controller.Reset(0.52f);
		int changes = 0;
		for (int i = 0; i < 240; ++i)
			changes += Step(controller, fixed, 45.0f, 40.0f).scaleChanged ? 1 : 0;
		assert(changes == 0);
		assert(std::abs(controller.GetOutput().scale - 0.52f) < 0.001f);
	}

	// Provider safety retained from v0.4.
	{
		const auto bounds = Adaptive80::Controller::ConstrainScaleBounds(0.48f, 0.64f, 0.38f, 0.50f, 1.00f);
		assert(std::abs(bounds.minScale - 0.50f) < 0.001f);
		assert(std::abs(bounds.maxScale - 0.64f) < 0.001f);
		assert(std::abs(bounds.emergencyMinScale - 0.50f) < 0.001f);
		assert(bounds.clampedByProvider);
	}

	std::cout << "AdaptiveResolutionController v0.5 tests passed\n";
	return 0;
}

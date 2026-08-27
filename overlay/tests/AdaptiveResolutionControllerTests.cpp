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
			.resolutionStep = 0.04f,
			.holdSeconds = 0.28f,
			.targetHoldSeconds = 0.80f,
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

	// 22-27 ms target band: stable scale, no event churn.
	{
		Adaptive80::Controller controller;
		controller.Reset(0.60f);
		int changes = 0;
		for (int i = 0; i < 180; ++i) {
			auto out = Step(controller, settings, 25.0f, 21.0f);
			changes += out.scaleChanged ? 1 : 0;
		}
		const auto output = controller.GetOutput();
		assert(std::abs(output.scale - 0.60f) < 0.001f);
		assert(changes == 0);
	}

	// GPU-bound workload: scale changes must be quantized and separated by hold.
	{
		Adaptive80::Controller controller;
		controller.Reset(0.70f);
		float frameTimeMs = 35.0f;
		int changes = 0;
		int framesSinceChange = 1000;
		for (int i = 0; i < 240; ++i) {
			const float scale = controller.GetOutput().scale;
			const float gpuTimeMs = 35.0f * (scale * scale) / (0.70f * 0.70f);
			frameTimeMs = std::max(10.0f, gpuTimeMs);
			auto out = Step(controller, settings, frameTimeMs, gpuTimeMs);
			if (out.scaleChanged) {
				// At ~35 ms/frame and a 280 ms hold there should normally be many
				// frames between events, never a frame-to-frame resize loop.
				assert(framesSinceChange >= 5);
				framesSinceChange = 0;
				++changes;
			}
			++framesSinceChange;
		}
		const auto output = controller.GetOutput();
		assert(output.boundState == Adaptive80::BoundState::Gpu || output.boundState == Adaptive80::BoundState::Mixed);
		assert(output.scale < 0.69f);
		assert(output.scale >= settings.minScale - 0.001f);
		assert(changes > 0 && changes < 12);
		assert(frameTimeMs <= 29.0f);
	}

	// Pure CPU/engine bottleneck: preserve quality.
	{
		Adaptive80::Controller controller;
		controller.Reset(0.70f);
		for (int i = 0; i < 120; ++i)
			Step(controller, settings, 50.0f, 12.0f);
		const auto output = controller.GetOutput();
		assert(output.boundState == Adaptive80::BoundState::Cpu || output.cpuGuardActive);
		assert(output.scale >= 0.69f);
	}

	// Mixed bottleneck from the user's dragon stress test shape: pre-FG ~52 ms,
	// GPU ~30 ms. It should shed some GPU work but never chase emergency scale.
	{
		Adaptive80::Controller controller;
		controller.Reset(0.70f);
		for (int i = 0; i < 150; ++i)
			Step(controller, settings, 52.0f, 30.0f);
		const auto output = controller.GetOutput();
		assert(output.boundState == Adaptive80::BoundState::Mixed || output.boundState == Adaptive80::BoundState::Cpu);
		assert(output.scale >= settings.minScale - 0.001f);
	}

	// Fixed scale A/B mode: if min=max=emergency, the controller cannot generate
	// render-size churn. This mirrors the in-game test that removed freezes.
	{
		auto fixed = settings;
		fixed.minScale = 0.52f;
		fixed.maxScale = 0.52f;
		fixed.emergencyMinScale = 0.52f;
		Adaptive80::Controller controller;
		controller.Reset(0.52f);
		int changes = 0;
		for (int i = 0; i < 240; ++i) {
			auto out = Step(controller, fixed, 45.0f, 40.0f);
			changes += out.scaleChanged ? 1 : 0;
		}
		assert(changes == 0);
		assert(std::abs(controller.GetOutput().scale - 0.52f) < 0.001f);
	}

	// Severe GPU load may enter emergency territory, but only via held events;
	// recovery then waits for a stable target interval and climbs slowly.
	{
		Adaptive80::Controller controller;
		controller.Reset(0.70f);
		float frameTimeMs = 100.0f;
		int rescueChanges = 0;
		for (int i = 0; i < 180; ++i) {
			const float scale = controller.GetOutput().scale;
			const float gpuTimeMs = 100.0f * (scale * scale) / (0.70f * 0.70f);
			frameTimeMs = std::max(8.0f, gpuTimeMs);
			auto out = Step(controller, settings, frameTimeMs, gpuTimeMs);
			rescueChanges += out.scaleChanged ? 1 : 0;
		}
		const float rescuedScale = controller.GetOutput().scale;
		assert(rescuedScale <= settings.minScale + 0.001f);
		assert(rescuedScale >= settings.emergencyMinScale - 0.001f);
		assert(rescueChanges > 0 && rescueChanges < 15);

		const float beforeRecovery = controller.GetOutput().scale;
		for (int i = 0; i < 40; ++i)
			Step(controller, settings, 14.0f, 12.0f);
		// Less than roughly one second should not instantly jump to max quality.
		assert(controller.GetOutput().scale <= beforeRecovery + settings.resolutionStep + 0.001f);

		for (int i = 0; i < 500; ++i)
			Step(controller, settings, 14.0f, 12.0f);
		assert(controller.GetOutput().scale > 0.66f);
	}


	// v0.4 provider-safety clamp: user requests below NVIDIA's reported range
	// must never escape the provider envelope.
	{
		const auto bounds = Adaptive80::Controller::ConstrainScaleBounds(0.48f, 0.64f, 0.38f, 0.50f, 0.67f);
		assert(std::abs(bounds.minScale - 0.50f) < 0.001f);
		assert(std::abs(bounds.maxScale - 0.64f) < 0.001f);
		assert(std::abs(bounds.emergencyMinScale - 0.50f) < 0.001f);
		assert(bounds.clampedByProvider);
	}

	// If the user's entire requested window lies below the provider range, AD80
	// safely collapses to the provider minimum instead of inventing a resolution.
	{
		const auto bounds = Adaptive80::Controller::ConstrainScaleBounds(0.40f, 0.48f, 0.35f, 0.52f, 0.70f);
		assert(std::abs(bounds.minScale - 0.52f) < 0.001f);
		assert(std::abs(bounds.maxScale - 0.52f) < 0.001f);
		assert(std::abs(bounds.emergencyMinScale - 0.52f) < 0.001f);
		assert(bounds.clampedByProvider);
	}

	std::cout << "AdaptiveResolutionController v0.4 tests passed\n";
	return 0;
}

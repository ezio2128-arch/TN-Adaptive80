#include "AdaptiveResolutionController.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float kMinimumFrameTimeMs = 1.0f;
	constexpr float kMaximumFrameTimeMs = 250.0f;
	constexpr float kMinimumDeltaSeconds = 1.0f / 500.0f;
	constexpr float kMaximumDeltaSeconds = 0.10f;
	constexpr float kGpuBoundEnterRatio = 0.82f;
	constexpr float kCpuBoundEnterRatio = 0.66f;
	constexpr std::uint32_t kGpuBoundConfirmationSamples = 3;
	constexpr std::uint32_t kMixedBoundConfirmationSamples = 4;
	constexpr std::uint32_t kCpuBoundConfirmationSamples = 7;
	constexpr std::uint32_t kProbeEvaluationSamples = 6;
	constexpr float kCpuGuardHoldSeconds = 2.0f;
	constexpr float kScaleEpsilon = 0.0025f;

	float ClampScale(float value)
	{
		return std::clamp(value, 0.33f, 1.0f);
	}
}

namespace Adaptive80
{
	void Controller::Reset(float initialScale)
	{
		output = {};
		output.scale = ClampScale(initialScale);
		output.requestedScale = output.scale;
		output.state = State::Target;
		output.boundState = BoundState::Unknown;
		initialized = true;
		gpuBoundSamples = 0;
		mixedBoundSamples = 0;
		cpuBoundSamples = 0;
		recoverySamples = 0;
		probeActive = false;
		probeSamples = 0;
		probeStartScale = output.scale;
		probeStartFrameTimeMs = 0.0f;
		probeFrameTimeSumMs = 0.0f;
		probeSettleSecondsRemaining = 0.0f;
		cpuGuardRestoreScale = output.scale;
		cpuGuardSecondsRemaining = 0.0f;
	}

	Output Controller::Update(const Settings& rawSettings, const Sample& sample)
	{
		Settings settings = rawSettings;
		settings.targetFrameTimeMs = std::clamp(settings.targetFrameTimeMs, 8.0f, 66.67f);
		settings.maxScale = ClampScale(settings.maxScale);
		settings.minScale = std::clamp(ClampScale(settings.minScale), 0.33f, settings.maxScale);
		settings.emergencyMinScale = std::clamp(ClampScale(settings.emergencyMinScale), 0.33f, settings.minScale);
		settings.attackScalePerSecond = std::clamp(settings.attackScalePerSecond, 0.05f, 3.0f);
		settings.recoveryScalePerSecond = std::clamp(settings.recoveryScalePerSecond, 0.005f, 0.50f);
		settings.gpuHeadroom = std::clamp(settings.gpuHeadroom, 0.80f, 0.98f);
		settings.resolutionStep = std::clamp(settings.resolutionStep, 0.02f, 0.08f);
		settings.holdSeconds = std::clamp(settings.holdSeconds, 0.15f, 1.00f);
		settings.targetHoldSeconds = std::clamp(settings.targetHoldSeconds, 0.35f, 3.00f);

		if (!initialized)
			Reset(settings.maxScale);

		output.scaleChanged = false;
		output.lastScaleDelta = 0.0f;
		output.scale = std::clamp(output.scale, settings.emergencyMinScale, settings.maxScale);
		output.requestedScale = std::clamp(output.requestedScale, settings.emergencyMinScale, settings.maxScale);

		const float deltaSeconds = std::clamp(
			std::isfinite(sample.deltaSeconds) ? sample.deltaSeconds : 0.0f,
			kMinimumDeltaSeconds,
			kMaximumDeltaSeconds);
		output.holdRemainingSeconds = std::max(0.0f, output.holdRemainingSeconds - deltaSeconds);

		if (sample.paused) {
			output.state = State::Paused;
			return output;
		}

		if (!std::isfinite(sample.frameTimeMs) || sample.frameTimeMs < kMinimumFrameTimeMs || sample.frameTimeMs > kMaximumFrameTimeMs)
			return output;

		if (output.smoothedFrameTimeMs <= 0.0f) {
			output.smoothedFrameTimeMs = sample.frameTimeMs;
		} else {
			// Losses are learned quickly; recoveries are deliberately slow so a
			// single good frame never starts a quality-recovery oscillation.
			const float timeConstant = sample.frameTimeMs > output.smoothedFrameTimeMs ? 0.12f : 0.58f;
			const float alpha = 1.0f - std::exp(-deltaSeconds / timeConstant);
			output.smoothedFrameTimeMs += (sample.frameTimeMs - output.smoothedFrameTimeMs) * alpha;
		}

		if (sample.frameTimeMs > 65.0f)
			output.smoothedFrameTimeMs = std::max(output.smoothedFrameTimeMs, sample.frameTimeMs * 0.80f);

		const float lowerDeadBandMs = settings.targetFrameTimeMs * 0.88f;
		const float upperDeadBandMs = settings.targetFrameTimeMs * 1.08f;
		const float rescueBoundaryMs = std::max(40.0f, settings.targetFrameTimeMs * 1.60f);
		const float emergencyBoundaryMs = std::max(65.0f, settings.targetFrameTimeMs * 2.60f);

		UpdateBoundState(settings, sample, upperDeadBandMs);
		UpdateProbe(settings, upperDeadBandMs, sample.frameTimeMs, deltaSeconds);

		const float effectiveFrameTimeMs = output.smoothedFrameTimeMs;
		if (effectiveFrameTimeMs <= upperDeadBandMs)
			output.targetStableSeconds = std::min(output.targetStableSeconds + deltaSeconds, 10.0f);
		else
			output.targetStableSeconds = 0.0f;

		if (output.cpuGuardActive) {
			cpuGuardSecondsRemaining = std::max(0.0f, cpuGuardSecondsRemaining - deltaSeconds);
			output.requestedScale = std::min(cpuGuardRestoreScale, settings.maxScale);
			output.state = State::CpuLimited;

			// CPU Guard restores quality in discrete, held steps rather than changing
			// render size every frame as v0.2 did.
			if (output.holdRemainingSeconds <= 0.0f && output.scale + kScaleEpsilon < output.requestedScale)
				ApplyScaleEvent(NextHigherScale(settings, output.requestedScale, 1), settings, 1.35f);

			const bool recovered = effectiveFrameTimeMs <= upperDeadBandMs;
			if (recovered && ++recoverySamples >= 4) {
				output.cpuGuardActive = false;
				output.boundState = BoundState::Unknown;
				cpuGuardSecondsRemaining = 0.0f;
				recoverySamples = 0;
			} else if (!recovered) {
				recoverySamples = 0;
			}

			if (cpuGuardSecondsRemaining <= 0.0f && !recovered) {
				output.cpuGuardActive = false;
				output.boundState = BoundState::Unknown;
				gpuBoundSamples = 0;
				mixedBoundSamples = 0;
				cpuBoundSamples = 0;
			}
			return output;
		}

		if (effectiveFrameTimeMs <= lowerDeadBandMs) {
			output.state = State::Quality;
			output.requestedScale = NextHigherScale(settings, settings.maxScale, 1);
			// Quality recovery waits for a sustained good interval and then moves one
			// quantized step at a time. Recovery Speed remains meaningful by defining
			// the minimum time required to earn one Resolution Step.
			const float recoveryWaitSeconds = std::clamp(
				settings.resolutionStep / std::max(settings.recoveryScalePerSecond, 0.005f),
				settings.targetHoldSeconds,
				4.0f);
			if (output.targetStableSeconds >= recoveryWaitSeconds && output.holdRemainingSeconds <= 0.0f &&
				output.scale + kScaleEpsilon < settings.maxScale) {
				ApplyScaleEvent(output.requestedScale, settings, 1.60f);
				output.targetStableSeconds = 0.0f;
			}
			return output;
		}

		if (effectiveFrameTimeMs <= upperDeadBandMs) {
			output.state = State::Target;
			output.requestedScale = output.scale;
			return output;
		}

		const bool emergency = effectiveFrameTimeMs > emergencyBoundaryMs;
		output.state = emergency ? State::Emergency : State::Rescue;

		if (settings.cpuGuard && output.boundState == BoundState::Cpu) {
			ActivateCpuGuard(std::max(output.scale, probeStartScale));
			return output;
		}

		// While timing is unknown and a response probe is still settling, freeze the
		// applied scale. This avoids several consecutive changes before the first one
		// has had time to affect DLSS/FSR + frame generation.
		if (settings.cpuGuard && output.boundState == BoundState::Unknown && probeActive) {
			output.state = State::Stabilizing;
			return output;
		}

		float floorScale = emergency ? settings.emergencyMinScale : settings.minScale;
		std::uint32_t attackSteps = 1;
		float holdMultiplier = 1.0f;

		if (output.boundState == BoundState::Mixed) {
			// Mixed means both the engine/CPU and GPU contribute. Shed enough GPU work
			// to create headroom, but never chase the emergency floor for a CPU-heavy scene.
			floorScale = settings.minScale;
			attackSteps = 1;
			holdMultiplier = 1.35f;
		} else if (output.boundState == BoundState::Gpu) {
			if (emergency && settings.attackScalePerSecond >= 1.0f)
				attackSteps = 2;
			else if (effectiveFrameTimeMs > rescueBoundaryMs && settings.attackScalePerSecond >= 1.25f)
				attackSteps = 2;
		} else {
			// Unknown timing gets one cautious step followed by a response probe.
			attackSteps = 1;
			holdMultiplier = 1.20f;
		}

		output.requestedScale = NextLowerScale(settings, floorScale, attackSteps);
		if (output.requestedScale >= output.scale - kScaleEpsilon)
			return output;

		// The hold is normally absolute. Only an extreme >100 ms condition can
		// shorten it, and even then only after half of the settling interval elapsed.
		const bool catastrophic = effectiveFrameTimeMs > 100.0f;
		const bool emergencyBypass = catastrophic && output.holdRemainingSeconds <= settings.holdSeconds * 0.50f;
		if (output.holdRemainingSeconds > 0.0f && !emergencyBypass) {
			output.state = State::Stabilizing;
			return output;
		}

		const float previousScale = output.scale;
		if (ApplyScaleEvent(output.requestedScale, settings, holdMultiplier) &&
			settings.cpuGuard && output.boundState == BoundState::Unknown && previousScale - output.scale >= 0.015f) {
			StartProbe(previousScale, effectiveFrameTimeMs, settings.holdSeconds * holdMultiplier);
		}

		return output;
	}

	void Controller::UpdateBoundState(const Settings& settings, const Sample& sample, float upperDeadBandMs)
	{
		if (output.cpuGuardActive)
			return;

		if (!sample.gpuTimeValid || !std::isfinite(sample.gpuTimeMs) || sample.gpuTimeMs <= 0.0f || sample.gpuTimeMs > 250.0f) {
			output.gpuBusyRatio = 0.0f;
			return;
		}

		if (output.smoothedGpuTimeMs <= 0.0f)
			output.smoothedGpuTimeMs = sample.gpuTimeMs;
		else {
			const float gpuAlpha = sample.gpuTimeMs > output.smoothedGpuTimeMs ? 0.32f : 0.18f;
			output.smoothedGpuTimeMs += (sample.gpuTimeMs - output.smoothedGpuTimeMs) * gpuAlpha;
		}

		const float referenceFrameTimeMs =
			std::isfinite(sample.gpuReferenceFrameTimeMs) && sample.gpuReferenceFrameTimeMs > 0.0f ?
				sample.gpuReferenceFrameTimeMs :
				sample.frameTimeMs;
		const float measuredBusyRatio = std::clamp(sample.gpuTimeMs / std::max(referenceFrameTimeMs, 0.1f), 0.0f, 1.25f);
		if (output.gpuBusyRatio <= 0.0f)
			output.gpuBusyRatio = measuredBusyRatio;
		else
			output.gpuBusyRatio += (measuredBusyRatio - output.gpuBusyRatio) * 0.22f;

		if (output.smoothedFrameTimeMs <= upperDeadBandMs) {
			gpuBoundSamples = 0;
			mixedBoundSamples = 0;
			cpuBoundSamples = 0;
			if (output.boundState == BoundState::Cpu)
				output.boundState = BoundState::Unknown;
			return;
		}

		const float gpuBudgetMs = settings.targetFrameTimeMs * settings.gpuHeadroom;
		const bool gpuOverBudget = output.smoothedGpuTimeMs > gpuBudgetMs * 1.03f;
		const bool gpuComfortable = output.smoothedGpuTimeMs < gpuBudgetMs * 0.88f;

		if (output.gpuBusyRatio >= kGpuBoundEnterRatio) {
			++gpuBoundSamples;
			mixedBoundSamples = mixedBoundSamples > 0 ? mixedBoundSamples - 1 : 0;
			cpuBoundSamples = 0;
		} else if (gpuOverBudget) {
			// Dragon-style scenes measured in v0.2 had a large CPU/engine component
			// but GPU times still well above the 40-FPS budget. Treat them as mixed.
			++mixedBoundSamples;
			gpuBoundSamples = gpuBoundSamples > 0 ? gpuBoundSamples - 1 : 0;
			cpuBoundSamples = cpuBoundSamples > 0 ? cpuBoundSamples - 1 : 0;
		} else if (output.gpuBusyRatio <= kCpuBoundEnterRatio && gpuComfortable) {
			++cpuBoundSamples;
			gpuBoundSamples = 0;
			mixedBoundSamples = mixedBoundSamples > 0 ? mixedBoundSamples - 1 : 0;
		} else {
			gpuBoundSamples = gpuBoundSamples > 0 ? gpuBoundSamples - 1 : 0;
			mixedBoundSamples = mixedBoundSamples > 0 ? mixedBoundSamples - 1 : 0;
			cpuBoundSamples = cpuBoundSamples > 0 ? cpuBoundSamples - 1 : 0;
		}

		if (gpuBoundSamples >= kGpuBoundConfirmationSamples) {
			output.boundState = BoundState::Gpu;
			output.cpuGuardActive = false;
		} else if (mixedBoundSamples >= kMixedBoundConfirmationSamples) {
			output.boundState = BoundState::Mixed;
			output.cpuGuardActive = false;
		} else if (settings.cpuGuard && cpuBoundSamples >= kCpuBoundConfirmationSamples) {
			output.boundState = BoundState::Cpu;
			ActivateCpuGuard(std::max(output.scale, probeStartScale));
		}
	}

	void Controller::StartProbe(float startScale, float startFrameTimeMs, float settleSeconds)
	{
		probeActive = true;
		probeSamples = 0;
		probeStartScale = startScale;
		probeStartFrameTimeMs = startFrameTimeMs;
		probeFrameTimeSumMs = 0.0f;
		probeSettleSecondsRemaining = std::max(0.0f, settleSeconds);
	}

	void Controller::UpdateProbe(const Settings& settings, float upperDeadBandMs, float currentFrameTimeMs, float deltaSeconds)
	{
		if (!probeActive)
			return;

		if (probeSettleSecondsRemaining > 0.0f) {
			probeSettleSecondsRemaining = std::max(0.0f, probeSettleSecondsRemaining - deltaSeconds);
			return;
		}

		probeFrameTimeSumMs += currentFrameTimeMs;
		if (++probeSamples < kProbeEvaluationSamples)
			return;

		probeActive = false;
		const float observedFrameTimeMs = probeFrameTimeSumMs / static_cast<float>(probeSamples);
		const float pixelReduction = 1.0f - (output.scale * output.scale) /
			std::max(probeStartScale * probeStartScale, 0.001f);
		const float frameImprovement = (probeStartFrameTimeMs - observedFrameTimeMs) /
			std::max(probeStartFrameTimeMs, 0.1f);
		const float requiredImprovement = std::max(0.025f, pixelReduction * 0.20f);

		if (settings.cpuGuard && output.smoothedFrameTimeMs > upperDeadBandMs &&
			pixelReduction > 0.025f && frameImprovement < requiredImprovement) {
			// If the GPU itself is still over its timing budget, this is Mixed rather
			// than pure CPU. Otherwise restore the lost quality with CPU Guard.
			const float gpuBudgetMs = settings.targetFrameTimeMs * settings.gpuHeadroom;
			if (output.smoothedGpuTimeMs > gpuBudgetMs * 1.03f) {
				output.boundState = BoundState::Mixed;
			} else {
				output.boundState = BoundState::Cpu;
				ActivateCpuGuard(probeStartScale);
			}
		} else if (pixelReduction > 0.025f && frameImprovement >= requiredImprovement) {
			output.boundState = output.gpuBusyRatio >= kGpuBoundEnterRatio ? BoundState::Gpu : BoundState::Mixed;
		}
	}

	void Controller::ActivateCpuGuard(float restoreScale)
	{
		output.cpuGuardActive = true;
		output.boundState = BoundState::Cpu;
		output.state = State::CpuLimited;
		cpuGuardRestoreScale = std::max(output.scale, ClampScale(restoreScale));
		cpuGuardSecondsRemaining = kCpuGuardHoldSeconds;
		probeActive = false;
		probeSamples = 0;
		probeFrameTimeSumMs = 0.0f;
		probeSettleSecondsRemaining = 0.0f;
		gpuBoundSamples = 0;
		mixedBoundSamples = 0;
		cpuBoundSamples = 0;
		recoverySamples = 0;
	}

	float Controller::NextLowerScale(const Settings& settings, float floorScale, std::uint32_t steps) const
	{
		float candidate = output.scale;
		for (std::uint32_t i = 0; i < std::max<std::uint32_t>(steps, 1); ++i)
			candidate = std::max(floorScale, candidate - settings.resolutionStep);
		return std::clamp(candidate, floorScale, settings.maxScale);
	}

	float Controller::NextHigherScale(const Settings& settings, float ceilingScale, std::uint32_t steps) const
	{
		float candidate = output.scale;
		for (std::uint32_t i = 0; i < std::max<std::uint32_t>(steps, 1); ++i)
			candidate = std::min(ceilingScale, candidate + settings.resolutionStep);
		return std::clamp(candidate, settings.emergencyMinScale, ceilingScale);
	}

	bool Controller::ApplyScaleEvent(float requestedScale, const Settings& settings, float holdMultiplier)
	{
		const float clamped = std::clamp(requestedScale, settings.emergencyMinScale, settings.maxScale);
		output.requestedScale = clamped;
		if (std::abs(clamped - output.scale) < kScaleEpsilon)
			return false;

		const float previousScale = output.scale;
		output.scale = clamped;
		output.lastScaleDelta = output.scale - previousScale;
		output.scaleChanged = true;
		output.holdRemainingSeconds = std::max(output.holdRemainingSeconds, settings.holdSeconds * std::max(holdMultiplier, 0.5f));
		output.state = State::Stabilizing;
		return true;
	}

	const char* Controller::GetStateName(State state)
	{
		switch (state) {
		case State::Disabled:
			return "Disabled";
		case State::Quality:
			return "Quality";
		case State::Target:
			return "Target";
		case State::Rescue:
			return "Rescue";
		case State::Emergency:
			return "Emergency";
		case State::CpuLimited:
			return "CPU Guard";
		case State::Stabilizing:
			return "Stabilizing";
		case State::Paused:
			return "Paused";
		}
		return "Unknown";
	}

	const char* Controller::GetBoundStateName(BoundState state)
	{
		switch (state) {
		case BoundState::Unknown:
			return "Learning";
		case BoundState::Gpu:
			return "GPU";
		case BoundState::Mixed:
			return "Mixed";
		case BoundState::Cpu:
			return "CPU";
		}
		return "Unknown";
	}
}

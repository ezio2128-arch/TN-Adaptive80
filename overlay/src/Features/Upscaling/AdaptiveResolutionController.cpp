#include "AdaptiveResolutionController.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float kMinimumFrameTimeMs = 1.0f;
	constexpr float kMaximumFrameTimeMs = 250.0f;
	constexpr float kMinimumDeltaSeconds = 1.0f / 500.0f;
	constexpr float kMaximumDeltaSeconds = 0.10f;
	constexpr float kGpuBoundEnterRatio = 0.86f;
	constexpr float kMixedBoundEnterRatio = 0.58f;
	constexpr float kCpuBoundEnterRatio = 0.50f;
	constexpr std::uint32_t kGpuBoundConfirmationSamples = 3;
	constexpr std::uint32_t kMixedBoundConfirmationSamples = 4;
	constexpr std::uint32_t kCpuBoundConfirmationSamples = 4;
	constexpr std::uint32_t kProbeEvaluationSamples = 6;
	constexpr float kCpuGuardHoldSeconds = 2.0f;
	constexpr float kScaleEpsilon = 0.0025f;
	constexpr float kGpuTimestampAlignmentTolerance = 0.30f;

	float ClampScale(float value)
	{
		return std::clamp(value, 0.33f, 1.0f);
	}
}

namespace Adaptive80
{
	ScaleBounds Controller::ConstrainScaleBounds(
		float requestedMinScale, float requestedMaxScale, float requestedEmergencyMinScale,
		float providerMinScale, float providerMaxScale)
	{
		providerMinScale = ClampScale(providerMinScale);
		providerMaxScale = ClampScale(providerMaxScale);
		if (providerMaxScale < providerMinScale)
			std::swap(providerMinScale, providerMaxScale);

		requestedMaxScale = ClampScale(requestedMaxScale);
		requestedMinScale = ClampScale(requestedMinScale);
		requestedEmergencyMinScale = ClampScale(requestedEmergencyMinScale);

		ScaleBounds out{};
		out.maxScale = std::clamp(requestedMaxScale, providerMinScale, providerMaxScale);
		out.minScale = std::clamp(requestedMinScale, providerMinScale, out.maxScale);
		out.emergencyMinScale = std::clamp(requestedEmergencyMinScale, providerMinScale, out.minScale);
		out.clampedByProvider =
			std::abs(out.maxScale - requestedMaxScale) > kScaleEpsilon ||
			std::abs(out.minScale - requestedMinScale) > kScaleEpsilon ||
			std::abs(out.emergencyMinScale - requestedEmergencyMinScale) > kScaleEpsilon;
		return out;
	}

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
		settings.preferredFrameTimeMs = std::clamp(settings.preferredFrameTimeMs, 6.0f, settings.targetFrameTimeMs);
		settings.maxScale = ClampScale(settings.maxScale);
		settings.minScale = std::clamp(ClampScale(settings.minScale), 0.33f, settings.maxScale);
		settings.emergencyMinScale = std::clamp(ClampScale(settings.emergencyMinScale), 0.33f, settings.minScale);
		settings.attackScalePerSecond = std::clamp(settings.attackScalePerSecond, 0.05f, 3.0f);
		settings.recoveryScalePerSecond = std::clamp(settings.recoveryScalePerSecond, 0.005f, 0.50f);
		settings.gpuHeadroom = std::clamp(settings.gpuHeadroom, 0.80f, 0.98f);
		settings.resolutionStep = std::clamp(settings.resolutionStep, 0.02f, 0.08f);
		settings.holdSeconds = std::clamp(settings.holdSeconds, 0.20f, 1.50f);
		settings.targetHoldSeconds = std::clamp(settings.targetHoldSeconds, 0.35f, 4.00f);
		settings.pressureQualificationSeconds = std::clamp(settings.pressureQualificationSeconds, 0.15f, 1.50f);

		if (!initialized)
			Reset(settings.maxScale);

		output.scaleChanged = false;
		output.lastScaleDelta = 0.0f;
		output.resizeQualified = false;
		output.scale = std::clamp(output.scale, settings.emergencyMinScale, settings.maxScale);
		output.requestedScale = std::clamp(output.requestedScale, settings.emergencyMinScale, settings.maxScale);

		const float deltaSeconds = std::clamp(
			std::isfinite(sample.deltaSeconds) ? sample.deltaSeconds : 0.0f,
			kMinimumDeltaSeconds,
			kMaximumDeltaSeconds);
		output.holdRemainingSeconds = std::max(0.0f, output.holdRemainingSeconds - deltaSeconds);

		if (sample.paused) {
			output.state = State::Paused;
			output.pressureStableSeconds = 0.0f;
			return output;
		}

		if (!std::isfinite(sample.frameTimeMs) || sample.frameTimeMs < kMinimumFrameTimeMs || sample.frameTimeMs > kMaximumFrameTimeMs)
			return output;

		if (output.smoothedFrameTimeMs <= 0.0f) {
			output.smoothedFrameTimeMs = sample.frameTimeMs;
		} else {
			// Fast attack, slower release. Camera/streaming spikes are then filtered a
			// second time by the pressure qualification gate below before any resize.
			const float timeConstant = sample.frameTimeMs > output.smoothedFrameTimeMs ? 0.14f : 0.50f;
			const float alpha = 1.0f - std::exp(-deltaSeconds / timeConstant);
			output.smoothedFrameTimeMs += (sample.frameTimeMs - output.smoothedFrameTimeMs) * alpha;
		}

		if (sample.frameTimeMs > 80.0f)
			output.smoothedFrameTimeMs = std::max(output.smoothedFrameTimeMs, sample.frameTimeMs * 0.72f);

		// 40 FPS is the floor, not the ceiling. The controller only spends extra
		// performance on quality when it is faster than the preferred reserve.
		const float qualityRecoveryBoundaryMs = settings.preferredFrameTimeMs * 0.98f;
		const float upperDeadBandMs = settings.targetFrameTimeMs * 1.02f;
		const float rescueBoundaryMs = std::max(38.0f, settings.targetFrameTimeMs * 1.52f);
		const float emergencyBoundaryMs = std::max(65.0f, settings.targetFrameTimeMs * 2.60f);

		UpdateBoundState(settings, sample, upperDeadBandMs);
		UpdateProbe(settings, upperDeadBandMs, sample.frameTimeMs, deltaSeconds);

		const float effectiveFrameTimeMs = output.smoothedFrameTimeMs;
		if (effectiveFrameTimeMs <= upperDeadBandMs)
			output.targetStableSeconds = std::min(output.targetStableSeconds + deltaSeconds, 10.0f);
		else
			output.targetStableSeconds = 0.0f;

		const bool gpuPressure = effectiveFrameTimeMs > upperDeadBandMs &&
			(output.boundState == BoundState::Gpu || output.boundState == BoundState::Mixed);
		if (gpuPressure && output.gpuTimingAligned) {
			output.pressureStableSeconds = std::min(output.pressureStableSeconds + deltaSeconds, 5.0f);
		} else if (output.boundState == BoundState::Cpu || !output.gpuTimingAligned) {
			output.pressureStableSeconds = 0.0f;
		} else {
			output.pressureStableSeconds = std::max(0.0f, output.pressureStableSeconds - deltaSeconds * 2.0f);
		}

		if (output.cpuGuardActive) {
			cpuGuardSecondsRemaining = std::max(0.0f, cpuGuardSecondsRemaining - deltaSeconds);
			// v0.5 stability rule: CPU Guard freezes the current render size. Do not
			// resize upward in the middle of a camera/streaming CPU spike; normal Slow
			// Recovery may restore quality only after performance is stable again.
			output.requestedScale = output.scale;
			output.state = State::CpuLimited;
			output.pressureStableSeconds = 0.0f;

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

		if (effectiveFrameTimeMs <= qualityRecoveryBoundaryMs) {
			output.state = State::Quality;
			output.requestedScale = NextHigherScale(settings, settings.maxScale, 1);
			const float recoveryWaitSeconds = std::clamp(
				settings.resolutionStep / std::max(settings.recoveryScalePerSecond, 0.005f),
				settings.targetHoldSeconds,
				4.0f);
			if (output.targetStableSeconds >= recoveryWaitSeconds && output.holdRemainingSeconds <= 0.0f &&
				output.scale + kScaleEpsilon < settings.maxScale) {
				ApplyScaleEvent(output.requestedScale, settings, 1.35f);
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

		// If GPU timing is delayed relative to a new camera/streaming spike, do not
		// resize on stale evidence. Wait for aligned timing first.
		const bool hasGpuTiming = output.smoothedGpuTimeMs > 0.0f;
		if (hasGpuTiming && (!output.gpuTimingAligned || output.boundState == BoundState::Unknown)) {
			output.state = State::Stabilizing;
			return output;
		}

		float floorScale = emergency ? settings.emergencyMinScale : settings.minScale;
		std::uint32_t attackSteps = 1;
		float holdMultiplier = 1.0f;

		if (output.boundState == BoundState::Mixed) {
			floorScale = settings.minScale;
			attackSteps = 1;
			holdMultiplier = 1.20f;
		} else if (output.boundState == BoundState::Gpu) {
			if (emergency && settings.attackScalePerSecond >= 1.0f)
				attackSteps = 2;
			else if (effectiveFrameTimeMs > rescueBoundaryMs && settings.attackScalePerSecond >= 1.25f)
				attackSteps = 2;
		} else if (!hasGpuTiming) {
			// GPU timestamp support unavailable: allow only one qualified probe step.
			attackSteps = 1;
			holdMultiplier = 1.30f;
		}

		output.requestedScale = NextLowerScale(settings, floorScale, attackSteps);
		if (output.requestedScale >= output.scale - kScaleEpsilon)
			return output;

		// v0.5: sustained GPU pressure must qualify a resize. A catastrophic GPU-bound
		// frame may shorten qualification, but a one-off camera turn may not.
		float qualificationSeconds = settings.pressureQualificationSeconds;
		if (emergency && output.boundState == BoundState::Gpu)
			qualificationSeconds *= 0.45f;
		const bool qualified = output.pressureStableSeconds >= qualificationSeconds || (!hasGpuTiming && output.targetStableSeconds <= 0.0f && effectiveFrameTimeMs > rescueBoundaryMs);
		output.resizeQualified = qualified;
		if (!qualified) {
			output.state = State::Stabilizing;
			return output;
		}

		if (output.holdRemainingSeconds > 0.0f) {
			output.state = State::Stabilizing;
			return output;
		}

		const float previousScale = output.scale;
		if (ApplyScaleEvent(output.requestedScale, settings, holdMultiplier)) {
			output.pressureStableSeconds = 0.0f;
			if (!hasGpuTiming && settings.cpuGuard && previousScale - output.scale >= 0.015f)
				StartProbe(previousScale, effectiveFrameTimeMs, settings.holdSeconds * holdMultiplier);
		}

		return output;
	}

	void Controller::UpdateBoundState(const Settings& settings, const Sample& sample, float upperDeadBandMs)
	{
		if (output.cpuGuardActive)
			return;

		// Update GPU timing only when a new asynchronous timestamp result arrives.
		if (sample.gpuTimeValid && std::isfinite(sample.gpuTimeMs) && sample.gpuTimeMs > 0.0f && sample.gpuTimeMs <= 250.0f) {
			if (output.smoothedGpuTimeMs <= 0.0f)
				output.smoothedGpuTimeMs = sample.gpuTimeMs;
			else {
				const float gpuAlpha = sample.gpuTimeMs > output.smoothedGpuTimeMs ? 0.38f : 0.20f;
				output.smoothedGpuTimeMs += (sample.gpuTimeMs - output.smoothedGpuTimeMs) * gpuAlpha;
			}

			const float referenceFrameTimeMs =
				std::isfinite(sample.gpuReferenceFrameTimeMs) && sample.gpuReferenceFrameTimeMs > 0.0f ?
					sample.gpuReferenceFrameTimeMs : sample.frameTimeMs;
			const float alignmentError = std::abs(referenceFrameTimeMs - sample.frameTimeMs) /
				std::max(std::max(referenceFrameTimeMs, sample.frameTimeMs), 0.1f);
			output.gpuTimingAligned = alignmentError <= kGpuTimestampAlignmentTolerance;
		}

		if (output.smoothedGpuTimeMs <= 0.0f) {
			output.gpuBusyRatio = 0.0f;
			output.gpuContributionRatio = 0.0f;
			output.gpuTimingAligned = false;
			return;
		}

		// Critical v0.5 fix: classify against the *current smoothed pre-FG frame*,
		// not only the delayed frame that happened to own the timestamp query.
		const float contribution = std::clamp(
			output.smoothedGpuTimeMs / std::max(output.smoothedFrameTimeMs, 0.1f), 0.0f, 1.25f);
		output.gpuContributionRatio = contribution;
		output.gpuBusyRatio = contribution;

		if (output.smoothedFrameTimeMs <= upperDeadBandMs) {
			gpuBoundSamples = 0;
			mixedBoundSamples = 0;
			cpuBoundSamples = 0;
			if (output.boundState == BoundState::Cpu)
				output.boundState = BoundState::Unknown;
			return;
		}

		if (!output.gpuTimingAligned) {
			// Stale/delayed GPU evidence during a sudden camera spike must never promote
			// a GPU-bound state. Decay previous confidence and wait for aligned data.
			gpuBoundSamples = gpuBoundSamples > 0 ? gpuBoundSamples - 1 : 0;
			mixedBoundSamples = mixedBoundSamples > 0 ? mixedBoundSamples - 1 : 0;
			if (contribution <= kCpuBoundEnterRatio)
				++cpuBoundSamples;
			else
				cpuBoundSamples = cpuBoundSamples > 0 ? cpuBoundSamples - 1 : 0;
			if (settings.cpuGuard && cpuBoundSamples >= kCpuBoundConfirmationSamples) {
				output.boundState = BoundState::Cpu;
				ActivateCpuGuard(std::max(output.scale, probeStartScale));
			} else {
				output.boundState = BoundState::Unknown;
			}
			return;
		}

		const float gpuBudgetMs = settings.targetFrameTimeMs * settings.gpuHeadroom;
		const bool gpuOverBudget = output.smoothedGpuTimeMs > gpuBudgetMs * 1.02f;
		const bool gpuComfortable = output.smoothedGpuTimeMs < gpuBudgetMs * 0.90f;

		if (contribution >= kGpuBoundEnterRatio && gpuOverBudget) {
			++gpuBoundSamples;
			mixedBoundSamples = mixedBoundSamples > 0 ? mixedBoundSamples - 1 : 0;
			cpuBoundSamples = 0;
		} else if (contribution >= kMixedBoundEnterRatio && gpuOverBudget) {
			++mixedBoundSamples;
			gpuBoundSamples = gpuBoundSamples > 0 ? gpuBoundSamples - 1 : 0;
			cpuBoundSamples = cpuBoundSamples > 0 ? cpuBoundSamples - 1 : 0;
		} else if (contribution <= kCpuBoundEnterRatio && gpuComfortable) {
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
			const float gpuBudgetMs = settings.targetFrameTimeMs * settings.gpuHeadroom;
			if (output.smoothedGpuTimeMs > gpuBudgetMs * 1.02f)
				output.boundState = BoundState::Mixed;
			else {
				output.boundState = BoundState::Cpu;
				ActivateCpuGuard(probeStartScale);
			}
		} else if (pixelReduction > 0.025f && frameImprovement >= requiredImprovement) {
			output.boundState = output.gpuContributionRatio >= kGpuBoundEnterRatio ? BoundState::Gpu : BoundState::Mixed;
		}
	}

	void Controller::ActivateCpuGuard(float restoreScale)
	{
		output.cpuGuardActive = true;
		output.boundState = BoundState::Cpu;
		output.state = State::CpuLimited;
		output.pressureStableSeconds = 0.0f;
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
		case State::Disabled: return "Disabled";
		case State::Quality: return "Quality";
		case State::Target: return "Target";
		case State::Rescue: return "Rescue";
		case State::Emergency: return "Emergency";
		case State::CpuLimited: return "CPU Guard";
		case State::Stabilizing: return "Stabilizing";
		case State::Paused: return "Paused";
		}
		return "Unknown";
	}

	const char* Controller::GetBoundStateName(BoundState state)
	{
		switch (state) {
		case BoundState::Unknown: return "Learning";
		case BoundState::Gpu: return "GPU";
		case BoundState::Mixed: return "Mixed";
		case BoundState::Cpu: return "CPU";
		}
		return "Unknown";
	}
}

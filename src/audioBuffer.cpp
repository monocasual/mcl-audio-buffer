/* -----------------------------------------------------------------------------
 *
 * AudioBuffer
 *
 * -----------------------------------------------------------------------------
 *
 * Copyright (C) 2021 Giovanni A. Zuliani | Monocasual
 *
 * This file is part of AudioBuffer.
 *
 * AudioBuffer is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AudioBuffer is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AudioBuffer. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * -------------------------------------------------------------------------- */

#include "audioBuffer.hpp"
#include <algorithm>
#include <cassert>

namespace mcl
{
AudioBuffer::AudioBuffer()
: m_data(nullptr)
, m_size(0)
, m_channels(0)
, m_viewing(false)
{
}

/* -------------------------------------------------------------------------- */

AudioBuffer::AudioBuffer(int size, int channels)
: AudioBuffer()
{
	alloc(size, channels);
}

/* -------------------------------------------------------------------------- */

AudioBuffer::AudioBuffer(float* data, int size, int channels)
: m_data(data)
, m_size(size)
, m_channels(channels)
, m_viewing(true)
{
}

/* -------------------------------------------------------------------------- */

AudioBuffer::AudioBuffer(const AudioBuffer& o)
{
	copy(o);
}

/* -------------------------------------------------------------------------- */

AudioBuffer::AudioBuffer(AudioBuffer&& o) noexcept
{
	move(std::move(o));
}

/* -------------------------------------------------------------------------- */

AudioBuffer::~AudioBuffer()
{
	free();
}

/* -------------------------------------------------------------------------- */

AudioBuffer& AudioBuffer::operator=(const AudioBuffer& o)
{
	if (this == &o)
		return *this;
	copy(o);
	return *this;
}

/* -------------------------------------------------------------------------- */

AudioBuffer& AudioBuffer::operator=(AudioBuffer&& o) noexcept
{
	if (this == &o)
		return *this;
	move(std::move(o));
	return *this;
}

/* -------------------------------------------------------------------------- */

float* AudioBuffer::operator[](int offset) const
{
	assert(m_data != nullptr);
	assert(offset < m_size);
	return m_data.get() + (offset * m_channels);
}

/* -------------------------------------------------------------------------- */

void AudioBuffer::clear(int a, int b)
{
	if (m_data == nullptr)
		return;
	if (b == -1)
		b = m_size;
	std::fill_n(m_data.get() + (a * m_channels), (b - a) * m_channels, 0.0);
}

/* -------------------------------------------------------------------------- */

int  AudioBuffer::countFrames() const { return m_size; }
int  AudioBuffer::countSamples() const { return m_size * m_channels; }
int  AudioBuffer::countChannels() const { return m_channels; }
bool AudioBuffer::isAllocd() const { return m_data != nullptr; }

/* -------------------------------------------------------------------------- */

float AudioBuffer::getPeak(int channel, int a, int b) const
{
	assert(channel < m_channels);
	assert(a >= 0);
	assert(b == -1 || a < b);
	assert(b == -1 || b <= countFrames());

	if (b == -1)
		b = countFrames();

	float peak = 0.0f;
	for (int i = a; i < b; i++)
		peak = std::max(peak, (*this)[i][channel]);
	return peak;
}

/* -------------------------------------------------------------------------- */

void AudioBuffer::debug() const
{
	for (int i = 0; i < countFrames(); i++)
	{
		for (int k = 0; k < countChannels(); k++)
			printf("%f ", (*this)[i][k]);
		puts("");
	}
}

/* -------------------------------------------------------------------------- */

void AudioBuffer::alloc(int size, int channels)
{
	free();
	m_size     = size;
	m_channels = channels;
	m_data     = std::make_unique<float[]>(m_size * m_channels);
	clear();
}

/* -------------------------------------------------------------------------- */

void AudioBuffer::free()
{
	if (m_viewing)
		m_data.release();
	else
		m_data.reset();

	m_size     = 0;
	m_channels = 0;
	m_viewing  = false;
}

/* -------------------------------------------------------------------------- */

void AudioBuffer::sum(const AudioBuffer& b, int framesToCopy, int srcOffset,
    int destOffset, int srcChannel, int destChannel, float gain)
{
	merge<Operation::SUM>(b, framesToCopy, srcOffset, destOffset, srcChannel,
	    destChannel, gain);
}

void AudioBuffer::set(const AudioBuffer& b, int framesToCopy, int srcOffset,
    int destOffset, int srcChannel, int destChannel, float gain)
{
	merge<Operation::SET>(b, framesToCopy, srcOffset, destOffset, srcChannel,
	    destChannel, gain);
}

void AudioBuffer::sum(const AudioBuffer& b, int srcChannel, int destChannel, float gain)
{
	merge<Operation::SUM>(b, -1, 0, 0, srcChannel, destChannel, gain);
}

void AudioBuffer::set(const AudioBuffer& b, int srcChannel, int destChannel, float gain)
{
	merge<Operation::SET>(b, -1, 0, 0, srcChannel, destChannel, gain);
}

/* -------------------------------------------------------------------------- */

void AudioBuffer::sumAll(const AudioBuffer& b, int framesToCopy, int srcOffset, int destOffset,
    float gain)
{
	mergeAll<Operation::SUM>(b, framesToCopy, srcOffset, destOffset, gain);
}

void AudioBuffer::setAll(const AudioBuffer& b, int framesToCopy, int srcOffset, int destOffset,
    float gain)
{
	mergeAll<Operation::SET>(b, framesToCopy, srcOffset, destOffset, gain);
}

void AudioBuffer::sumAll(const AudioBuffer& b, float gain)
{
	mergeAll<Operation::SUM>(b, b.countFrames(), 0, 0, gain);
}

void AudioBuffer::setAll(const AudioBuffer& b, float gain)
{
	mergeAll<Operation::SET>(b, b.countFrames(), 0, 0, gain);
}

/* -------------------------------------------------------------------------- */

template <AudioBuffer::Operation O>
void AudioBuffer::merge(const AudioBuffer& b, int framesToCopy, int srcOffset,
    int destOffset, int srcChannel, int destChannel, float gain)
{
	assert(m_data != nullptr);
	assert(destOffset >= 0 && destOffset < m_size);
	assert(srcChannel >= 0 && srcChannel < b.countChannels());
	assert(destChannel >= 0 && destChannel < countChannels());

	/* Make sure the amount of frames to copy lies within the current buffer
	size. */

	framesToCopy = framesToCopy == -1 ? b.countFrames() : framesToCopy;
	framesToCopy = std::min(framesToCopy, m_size - destOffset);

	for (int destF = 0, srcF = srcOffset; destF < framesToCopy && destF < b.countFrames(); destF++, srcF++)
	{
		if constexpr (O == Operation::SUM)
			sum(destF + destOffset, destChannel, b[srcF][srcChannel] * gain);
		else
			set(destF + destOffset, destChannel, b[srcF][srcChannel] * gain);
	}
}

/* -------------------------------------------------------------------------- */

template <AudioBuffer::Operation O>
void AudioBuffer::mergeAll(const AudioBuffer& b, int framesToCopy, int srcOffset,
    int destOffset, float gain)
{
	for (int destCh = 0, srcCh = 0; destCh < countChannels(); destCh++, srcCh++)
	{
		if (srcCh == b.countChannels())
			srcCh = 0;
		if constexpr (O == Operation::SUM)
			sum(b, framesToCopy, srcOffset, destOffset, srcCh, destCh);
		else
			set(b, framesToCopy, srcOffset, destOffset, srcCh, destCh);
	}
}

/* -------------------------------------------------------------------------- */

void AudioBuffer::applyGain(float g, int a, int b)
{
	assert(a >= 0);
	assert(b < countSamples());

	if (b == -1)
	{
		b = countSamples();
		assert(a < b);
	}

	for (int i = a; i < b; i++)
		m_data.get()[i] *= g;
}

/* -------------------------------------------------------------------------- */

void AudioBuffer::sum(int f, int channel, float val) { (*this)[f][channel] += val; }
void AudioBuffer::set(int f, int channel, float val) { (*this)[f][channel] = val; }

/* -------------------------------------------------------------------------- */

void AudioBuffer::move(AudioBuffer&& o)
{
	m_data     = std::move(o.m_data);
	m_size     = o.m_size;
	m_channels = o.m_channels;
	m_viewing  = o.m_viewing;

	o.m_data     = nullptr;
	o.m_size     = 0;
	o.m_channels = 0;
	o.m_viewing  = false;
}

/* -------------------------------------------------------------------------- */

void AudioBuffer::copy(const AudioBuffer& o)
{
	m_data     = std::make_unique<float[]>(o.m_size * o.m_channels);
	m_size     = o.m_size;
	m_channels = o.m_channels;
	m_viewing  = o.m_viewing;

	std::copy(o.m_data.get(), o.m_data.get() + (o.m_size * o.m_channels), m_data.get());
}

/* -------------------------------------------------------------------------- */

void AudioBuffer::forEachFrame(std::function<void(float*, int)> f)
{
	for (int i = 0; i < countFrames(); i++)
		f((*this)[i], i);
}

/* -------------------------------------------------------------------------- */

void AudioBuffer::forEachChannel(int frame, std::function<void(float&, int)> f)
{
	assert(frame < m_size);

	for (int i = 0; i < countChannels(); i++)
		f((*this)[frame][i], i);
}

/* -------------------------------------------------------------------------- */

void AudioBuffer::forEachSample(std::function<void(float&, int)> f)
{
	for (int i = 0; i < countSamples(); i++)
		f(m_data.get()[i], i);
}

/* -------------------------------------------------------------------------- */

template void AudioBuffer::merge<AudioBuffer::Operation::SUM>(const AudioBuffer&, int, int, int, int, int, float);
template void AudioBuffer::merge<AudioBuffer::Operation::SET>(const AudioBuffer&, int, int, int, int, int, float);
template void AudioBuffer::mergeAll<AudioBuffer::Operation::SUM>(const AudioBuffer&, int, int, int, float);
template void AudioBuffer::mergeAll<AudioBuffer::Operation::SET>(const AudioBuffer&, int, int, int, float);
} // namespace mcl
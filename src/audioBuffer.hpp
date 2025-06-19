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

#ifndef MONOCASUAL_AUDIO_BUFFER_H
#define MONOCASUAL_AUDIO_BUFFER_H

#include <algorithm>
#include <cassert>
#include <functional>
#include <memory>

namespace mcl
{
/* AudioBuffer
A class that holds a buffer filled with audio data. */

class AudioBuffer
{
public:
	/* AudioBuffer (1)
	Creates an empty (and invalid) audio buffer. */

	constexpr AudioBuffer()
	: m_data(nullptr)
	, m_size(0)
	, m_channels(0)
	, m_viewing(false)
	{
	}

	/* ---------------------------------------------------------------------- */

	/* AudioBuffer (2)
	Creates an audio buffer and allocates memory for size * channels frames. */

	constexpr AudioBuffer(int size, int channels)
	: AudioBuffer()
	{
		alloc(size, channels);
	}

	/* ---------------------------------------------------------------------- */

	/* AudioBuffer (3)
	Creates an audio buffer out of a raw pointer. AudioBuffer created this way
	is instructed not to free the owned data on destruction. */

	constexpr AudioBuffer(float* data, int size, int channels)
	: m_data(data)
	, m_size(size)
	, m_channels(channels)
	, m_viewing(true)
	{
	}

	/* ---------------------------------------------------------------------- */

	/* AudioBuffer(const AudioBuffer&)
	Copy constructor. */

	constexpr AudioBuffer(const AudioBuffer& o)
	{
		copy(o);
	}

	/* ---------------------------------------------------------------------- */

	/* AudioBuffer(AudioBuffer&&)
	Move constructor. */

	constexpr AudioBuffer(AudioBuffer&& o) noexcept
	{
		move(std::move(o));
	}

	/* ---------------------------------------------------------------------- */

	/* ~AudioBuffer
	Destructor. */

	constexpr ~AudioBuffer()
	{
		free();
	}

	/* ---------------------------------------------------------------------- */

	/* operator = (const AudioBuffer& o)
	Copy assignment operator. */

	constexpr AudioBuffer& operator=(const AudioBuffer& o)
	{
		if (this == &o)
			return *this;
		copy(o);
		return *this;
	}

	/* ---------------------------------------------------------------------- */

	/* operator = (AudioBuffer&& o)
	Move assignment operator. */

	constexpr AudioBuffer& operator=(AudioBuffer&& o) noexcept
	{
		if (this == &o)
			return *this;
		move(std::move(o));
		return *this;
	}

	/* ---------------------------------------------------------------------- */

	/* operator []
	Given a frame 'offset', returns a pointer to it. This is useful for digging
	inside a frame, i.e. parsing each channel. How to use it:

	    for (int k=0; k<buffer->countFrames(), k++)
	        for (int i=0; i<buffer->countChannels(); i++)
	            ... buffer[k][i] ...

	Also note that buffer[0] will give you a pointer to the whole internal data
	array. */

	constexpr float* operator[](int offset) const
	{
		assert(m_data != nullptr);
		assert(offset < m_size);
		return m_data.get() + (offset * m_channels);
	}

	/* ---------------------------------------------------------------------- */

	constexpr int  countFrames() const { return m_size; }
	constexpr int  countSamples() const { return m_size * m_channels; }
	constexpr int  countChannels() const { return m_channels; }
	constexpr bool isAllocd() const { return m_data != nullptr; }

	/* ---------------------------------------------------------------------- */

	/* getPeak
	Returns the highest value from the specified channel. */

	constexpr float getPeak(int channel, int a = 0, int b = -1) const
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

	/* ---------------------------------------------------------------------- */

	constexpr void debug() const
	{
		for (int i = 0; i < countFrames(); i++)
		{
			for (int k = 0; k < countChannels(); k++)
				printf("%f ", (*this)[i][k]);
			puts("");
		}
	}

	/* ---------------------------------------------------------------------- */

	constexpr void alloc(int size, int channels)
	{
		free();
		m_size     = size;
		m_channels = channels;
		m_data     = std::make_unique<float[]>(m_size * m_channels);
		clear();
	}

	/* ---------------------------------------------------------------------- */

	constexpr void free()
	{
		if (m_viewing)
			m_data.release();
		else
			m_data.reset();

		m_size     = 0;
		m_channels = 0;
		m_viewing  = false;
	}

	/* ---------------------------------------------------------------------- */

	/* sum, set (1)
	Merges (sum) or copies (set) chunks of data from buffer 'b' onto this one.
	framesToCopy - how many frames to grab from 'b'
	srcOffset - the frame offset where to read from 'b'
	destOffset - the frame offset where to put data read from 'b'
	srcChannel - the channel within the source buffer to read from
	destChannel - the channel within this buffer to add the samples to. */

	constexpr void sum(const AudioBuffer& b, int framesToCopy, int srcOffset,
	    int destOffset, int srcChannel, int destChannel, float gain = 1.0f)
	{
		merge<Operation::SUM>(b, framesToCopy, srcOffset, destOffset, srcChannel,
		    destChannel, gain);
	}

	constexpr void set(const AudioBuffer& b, int framesToCopy, int srcOffset,
	    int destOffset, int srcChannel, int destChannel, float gain = 1.0f)
	{
		merge<Operation::SET>(b, framesToCopy, srcOffset, destOffset, srcChannel,
		    destChannel, gain);
	}

	/* ---------------------------------------------------------------------- */

	/* sum, set (2)
	Same as sum, set (1) without boundaries or offsets: it just copies as much
	as possibile. */

	constexpr void sum(const AudioBuffer& b, int srcChannel, int destChannel, float gain = 1.0f)
	{
		merge<Operation::SUM>(b, -1, 0, 0, srcChannel, destChannel, gain);
	}

	/* ---------------------------------------------------------------------- */

	constexpr void set(const AudioBuffer& b, int srcChannel, int destChannel, float gain = 1.0f)
	{
		merge<Operation::SET>(b, -1, 0, 0, srcChannel, destChannel, gain);
	}

	/* ---------------------------------------------------------------------- */

	/* sumAll, setAll (1)
	Merge or sum all channels of 'b' onto this one. Channels in 'b' are spread
	over this one in case it has less channels. */

	constexpr void sumAll(const AudioBuffer& b, int framesToCopy, int srcOffset, int destOffset,
	    float gain = 1.0f)
	{
		mergeAll<Operation::SUM>(b, framesToCopy, srcOffset, destOffset, gain);
	}

	constexpr void setAll(const AudioBuffer& b, int framesToCopy, int srcOffset, int destOffset,
	    float gain = 1.0f)
	{
		mergeAll<Operation::SET>(b, framesToCopy, srcOffset, destOffset, gain);
	}

	/* ---------------------------------------------------------------------- */

	/* sumAll, setAll (2)
	Same as sumAll, setAll (1) without boundaries or offsets: it just copies as
	much as possibile. */

	constexpr void sumAll(const AudioBuffer& b, float gain = 1.0f)
	{
		mergeAll<Operation::SUM>(b, b.countFrames(), 0, 0, gain);
	}

	constexpr void setAll(const AudioBuffer& b, float gain = 1.0f)
	{
		mergeAll<Operation::SET>(b, b.countFrames(), 0, 0, gain);
	}

	/* ---------------------------------------------------------------------- */

	/* clear
	Clears the internal data by setting all bytes to 0.0f. Optional parameters
	'a' and 'b' set the range. */

	constexpr void clear(int a = 0, int b = -1)
	{
		if (m_data == nullptr)
			return;
		if (b == -1)
			b = m_size;
		std::fill_n(m_data.get() + (a * m_channels), (b - a) * m_channels, 0.0);
	}

	/* ---------------------------------------------------------------------- */

	/* applyGain
	Applies gain 'g' to buffer. Optional parameters	'a' and 'b' set the range.*/

	constexpr void applyGain(float g, int a = 0, int b = -1)
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

	/* ---------------------------------------------------------------------- */

	/* forEachFrame
	Applies a function to each frame in the audio buffer. */

	void forEachFrame(std::function<void(float* /*channels*/, int /*numFrame*/)> f)
	{
		for (int i = 0; i < countFrames(); i++)
			f((*this)[i], i);
	}

	/* ---------------------------------------------------------------------- */

	/* forEachChannel
	Applies a function to each channel in the given frame. */

	void forEachChannel(int frame, std::function<void(float& /*value*/, int /*numChannel*/)> f)
	{
		assert(frame < m_size);

		for (int i = 0; i < countChannels(); i++)
			f((*this)[frame][i], i);
	}

	/* ---------------------------------------------------------------------- */

	/* forEachSample
	Applies a function to each sample in the audio buffer. */

	void forEachSample(std::function<void(float& /*value*/, int /*numSample*/)> f)
	{
		for (int i = 0; i < countSamples(); i++)
			f(m_data.get()[i], i);
	}

	/* ---------------------------------------------------------------------- */

private:
	enum class Operation
	{
		SUM,
		SET
	};

	template <Operation O>
	constexpr void merge(const AudioBuffer& b, int framesToCopy, int srcOffset, int destOffset,
	    int srcChannel, int destChannel, float gain)
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

	/* ---------------------------------------------------------------------- */

	template <Operation O = Operation::SET>
	constexpr void mergeAll(const AudioBuffer& b, int framesToCopy, int srcOffset, int destOffset,
	    float gain)
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

	/* ---------------------------------------------------------------------- */

	constexpr void move(AudioBuffer&& o)
	{
		m_data     = std::exchange(o.m_data, nullptr);
		m_size     = std::exchange(o.m_size, 0);
		m_channels = std::exchange(o.m_channels, 0);
		m_viewing  = std::exchange(o.m_viewing, false);
	}

	/* ---------------------------------------------------------------------- */

	constexpr void copy(const AudioBuffer& o)
	{
		m_data     = std::make_unique<float[]>(o.m_size * o.m_channels);
		m_size     = o.m_size;
		m_channels = o.m_channels;
		m_viewing  = o.m_viewing;

		std::copy(o.m_data.get(), o.m_data.get() + (o.m_size * o.m_channels), m_data.get());
	}

	/* ---------------------------------------------------------------------- */

	constexpr void sum(int f, int channel, float val) { (*this)[f][channel] += val; }
	constexpr void set(int f, int channel, float val) { (*this)[f][channel] = val; }

	std::unique_ptr<float[]> m_data;
	int                      m_size;
	int                      m_channels;
	bool                     m_viewing;
};
} // namespace mcl

#endif
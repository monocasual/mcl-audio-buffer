/* -----------------------------------------------------------------------------
 *
 * AudioBuffer
 *
 * -----------------------------------------------------------------------------
 *
 * Copyright (C) 2021-2026 Giovanni A. Zuliani | Monocasual
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
#include <array>
#include <cassert>
#include <functional>
#include <mdspan>
#include <memory>
#include <span>
#include <utility>

namespace mcl
{
/* AudioBuffer
A class that holds a buffer filled with audio data. The buffer uses planar layout:
samples are grouped by channel, so all frames for channel 0 come first, then all
frames for channel 1, and so on.

Example with 2 channels and 3 frames:
    channel 0: [f0, f1, f2]
    channel 1: [f0, f1, f2]

So the underlying memory order is:
    [ch0_f0, ch0_f1, ch0_f2, ch1_f0, ch1_f1, ch1_f2]

Access examples:
    buffer.at(frame, channel) = 0.5f;
    buffer.at(0, 1) = 1.0f;  // first frame of channel 1 */

class AudioBuffer
{
public:
	using ChannelView      = std::span<float>;
	using ConstChannelView = std::span<const float>;

	/* Extents
	The shape of an mdspan (used in getDataView): how many channels, how many
	frames. Both are decided at runtime (that's what dynamic_extent means), since
	they depend on the buffer's actual size. */

	using Extents = std::extents<std::size_t, std::dynamic_extent, std::dynamic_extent>;

	/* DataView
	A view over the internal data array that still knows the buffer's real
	per-channel size internally (that's what layout_stride is for).
	This means a "view" over a shorter range of frames still reads/writes the
	right memory, because it never confuses "how many frames I can see" with
	"how far apart the channels really are". */

	using DataView      = std::mdspan<float, Extents, std::layout_stride>;
	using ConstDataView = std::mdspan<const float, Extents, std::layout_stride>;

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
	Creates an audio buffer and allocates memory for size * channels samples. */

	constexpr AudioBuffer(int size, int channels)
	: AudioBuffer()
	{
		alloc(size, channels);
	}

	/* ---------------------------------------------------------------------- */

	/* AudioBuffer (3)
	Creates a non-owning audio buffer view over raw planar data. The buffer does
	not take ownership of the provided memory and will not free it on destruction. */

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

	constexpr int  countFrames() const { return m_size; }
	constexpr int  countSamples() const { return m_size * m_channels; }
	constexpr int  countChannels() const { return m_channels; }
	constexpr bool isAllocd() const { return m_data != nullptr; }

	/* ---------------------------------------------------------------------- */

	/* at
	Returns a reference to the sample at the given frame and channel. A reference
	is used instead of a pointer so the caller can read/write the sample directly,
	just like with array indexing.
	Example:

	    buffer.at(frame, channel) = 0.5f; */

	constexpr float& at(int frame, int channel)
	{
		assertSample(frame, channel);
		return m_data.get()[channel * m_size + frame];
	}

	constexpr const float& at(int frame, int channel) const
	{
		assertSample(frame, channel);
		return m_data.get()[channel * m_size + frame];
	}

	/* ---------------------------------------------------------------------- */

	/* getChannel
	Returns the frames belonging to one channel, as a span. */

	constexpr ChannelView getChannelView(int channel)
	{
		assertChannel(channel);
		return std::span<float>(m_data.get() + (channel * m_size), m_size);
	}

	constexpr ConstChannelView getChannelView(int channel) const
	{
		assertChannel(channel);
		return std::span<const float>(m_data.get() + (channel * m_size), m_size);
	}

	/* ---------------------------------------------------------------------- */

	/* getData
	Returns a view over frames [begin, end), covering every channel. Calling it
	with no arguments (or end left at 0) returns a view over the whole buffer. */

	constexpr ConstDataView getDataView(std::size_t begin = 0, std::size_t end = 0) const
	{
		// TODO - use std::size_t everywhere
		if (end == 0)
			end = static_cast<std::size_t>(m_size);

		assert(begin <= end);
		assert(end <= static_cast<std::size_t>(m_size));

		const Extents                              shape(m_channels, end - begin);
		const std::array<std::size_t, 2>           strides{static_cast<std::size_t>(m_size), 1};
		const std::layout_stride::mapping<Extents> mapping(shape, strides);

		return ConstDataView(m_data.get() + begin, mapping);
	}

	constexpr DataView getDataView(std::size_t begin = 0, std::size_t end = 0)
	{
		const ConstDataView view = std::as_const(*this).getDataView(begin, end);
		return DataView(const_cast<float*>(view.data_handle()), view.mapping());
	}

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
			peak = std::max(peak, at(i, channel));
		return peak;
	}

	/* ---------------------------------------------------------------------- */

	constexpr void debug() const
	{
		for (int i = 0; i < countFrames(); i++)
		{
			for (int k = 0; k < countChannels(); k++)
				printf("%f ", at(i, k));
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

	/* resizeChannels
	Changes the number of channels. Note: it will free and reallocate data. */

	constexpr void resizeChannels(int newChannels)
	{
		alloc(m_size, newChannels);
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
		mergeAll<Operation::SUM>(b, framesToCopy, srcOffset, destOffset, gain, std::array<float, 0>{});
	}

	constexpr void setAll(const AudioBuffer& b, int framesToCopy, int srcOffset, int destOffset,
	    float gain = 1.0f)
	{
		mergeAll<Operation::SET>(b, framesToCopy, srcOffset, destOffset, gain, std::array<float, 0>{});
	}

	/* ---------------------------------------------------------------------- */

	/* sumAll, setAll (2)
	Same as sumAll, setAll (1) without boundaries or offsets: it just copies as
	much as possibile. */

	constexpr void sumAll(const AudioBuffer& b, float gain = 1.0f)
	{
		mergeAll<Operation::SUM>(b, b.countFrames(), 0, 0, gain, std::array<float, 0>{});
	}

	constexpr void setAll(const AudioBuffer& b, float gain = 1.0f)
	{
		mergeAll<Operation::SET>(b, b.countFrames(), 0, 0, gain, std::array<float, 0>{});
	}

	/* ---------------------------------------------------------------------- */

	/* sumAll, setAll (2)
	Same as sumAll, setAll (2) with an extra 'pan' parameter, to apply panning
	to the destination buffer (aka this one) while merging data. Note: the
	pan array must have exactly b.countChannels() element in it. */

	template <std::size_t panSize>
	constexpr void sumAll(const AudioBuffer& b, std::array<float, panSize> pan, float gain = 1.0f)
	{
		mergeAll<Operation::SUM>(b, b.countFrames(), 0, 0, gain, pan);
	}

	template <std::size_t panSize>
	constexpr void setAll(const AudioBuffer& b, std::array<float, panSize> pan, float gain = 1.0f)
	{
		mergeAll<Operation::SET>(b, b.countFrames(), 0, 0, gain, pan);
	}

	/* ---------------------------------------------------------------------- */

	/* clear
	Clears the internal data by setting all samples to 0.0f. Optional parameters
	'a' and 'b' set the range. */

	constexpr void clear(int a = 0, int b = -1)
	{
		if (m_data == nullptr)
			return;
		if (b == -1)
			b = m_size;
		for (int channel = 0; channel < m_channels; ++channel)
			std::fill_n(m_data.get() + (channel * m_size) + a, b - a, 0.0);
	}

	/* ---------------------------------------------------------------------- */

	/* applyGain
	Applies gain 'g' to buffer. Optional parameters	'a' and 'b' set the range.*/

	constexpr void applyGain(float g, int a = 0, int b = -1)
	{
		assert(a >= 0);
		assert(a <= countFrames());
		assert(b == -1 || b <= countFrames());
		assert(b == -1 || a <= b);

		if (b == -1)
			b = countFrames();

		for (int channel = 0; channel < m_channels; ++channel)
			for (int frame = a; frame < b; ++frame)
				at(frame, channel) *= g;
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
				sum(destF + destOffset, destChannel, b.at(srcF, srcChannel) * gain);
			else
				set(destF + destOffset, destChannel, b.at(srcF, srcChannel) * gain);
		}
	}

	/* ---------------------------------------------------------------------- */

	template <Operation O, std::size_t PanSize>
	constexpr void mergeAll(const AudioBuffer& b, int framesToCopy, int srcOffset, int destOffset,
	    float gain, std::array<float, PanSize> pan)
	{
		if constexpr (PanSize > 0)
			assert(pan.size() == static_cast<std::size_t>(b.countChannels()));

		for (int destCh = 0, srcCh = 0; destCh < countChannels(); destCh++, srcCh++)
		{
			if (srcCh == b.countChannels())
				srcCh = 0;
			const float chanGain = pan.size() == 0 ? gain : gain * pan[srcCh];
			if constexpr (O == Operation::SUM)
				sum(b, framesToCopy, srcOffset, destOffset, srcCh, destCh, chanGain);
			else
				set(b, framesToCopy, srcOffset, destOffset, srcCh, destCh, chanGain);
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

	constexpr void sum(int f, int channel, float val) { at(f, channel) += val; }
	constexpr void set(int f, int channel, float val) { at(f, channel) = val; }

	/* ---------------------------------------------------------------------- */

	constexpr void assertSample(int frame, int channel) const
	{
		assert(m_data != nullptr);
		assert(frame < m_size);
		assert(channel < m_channels);
	}

	/* ---------------------------------------------------------------------- */

	constexpr void assertChannel(int channel) const
	{
		assert(channel >= 0);
		assert(channel < m_channels);
	}

	/* ---------------------------------------------------------------------- */

	std::unique_ptr<float[]> m_data;
	int                      m_size;
	int                      m_channels;
	bool                     m_viewing;
};
} // namespace mcl

#endif
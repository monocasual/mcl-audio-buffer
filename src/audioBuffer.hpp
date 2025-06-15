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

#include <array>
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

	AudioBuffer();

	/* AudioBuffer (2)
	Creates an audio buffer and allocates memory for size * channels frames. */

	AudioBuffer(int size, int channels);

	/* AudioBuffer (3)
	Creates an audio buffer out of a raw pointer. AudioBuffer created this way
	is instructed not to free the owned data on destruction. */

	AudioBuffer(float* data, int size, int channels);

	/* AudioBuffer(const AudioBuffer&)
	Copy constructor. */

	AudioBuffer(const AudioBuffer& o);

	/* AudioBuffer(AudioBuffer&&)
	Move constructor. */

	AudioBuffer(AudioBuffer&& o) noexcept;

	/* ~AudioBuffer
	Destructor. */

	~AudioBuffer();

	/* operator = (const AudioBuffer& o)
	Copy assignment operator. */

	AudioBuffer& operator=(const AudioBuffer& o);

	/* operator = (AudioBuffer&& o)
	Move assignment operator. */

	AudioBuffer& operator=(AudioBuffer&& o) noexcept;

	/* operator []
	Given a frame 'offset', returns a pointer to it. This is useful for digging
	inside a frame, i.e. parsing each channel. How to use it:

	    for (int k=0; k<buffer->countFrames(), k++)
	        for (int i=0; i<buffer->countChannels(); i++)
	            ... buffer[k][i] ...

	Also note that buffer[0] will give you a pointer to the whole internal data
	array. */

	float* operator[](int offset) const;

	int  countFrames() const;
	int  countSamples() const;
	int  countChannels() const;
	bool isAllocd() const;

	/* getPeak
	Returns the highest value from the specified channel. */

	float getPeak(int channel, int a = 0, int b = -1) const;

	void alloc(int size, int channels);
	void free();

	/* sum, set (1)
	Merges (sum) or copies (set) 'framesToCopy' frames of buffer 'b' onto this
	one. If 'framesToCopy' is -1 the whole buffer will be copied. If 'b' has
	less channels than this one, they will be spread over the current ones.
	Buffer 'b' MUST NOT contain more channels than this one. */

	void sum(const AudioBuffer& b, int framesToCopy = -1, int srcOffset = 0,
	    int destOffset = 0, float gain = 1.0f);
	void set(const AudioBuffer& b, int framesToCopy = -1, int srcOffset = 0,
	    int destOffset = 0, float gain = 1.0f);

	/* sum, set (2)
	Same as sum, set (1) without boundaries or offsets: it just copies as much
	as possibile. */

	void sum(const AudioBuffer& b, float gain = 1.0f);
	void set(const AudioBuffer& b, float gain = 1.0f);

	/* clear
	Clears the internal data by setting all bytes to 0.0f. Optional parameters
	'a' and 'b' set the range. */

	void clear(int a = 0, int b = -1);

	/* applyGain
	Applies gain 'g' to buffer. Optional parameters	'a' and 'b' set the range.*/

	void applyGain(float g, int a = 0, int b = -1);

	/* forEachFrame
	Applies a function to each frame in the audio buffer. */

	void forEachFrame(std::function<void(float* /*channels*/, int /*numFrame*/)>);

	/* forEachChannel
	Applies a function to each channel in the given frame. */

	void forEachChannel(int frame, std::function<void(float& /*value*/, int /*numChannel*/)>);

	/* forEachSample
	Applies a function to each sample in the audio buffer. */

	void forEachSample(std::function<void(float& /*value*/, int /*numSample*/)>);

private:
	enum class Operation
	{
		SUM,
		SET
	};

	template <Operation O = Operation::SET>
	void copyData(const AudioBuffer& b, int framesToCopy = -1,
	    int srcOffset = 0, int destOffset = 0, float gain = 1.0f);

	void move(AudioBuffer&& o);
	void copy(const AudioBuffer& o);
	void sum(int f, int channel, float val);
	void set(int f, int channel, float val);

	std::unique_ptr<float[]> m_data;
	int                      m_size;
	int                      m_channels;
	bool                     m_viewing;
};
} // namespace mcl

#endif
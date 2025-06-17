#include "src/audioBuffer.hpp"
#include <catch2/catch_test_macros.hpp>
#include <iostream>

using namespace mcl;

/*
fillBufferWithData
Fill AudioBuffer with fake data, like so:

channel 0: 0, 1,   ... b.countFrames() - 1
channel 1: 1, 2,   ... b.countFrames()
...
channel n: n, n+1, ... b.countFrames() + n */

void fillBufferWithData(AudioBuffer& b)
{
	for (int i = 0; i < b.countFrames(); i++)
		for (int k = 0; k < b.countChannels(); k++)
			b[i][k] = static_cast<float>(i + k);
}

void testCopy(AudioBuffer& a, AudioBuffer& b)
{
	REQUIRE(a.isAllocd() == true);
	REQUIRE(b.isAllocd() == true);
	REQUIRE(a.countFrames() == b.countFrames());
	REQUIRE(a.countSamples() == b.countSamples());
	REQUIRE(a.countChannels() == b.countChannels());
}

void testMove(AudioBuffer& a, AudioBuffer& b, int sourceBufferSize, int sourceNumChannels)
{
	REQUIRE(b.isAllocd() == false);
	REQUIRE(b.countFrames() == 0);
	REQUIRE(b.countSamples() == 0);
	REQUIRE(b.countChannels() == 0);

	REQUIRE(a.isAllocd() == true);
	REQUIRE(a.countFrames() == sourceBufferSize);
	REQUIRE(a.countSamples() == sourceBufferSize * sourceNumChannels);
	REQUIRE(a.countChannels() == sourceNumChannels);

	for (int i = 0; i < a.countFrames(); i++)
		for (int k = 0; k < a.countChannels(); k++)
			REQUIRE(a[i][k] == static_cast<float>(i + k));
}

TEST_CASE("AudioBuffer")
{

	static const int BUFFER_SIZE = 4096;

	AudioBuffer buffer;
	buffer.alloc(BUFFER_SIZE, 2);

	fillBufferWithData(buffer);

	SECTION("test allocation")
	{
		SECTION("test mono")
		{
			buffer.alloc(BUFFER_SIZE, 1);
			REQUIRE(buffer.countFrames() == BUFFER_SIZE);
			REQUIRE(buffer.countSamples() == BUFFER_SIZE);
			REQUIRE(buffer.countChannels() == 1);
		}

		SECTION("test stereo")
		{
			REQUIRE(buffer.countFrames() == BUFFER_SIZE);
			REQUIRE(buffer.countSamples() == BUFFER_SIZE * 2);
			REQUIRE(buffer.countChannels() == 2);
		}

		buffer.free();

		REQUIRE(buffer.countFrames() == 0);
		REQUIRE(buffer.countSamples() == 0);
		REQUIRE(buffer.countChannels() == 0);
	}

	SECTION("test clear all")
	{
		buffer.clear();
		for (int i = 0; i < buffer.countFrames(); i++)
			for (int k = 0; k < buffer.countChannels(); k++)
				REQUIRE(buffer[i][k] == 0.0f);
		buffer.free();
	}

	SECTION("test clear range")
	{
		for (int i = 0; i < buffer.countFrames(); i++)
			for (int k = 0; k < buffer.countChannels(); k++)
				buffer[i][k] = 1.0f;

		buffer.clear(5, 6);

		for (int k = 0; k < buffer.countChannels(); k++)
			REQUIRE(buffer[5][k] == 0.0f);

		buffer.free();
	}

	SECTION("test copy")
	{
		constexpr int numChannels = 2;

		SECTION("test full copy with copy constructor")
		{
			AudioBuffer other(buffer);

			testCopy(buffer, other);
		}

		SECTION("test full copy with copy assignment")
		{
			AudioBuffer other(BUFFER_SIZE, numChannels);

			other = buffer;

			testCopy(buffer, other);
		}

		SECTION("test full copy with set()")
		{
			AudioBuffer other(BUFFER_SIZE, numChannels);

			other.set(buffer, 0, 0, 1.0f);

			testCopy(buffer, other);
		}
	}

	SECTION("test move")
	{
		constexpr int numChannels = 2;

		SECTION("with move operator")
		{
			AudioBuffer b(BUFFER_SIZE, numChannels); // Filled with 0.0f's by default
			fillBufferWithData(b);

			AudioBuffer a(std::move(b));

			testMove(a, b, BUFFER_SIZE, numChannels);
		}

		SECTION("with move assignment")
		{

			AudioBuffer a(BUFFER_SIZE, numChannels); // Filled with 0.0f's by default
			AudioBuffer b(BUFFER_SIZE, numChannels);

			fillBufferWithData(b);

			a = std::move(b);

			testMove(a, b, BUFFER_SIZE, numChannels);
		}
	}

	SECTION("test view")
	{
		constexpr int bufferSize  = 1024;
		constexpr int numChannels = 1;

		std::unique_ptr<float[]> raw = std::make_unique<float[]>(bufferSize);
		AudioBuffer              buf(raw.get(), bufferSize, numChannels);
	}

	SECTION("test set")
	{
		constexpr int numChannels = 2;

		AudioBuffer src(BUFFER_SIZE, numChannels); // Filled with 0.0f's by default
		AudioBuffer dest(BUFFER_SIZE, numChannels);
		fillBufferWithData(src);

		SECTION("total")
		{
			dest.set(src, 0, 0);
			dest.set(src, 1, 1);

			for (int i = 0; i < dest.countFrames(); i++)
				for (int k = 0; k < dest.countChannels(); k++)
					REQUIRE(dest[i][k] == static_cast<float>(i + k));
		}

		SECTION("partial")
		{
			constexpr int framesToCopy = BUFFER_SIZE / 2;

			dest.set(src, framesToCopy, 0, 0, 0, 0);
			dest.set(src, framesToCopy, 0, 0, 1, 1);

			for (int i = 0; i < dest.countFrames(); i++)
				for (int k = 0; k < dest.countChannels(); k++)
					REQUIRE(dest[i][k] == (i < framesToCopy ? static_cast<float>(i + k) : 0.0f));
		}

		SECTION("partial with offset")
		{
			constexpr int framesToCopy = BUFFER_SIZE / 2;
			constexpr int offset       = 1;

			dest.set(src, framesToCopy, offset, offset, 0, 0);
			dest.set(src, framesToCopy, offset, offset, 1, 1);

			for (int i = 0; i < dest.countFrames(); i++)
				for (int k = 0; k < dest.countChannels(); k++)
				{
					if (i == 0)
						REQUIRE(dest[i][k] == 0.0f);
					else
						REQUIRE(dest[i][k] == (i < framesToCopy + offset ? static_cast<float>(i + k) : 0.0f));
				}
		}
	}

	SECTION("test setAll")
	{
		constexpr int numChannels  = 2;
		constexpr int framesToCopy = BUFFER_SIZE;
		constexpr int offset       = 0;

		AudioBuffer src(BUFFER_SIZE, numChannels - 1);
		AudioBuffer dest(BUFFER_SIZE, numChannels); // Empty
		fillBufferWithData(src);

		dest.setAll(src, framesToCopy, offset, offset);

		for (int i = 0; i < dest.countFrames(); i++)
			for (int k = 0; k < dest.countChannels(); k++)
				REQUIRE(dest[i][k] == static_cast<float>(i));
	}
}

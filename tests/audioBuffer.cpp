#include "src/audioBuffer.hpp"
#include <catch2/catch_test_macros.hpp>
#include <iostream>

using namespace mcl;

/* 
fillBufferWithData
Fill AudioBuffer with fake data 0...b.countFrames() - 1 */

void fillBufferWithData(AudioBuffer& b)
{
	b.forEachFrame([](float* channels, int numFrame) {
		channels[0] = static_cast<float>(numFrame);
		channels[1] = static_cast<float>(numFrame);
	});
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
		AudioBuffer other(BUFFER_SIZE, 2);

		SECTION("test full copy with copy constructor")
		{
			other = buffer;

			REQUIRE(other[0][0] == 0.0f);
			REQUIRE(other[16][0] == 16.0f);
			REQUIRE(other[128][0] == 128.0f);
			REQUIRE(other[1024][0] == 1024.0f);
			REQUIRE(other[BUFFER_SIZE - 1][0] == static_cast<float>(BUFFER_SIZE - 1));
		}

		SECTION("test full copy with set()")
		{
			other.set(buffer, 1.0f);

			REQUIRE(other[0][0] == 0.0f);
			REQUIRE(other[16][0] == 16.0f);
			REQUIRE(other[128][0] == 128.0f);
			REQUIRE(other[1024][0] == 1024.0f);
			REQUIRE(other[BUFFER_SIZE - 1][0] == static_cast<float>(BUFFER_SIZE - 1));
		}
	}

	SECTION("test move")
	{
		AudioBuffer other(BUFFER_SIZE, 2); // Filled with 0.0f's by default

		other = std::move(buffer);

		REQUIRE(buffer.isAllocd() == false);
		REQUIRE(buffer.countFrames() == 0);
		REQUIRE(buffer.countSamples() == 0);
		REQUIRE(buffer.countChannels() == 0);

		REQUIRE(other.isAllocd() == true);
		REQUIRE(other.countFrames() == BUFFER_SIZE);
		REQUIRE(other.countSamples() == BUFFER_SIZE * 2);
		REQUIRE(other.countChannels() == 2);

		other.forEachFrame([](float* channels, int numFrame) {
			REQUIRE(channels[0] == static_cast<float>(numFrame));
			REQUIRE(channels[1] == static_cast<float>(numFrame));
		});
	}
}

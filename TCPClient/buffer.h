#pragma once

#include <vector>

class Buffer
{
public:
	Buffer(int size)
	{
		m_BufferData.resize(size);
		m_WriteIndex = 0;
		m_ReadIndex = 0;
	}
	~Buffer() { }

	uint32_t ReadUInt32LE()
	{
		uint32_t value = 0;

		value |= m_BufferData[m_ReadIndex++];
		value |= m_BufferData[m_ReadIndex++] << 8;
		value |= m_BufferData[m_ReadIndex++] << 16;
		value |= m_BufferData[m_ReadIndex++] << 24;

		return value;
	}

	void WriteUInt32LE(uint32_t value)
	{
		m_BufferData[m_WriteIndex++] = value;
		m_BufferData[m_WriteIndex++] = value >> 8;
		m_BufferData[m_WriteIndex++] = value >> 16;
		m_BufferData[m_WriteIndex++] = value >> 24;
	}

	uint32_t ReadUInt32BE()
	{
		uint32_t value = 0;

		value |= m_BufferData[m_ReadIndex++] << 24;
		value |= m_BufferData[m_ReadIndex++] << 16;
		value |= m_BufferData[m_ReadIndex++] << 8;
		value |= m_BufferData[m_ReadIndex++];

		return value;
	}

	void WriteUInt32BE(uint32_t value)
	{
		m_BufferData[m_WriteIndex++] = value >> 24;
		m_BufferData[m_WriteIndex++] = value >> 16;
		m_BufferData[m_WriteIndex++] = value >> 8;
		m_BufferData[m_WriteIndex++] = value;
	}




	uint16_t ReadUInt16LE()
	{
		uint16_t value = 0;

		value |= m_BufferData[m_ReadIndex++];
		value |= m_BufferData[m_ReadIndex++] << 8;

		return value;
	}

	void WriteUInt16LE(uint16_t value)
	{
		m_BufferData[m_WriteIndex++] = value;
		m_BufferData[m_WriteIndex++] = value >> 8;
	}

	uint16_t ReadUInt16BE()
	{
		uint16_t value = 0;

		value |= m_BufferData[m_ReadIndex++] << 8;
		value |= m_BufferData[m_ReadIndex++];

		return value;
	}

	void WriteUInt16BE(uint16_t value)
	{
		m_BufferData[m_WriteIndex++] = value >> 8;
		m_BufferData[m_WriteIndex++] = value;
	}




	void WriteString(const std::string& str)
	{
		int strLength = str.length();
		for (int i = 0; i < strLength; i++)
		{
			m_BufferData[m_WriteIndex++] = str[i];
		}
	}

	std::string ReadString(uint32_t length)
	{
		std::string str;
		for (int i = 0; i < length; i++)
		{
			str.push_back(m_BufferData[m_ReadIndex++]);
		}
		return str;
	}

	std::vector<uint8_t> m_BufferData;
	int m_WriteIndex;
	int m_ReadIndex;
};
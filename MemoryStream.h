#ifndef CS422_MEMORYSTREAMH
#define CS422_MEMORYSTREAMH

#include "Stream.hpp"

namespace CS422
{
	class MemoryStream : public Stream
	{
	protected:
		int m_allocSize;
		int m_len;
		bool m_ownPtr;
		i64 m_pos;
		u8* m_ptr;
		bool m_isFixedSize;

		static void Cpy(u8* dst, const u8* src, int count)
		{
			while (count--)
			{
				*dst++ = *src++;
			}
		}

	public:
		MemoryStream() : MemoryStream(1024) { }
		
		// Creates a memory stream with the initial allocation size
		MemoryStream(int InitialAllocationSize)
		{
			m_allocSize = InitialAllocationSize;
            m_len = 0;
			m_pos = 0;
			m_ownPtr = true;
			m_isFixedSize = false;

			m_ptr = new u8[InitialAllocationSize];
			if (!m_ptr)
			{
				m_allocSize = 0;
			}
		}

		virtual ~MemoryStream()
		{
			if (m_ownPtr && m_ptr)
			{
				delete [] m_ptr;
				m_ptr = 0;
			}
		}

		// Returns a boolean indicating whether or not this stream can be read from.
		virtual bool CanRead()
		{
			return true;
		}

		// Returns a boolean indicating whether or seeking is allowed with this stream.
		virtual bool CanSeek()
		{
			return true;
		}

		// Returns a boolean indicating whether or not this stream can be written to.
		virtual bool CanWrite()
		{
			return true;
		}

		// Returns a 64-bit signed integer indicating the total size of the stream,
		// in bytes. Any negative values returned indicate an error.
		virtual i64 GetLength() const
		{
			return m_len;
		}

		// Returns a pointer to the beginning of the stream data. The entire memory stream is 
		// stored in one contiguous buffer.
		unsigned char* GetPointer()
		{
			return m_ptr;
		}
	
		virtual i64 GetPosition() const
		{
			return m_pos;
		}

		bool IsFixedSize()
		{
			return m_isFixedSize;
		}

		// Attempts to read "byteCount" bytes from the stream starting at the current position.
		// Returns the number of bytes successfully read on success. After a successful read 
		// the position of the stream will also be increased by the number of bytes read.
		//
		// Returns 0 if the position was at the end of the stream at the start of the read and 
		// no bytes were read. A return value of 0 must always imply end of stream and not an 
		// actual error.
		//
		// Returns a negative value if an error occurs. The negative-valued error codes will be 
		// specific to the stream type.
		virtual int Read(void* Buf, int ByteCount)
		{
			if (!m_ptr || m_pos > m_len)
			{
				// Memory stream is in an invalid state
				return -1;
			}
			
			if (m_pos == m_len)
			{
				// End of stream
				return 0;
			}

			u8* src = &m_ptr[m_pos];
			u8* dst = (u8*)Buf;
			int read = 0;
			while (ByteCount > 0 && m_pos < m_len)
			{
				*dst++ = *src++;
				m_pos++;
				read++;
				ByteCount--;
			}

			return read;
		}

		// A specialized function that allows the memory stream to be used as a streaming buffer, 
		// where you wish to discard the front part of the memory stream that's already been read. 
		// This function takes the current stream position and removes everything before it, then 
		// resets the stream position to 0.
		// Not supported if the stream is fixed size.
		bool RemoveBeforePosition()
		{
			if (m_isFixedSize) { return false; }
			
			// Special case if position is 0 (no action required)
			if (0 == m_pos) { return true; }

			// Another special case if the position is at the end of the allocated block. It's 
			// implicitly true that this is also the stream length in this case.
			if (m_pos == m_allocSize)
			{
				// Just reset position and length to 0
				m_pos = 0;
				m_len = 0;
				return true;
			}
            
            // We're lowering the size of the stream so we'll never need to reallocate. We just
            // copy the used part of the stream data down to the beginning.
            Cpy(m_ptr, &m_ptr[m_pos], m_len - m_pos);

			// We're chopping off bytes so the length goes down
			m_len -= m_pos;

			// Position gets reset to 0 but allocation size and all other properties don't change
			m_pos = 0;

			return true;
		}

		// Sets the position of the stream. If the requested stream position is invalid, then 
		// no changes will be made to the stream and the current position will be returned. 
		// If the requested stream position is valid, then it is set as the current stream 
		// position and the new stream position (which will be the same as the position 
		// parameter) will be returned.
		virtual i64 SetPosition(i64 position)
		{
			if (position >= 0 && position <= m_len)
			{
				m_pos = position;
			}
			return m_pos;
		}

		// Attempts to write "ByteCount" bytes to the stream starting at the current position.
		// Returns the number of bytes successfully written.
		virtual int Write(const void* Buf, int ByteCount)
		{
			if (ByteCount <= 0) { return 0; }
			
			// Check if we need to resize, but only do so if the stream is not fixed size
			if (m_pos + ByteCount > m_allocSize && !m_isFixedSize)
			{
				// Reallocate
				int newSize = m_allocSize * 2;
				while (newSize < m_pos + ByteCount)
				{
					newSize *= 2;
				}
				u8* newMem = new u8[newSize];
				if (!newMem) { return 0; }
				// Copy from old source
				Cpy(newMem, m_ptr, m_len);
				// Fill remainder with zeros
				int i = m_len;
				while (i < newSize)
				{
					newMem[i] = 0;
					i++;
				}

				// Set new, free old
				u8* old = m_ptr;
				m_ptr = newMem;
				m_allocSize = newSize;
				delete[] old;
			}
			
			// Make a temporary pointer for where we want to start writing
			u8* dst = &m_ptr[m_pos];

			// Also make a source pointer
			u8* src = (u8*)Buf;

			// Keep track of how many bytes we write
			int written = 0;

			while (ByteCount > 0 && m_pos < m_allocSize)
			{
				// Write a byte
				*dst++ = *src++;
			
				ByteCount--;
				m_pos++;
				written++;
			}

			if (!m_isFixedSize)
			{
				if (m_pos > m_len)
				{
					m_len = m_pos;
				}
			}

			return written;
		}	
	};
}
#endif
#include "MsRingBuffer.h"

void MsRingBuffer::extend(size_t minRequired) {
	// Calculate new capacity (at least double, or enough for minRequired)
	size_t newCapacity = std::max(capacity * 2, capacity + minRequired);
	std::vector<char> newBuffer(newCapacity);

	// Copy existing data to new buffer
	if (count > 0) {
		if (tail < head) {
			// Data is contiguous
			std::memcpy(newBuffer.data(), buffer.data() + tail, count);
		} else {
			// Data wraps around
			size_t firstPart = capacity - tail;
			std::memcpy(newBuffer.data(), buffer.data() + tail, firstPart);
			std::memcpy(newBuffer.data() + firstPart, buffer.data(), head);
		}
	}

	buffer = std::move(newBuffer);
	tail = 0;
	head = count;
	capacity = newCapacity;
}

// Write data to buffer, returns number of bytes written
int MsRingBuffer::write(const void *buf, int bufSize) {
	if (buf == nullptr || bufSize <= 0) {
		return 0;
	}

	// Extend buffer if there's not enough space
	if (getWritableSize() < static_cast<size_t>(bufSize)) {
		extend(bufSize - getWritableSize());
	}

	const char *src = static_cast<const char *>(buf);
	int bytesToWrite = bufSize;
	int written = 0;

	while (bytesToWrite > 0) {
		// Calculate continuous space available from head position
		size_t contiguousSpace = (head < tail) ? (tail - head) : (capacity - head);
		size_t toWrite = std::min(static_cast<size_t>(bytesToWrite), contiguousSpace);

		std::memcpy(buffer.data() + head, src + written, toWrite);

		head = (head + toWrite) % capacity;
		written += toWrite;
		bytesToWrite -= toWrite;
		count += toWrite;
	}

	return written;
}

// Read data from buffer, returns number of bytes read
int MsRingBuffer::read(void *buf, int bufSize) {
	if (buf == nullptr || bufSize <= 0) {
		return 0;
	}

	int bytesToRead = std::min(bufSize, static_cast<int>(count));
	if (bytesToRead == 0) {
		return 0; // Buffer is empty
	}

	char *dest = static_cast<char *>(buf);
	int bytesRead = 0;
	int remaining = bytesToRead;

	while (remaining > 0) {
		// Calculate continuous data available from tail position
		size_t contiguousData = (tail < head) ? (head - tail) : (capacity - tail);
		size_t toRead = std::min(static_cast<size_t>(remaining), contiguousData);

		std::memcpy(dest + bytesRead, buffer.data() + tail, toRead);

		tail = (tail + toRead) % capacity;
		bytesRead += toRead;
		remaining -= toRead;
		count -= toRead;
	}

	return bytesRead;
}
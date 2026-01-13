#ifndef MS_RING_BUFFER_H
#define MS_RING_BUFFER_H
#include <algorithm>
#include <cstring>
#include <vector>

class MsRingBuffer {
private:
	std::vector<char> buffer;
	size_t head;     // Write position
	size_t tail;     // Read position
	size_t count;    // Number of bytes stored
	size_t capacity; // Current capacity

	void extend(size_t minRequired);

	size_t getWritableSize() const { return capacity - count; }

public:
	explicit MsRingBuffer(size_t initialCapacity = 1024)
	    : buffer(initialCapacity), head(0), tail(0), count(0), capacity(initialCapacity) {}

	// Write data to buffer, returns number of bytes written
	int write(const void *buf, int bufSize);

	// Read data from buffer, returns number of bytes read
	int read(void *buf, int bufSize);

	bool isEmpty() const { return count == 0; }

	size_t size() const { return count; }

	size_t getCapacity() const { return capacity; }

	void clear() {
		head = 0;
		tail = 0;
		count = 0;
	}
};

#endif // MS_RING_BUFFER_H

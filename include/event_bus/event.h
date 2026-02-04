#ifndef __EVENT_H__
#define __EVENT_H__

#include <array>
#include <cstdint>
#include <cstddef>

namespace event_bus {

	struct Event
	{
		uint64_t timestamp;
		uint32_t type;
		std::array<std::byte, 64> payload;
	};
}
#endif // !__EVENT_H__
#pragma once

#include "EventData.h"

struct EventDataSerializerOptions {
	bool ResolveSymbols{ false };
	bool WriteHeaderLine{ true };
	bool CompressOutput{ false };
	uint32_t StartIndex{ (uint32_t)-1 };
};

struct IEventDataSerializer abstract {
	virtual bool Save(const std::vector<std::shared_ptr<EventData>>& events, const EventDataSerializerOptions& options, PCWSTR path) = 0;
	virtual std::vector<std::shared_ptr<EventData>> Load(PCWSTR path) = 0;
	virtual bool OpenFileForAppend(PCWSTR path) = 0;
	virtual bool Append(const std::shared_ptr<EventData>& data) = 0;
};


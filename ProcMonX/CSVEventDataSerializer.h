#pragma once
#include "IEventDataSerializer.h"
#include <fstream>

class CSVEventDataSerializer : public IEventDataSerializer {
public:
	virtual bool Save(const std::vector<std::shared_ptr<EventData>>& events, const EventDataSerializerOptions& options, PCWSTR path) override;
	virtual std::vector<std::shared_ptr<EventData>> Load(PCWSTR path) override;
	virtual bool OpenFileForAppend(PCWSTR path) override;
	virtual bool Append(const std::shared_ptr<EventData>& data) override;

protected:
	static bool SaveEvent(std::ofstream& stream, const std::shared_ptr<EventData>& data);
private:
	std::ofstream m_FileStream;
};


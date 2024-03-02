#include "pch.h"
#include "CSVEventDataSerializer.h"
#include <fstream>
#include "FormatHelper.h"

bool CSVEventDataSerializer::Save(const std::vector<std::shared_ptr<EventData>>& events, const EventDataSerializerOptions& options, PCWSTR path) {
    std::ofstream out;
    out.open(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if(out.fail())
        return false;

    static const BYTE unicodeHead[] = { 0xFF,0xFE }; //unicode文件头文件  
    out.write((char*)unicodeHead, 2);

    // write title
    std::wstring wcsTitle = _T("#,Time,Event,PID,Process Name,TID,Details\r\n");
    out.write(reinterpret_cast<const char*>(wcsTitle.c_str()), wcsTitle.length() * 2);

    for (auto& data : events) {
		SaveEvent(out, data);
    }

    out.close();
    return true;
}

std::vector<std::shared_ptr<EventData>> CSVEventDataSerializer::Load(PCWSTR path) {
    return std::vector<std::shared_ptr<EventData>>();
}

bool CSVEventDataSerializer::OpenFileForAppend(PCWSTR path)
{
    m_FileStream.open(path, std::ios::out | std::ios::binary | std::ios::app);
    if (m_FileStream.fail())
        return false;
    return true;
}

bool CSVEventDataSerializer::Append(const std::shared_ptr<EventData>& data)
{
    if (!m_FileStream.is_open())
        return false;

    SaveEvent(m_FileStream, data);
    m_FileStream.flush();
    return true;
}

bool CSVEventDataSerializer::SaveEvent(std::ofstream& stream, const std::shared_ptr<EventData>& data)
{
    int colCount = FormatHelper::GetColumnCount();
    CString line;
    line.Format(L"%7u", data->GetIndex());  // index, 不需要加'*'标记
    for (int i = 1; i < colCount; ++i) {
        line += L",";
        line += FormatHelper::GetColumnText(data.get(), i);
    }
    line += L"\r\n";
    stream.write(reinterpret_cast<const char*>(line.GetString()), line.GetLength() * 2);
	return true;
}

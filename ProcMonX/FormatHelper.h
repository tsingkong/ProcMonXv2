#pragma once
#include "EventData.h"

struct FormatHelper {
	static std::wstring FormatProperty(const EventData* data, const EventProperty& prop);
	static CString FormatTime(LONGLONG ts);
	static CString VirtualAllocFlagsToString(DWORD flags, bool withNumeric = false);
	static CString MajorFunctionToString(UCHAR mf);
	static PCWSTR ObjectTypeToString(int type);

	static int GetColumnCount();
	static CString GetColumnText(EventData* item, int col);
	static std::wstring ProcessSpecialEvent(EventData* data);
	static std::wstring GetEventDetails(EventData* data);
};


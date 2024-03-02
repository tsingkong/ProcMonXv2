#include "pch.h"
#include "FormatHelper.h"
#include "TraceManager.h"
#include <functional>

#pragma comment(lib, "ntdll")

typedef enum _OBJECT_INFORMATION_CLASS {
	ObjectBasicInformation, // OBJECT_BASIC_INFORMATION
	ObjectNameInformation, // OBJECT_NAME_INFORMATION
	ObjectTypeInformation, // OBJECT_TYPE_INFORMATION
	ObjectTypesInformation, // OBJECT_TYPES_INFORMATION
	ObjectHandleFlagInformation, // OBJECT_HANDLE_FLAG_INFORMATION
	ObjectSessionInformation,
	ObjectSessionObjectInformation,
	MaxObjectInfoClass
} OBJECT_INFORMATION_CLASS;

typedef struct _UNICODE_STRING {
	USHORT Length;
	USHORT MaximumLength;
	PWSTR  Buffer;
} UNICODE_STRING;
typedef UNICODE_STRING* PUNICODE_STRING;
typedef const UNICODE_STRING* PCUNICODE_STRING;

extern "C" NTSTATUS NTAPI NtQueryObject(
	_In_opt_ HANDLE Handle,
	_In_ OBJECT_INFORMATION_CLASS ObjectInformationClass,
	_Out_writes_bytes_opt_(ObjectInformationLength) PVOID ObjectInformation,
	_In_ ULONG ObjectInformationLength,
	_Out_opt_ PULONG ReturnLength);

typedef struct _OBJECT_TYPE_INFORMATION {
	UNICODE_STRING TypeName;
	ULONG TotalNumberOfObjects;
	ULONG TotalNumberOfHandles;
	ULONG TotalPagedPoolUsage;
	ULONG TotalNonPagedPoolUsage;
	ULONG TotalNamePoolUsage;
	ULONG TotalHandleTableUsage;
	ULONG HighWaterNumberOfObjects;
	ULONG HighWaterNumberOfHandles;
	ULONG HighWaterPagedPoolUsage;
	ULONG HighWaterNonPagedPoolUsage;
	ULONG HighWaterNamePoolUsage;
	ULONG HighWaterHandleTableUsage;
	ULONG InvalidAttributes;
	GENERIC_MAPPING GenericMapping;
	ULONG ValidAccessMask;
	BOOLEAN SecurityRequired;
	BOOLEAN MaintainHandleCount;
	UCHAR TypeIndex; // since WINBLUE
	CHAR ReservedByte;
	ULONG PoolType;
	ULONG DefaultPagedPoolCharge;
	ULONG DefaultNonPagedPoolCharge;
} OBJECT_TYPE_INFORMATION, * POBJECT_TYPE_INFORMATION;

typedef struct _OBJECT_TYPES_INFORMATION {
	ULONG NumberOfTypes;
	OBJECT_TYPE_INFORMATION TypeInformation[1];
} OBJECT_TYPES_INFORMATION, * POBJECT_TYPES_INFORMATION;

typedef struct _SYSTEM_TIMEOFDAY_INFORMATION {
	LARGE_INTEGER BootTime;
	LARGE_INTEGER CurrentTime;
	LARGE_INTEGER TimeZoneBias;
	ULONG TimeZoneId;
	ULONG Reserved;
	ULONGLONG BootTimeBias;
	ULONGLONG SleepTimeBias;
} SYSTEM_TIMEOFDAY_INFORMATION, * PSYSTEM_TIMEOFDAY_INFORMATION;

enum SYSTEM_INFORMATION_CLASS {
	SystemTimeOfDayInformation = 3
};

extern "C" NTSTATUS NTAPI NtQuerySystemInformation(
	_In_ SYSTEM_INFORMATION_CLASS SystemInformationClass,
	_Out_writes_bytes_opt_(SystemInformationLength) PVOID SystemInformation,
	_In_ ULONG SystemInformationLength,
	_Out_opt_ PULONG ReturnLength);

uint64_t GetBootTime() {
	static int64_t time = 0;
	if (time == 0) {
		SYSTEM_TIMEOFDAY_INFORMATION info;
		if (0 == ::NtQuerySystemInformation(SystemTimeOfDayInformation, &info, sizeof(info), nullptr))
			time = info.BootTime.QuadPart;
	}

	return time;
}

std::wstring FormatHelper::FormatProperty(const EventData* data, const EventProperty& prop) {
	static const auto statusFunction = [](auto, auto& p) {
		CString result;
		ATLASSERT(p.GetLength() == sizeof(DWORD));
		result.Format(L"0x%08X", p.GetValue<DWORD>());
		return (PCWSTR)result;
	};

	static const auto int64ToHex = [](auto, auto& p) -> std::wstring {
		ATLASSERT(p.GetLength() == sizeof(LONGLONG));
		CString text;
		text.Format(L"0x%llX", p.GetValue<LONGLONG>());
		return (PCWSTR)text;
	};

	static const auto initialTimeToString = [](auto, auto& p) -> std::wstring {
		ATLASSERT(p.GetLength() == sizeof(LONGLONG));
		auto time = p.GetValue<time_t>();
		auto initTime = GetBootTime() + time;
		return std::wstring(FormatTime(initTime));
	};

	static const std::unordered_map<std::wstring, std::function<std::wstring(const EventData*, const EventProperty&)>> functions{
		{ L"Status", statusFunction },
		{ L"NtStatus", statusFunction },
		{ L"InitialTime", initialTimeToString },
		{ L"TimeDateStamp", statusFunction },
		{ L"PageFault/VirtualAlloc;Flags", [](auto, auto& p) -> std::wstring {
			ATLASSERT(p.GetLength() == sizeof(DWORD));
			return (PCWSTR)VirtualAllocFlagsToString(p.GetValue<DWORD>(), true);
			} },
		{ L"MajorFunction", [](auto, auto& p) -> std::wstring {
			ATLASSERT(p.GetLength() == sizeof(DWORD));
			return (PCWSTR)MajorFunctionToString((UCHAR)p.GetValue<DWORD>());
			} },
		{ L"FileName", [](auto data, auto& p) -> std::wstring {
			auto value = data->FormatProperty(p);
			if (value[0] == L'\\')
				value = TraceManager::GetDosNameFromNtName(value.c_str());
			return value;
			} },
		{ L"ObjectType", [](auto, auto& p) -> std::wstring {
			ATLASSERT(p.GetLength() == sizeof(USHORT));
			auto type = p.GetValue<USHORT>();
			return std::to_wstring(type) + L" (" + ObjectTypeToString(type) + L")";
			} },
		{ L"Tag", [](auto data, auto& p) -> std::wstring {
			if (p.GetLength() != sizeof(DWORD))
				return data->FormatProperty(p);
			auto tag = p.GetValue<DWORD>();
			auto chars = (const char*)&tag;
			CStringA str(chars[0]);
			((str += chars[1]) += chars[2]) += chars[3];
			CStringA text;
			text.Format("%s (0x%X)", str, tag);
			return std::wstring(CString(text));
			} },
	};

	auto it = functions.find(data->GetEventName() + L";" + prop.Name);
	if (it == functions.end())
		it = functions.find(prop.Name);
	if (it == functions.end())
		return L"";

	return (it->second)(data, prop);
}

CString FormatHelper::FormatTime(LONGLONG ts) {
	CString text;
	text.Format(L".%06u", (ts / 10) % 1000000);
	return CTime(*(FILETIME*)&ts).Format(L"%x %X") + text;
}

CString FormatHelper::VirtualAllocFlagsToString(DWORD flags, bool withNumeric) {
	CString text;
	if (flags & MEM_COMMIT)
		text += L"MEM_COMMIT | ";
	if (flags & MEM_RESERVE)
		text += L"MEM_RESERVE | ";

	if (!text.IsEmpty())
		text = text.Left(text.GetLength() - 3);

	if (withNumeric)
		text.Format(L"%s (0x%X)", text, flags);

	return text;
}

CString FormatHelper::MajorFunctionToString(UCHAR mf) {
	static PCWSTR major[] = {
		L"CREATE",
		L"CREATE_NAMED_PIPE",
		L"CLOSE",
		L"READ",
		L"WRITE",
		L"QUERY_INFORMATION",
		L"SET_INFORMATION",
		L"QUERY_EA",
		L"SET_EA",
		L"FLUSH_BUFFERS",
		L"QUERY_VOLUME_INFORMATION",
		L"SET_VOLUME_INFORMATION",
		L"DIRECTORY_CONTROL",
		L"FILE_SYSTEM_CONTROL",
		L"DEVICE_CONTROL",
		L"INTERNAL_DEVICE_CONTROL",
		L"SHUTDOWN",
		L"LOCK_CONTROL",
		L"CLEANUP",
		L"CREATE_MAILSLOT",
		L"QUERY_SECURITY",
		L"SET_SECURITY",
		L"POWER",
		L"SYSTEM_CONTROL",
		L"DEVICE_CHANGE",
		L"QUERY_QUOTA",
		L"SET_QUOTA",
		L"PNP"
	};

	static PCWSTR major_flt[] = {
		L"ACQUIRE_FOR_SECTION_SYNCHRONIZATION",
		L"RELEASE_FOR_SECTION_SYNCHRONIZATION",
		L"ACQUIRE_FOR_MOD_WRITE",
		L"RELEASE_FOR_MOD_WRITE",
		L"ACQUIRE_FOR_CC_FLUSH",
		L"RELEASE_FOR_CC_FLUSH",
		L"QUERY_OPEN",
	};

	static PCWSTR major_flt2[] = {
		L"FAST_IO_CHECK_IF_POSSIBLE",
		L"NETWORK_QUERY_OPEN",
		L"MDL_READ",
		L"MDL_READ_COMPLETE",
		L"PREPARE_MDL_WRITE",
		L"MDL_WRITE_COMPLETE",
		L"VOLUME_MOUNT",
		L"VOLUME_DISMOUNT"
	};

	CString text;
	if (mf < _countof(major))
		text.Format(L"IRP_MJ_%s (%d)", major[mf], (int)mf);
	else if (mf >= 255 - 7)
		text.Format(L"IRP_MJ_%s (%d)", major_flt[255 - mf - 1], (int)mf);
	else if (mf >= 255 - 20)
		text.Format(L"IRP_MJ_%s (%d)", major_flt2[255 - mf - 7], (int)mf);
	else
		text.Format(L"%d", (int)mf);
	return text;
}

PCWSTR FormatHelper::ObjectTypeToString(int type) {
	static std::unordered_map<int, std::wstring> types;
	if (types.empty()) {
		const ULONG len = 1 << 14;
		BYTE buffer[len];
		if (0 != ::NtQueryObject(nullptr, ObjectTypesInformation, buffer, len, nullptr))
			return L"";

		auto p = reinterpret_cast<OBJECT_TYPES_INFORMATION*>(buffer);
		auto count = p->NumberOfTypes;
		types.reserve(count);

		auto raw = &p->TypeInformation[0];
		for (ULONG i = 0; i < count; i++) {
			types.insert({ raw->TypeIndex, std::wstring(raw->TypeName.Buffer, raw->TypeName.Length / sizeof(WCHAR)) });

			auto temp = (BYTE*)raw + sizeof(OBJECT_TYPE_INFORMATION) + raw->TypeName.MaximumLength;
			temp += sizeof(PVOID) - 1;
			raw = reinterpret_cast<OBJECT_TYPE_INFORMATION*>((ULONG_PTR)temp / sizeof(PVOID) * sizeof(PVOID));
		}
	}
	return types.empty() ? L"" : types[type].c_str();
}

std::wstring FormatHelper::ProcessSpecialEvent(EventData* data) {
	std::wstring details;
	CString text;
	auto& name = data->GetEventName();
	if (name == L"Process/Start") {
		text.Format(L"PID: %u; Image: %s; Command Line: %s",
			data->GetProperty(L"ProcessId")->GetValue<DWORD>(),
			CString(data->GetProperty(L"ImageFileName")->GetAnsiString()),
			data->GetProperty(L"CommandLine")->GetUnicodeString());
		details = std::move(text);
	}
	return details;
}

std::wstring FormatHelper::GetEventDetails(EventData* data) {
	auto details = ProcessSpecialEvent(data);
	if (details.empty()) {
		for (auto& prop : data->GetProperties()) {
			if (prop.Name.substr(0, 8) != L"Reserved" && prop.Name.substr(0, 4) != L"TTID") {
				auto value = FormatHelper::FormatProperty(data, prop);
				if (value.empty())
					value = data->FormatProperty(prop);
				if (!value.empty()) {
					if (value.size() > 102)
						value = value.substr(0, 100) + L"...";
					details += prop.Name + L": " + value + L"; ";
				}
			}
		}
	}
	return details;
}

int FormatHelper::GetColumnCount() {
	return 7;
}

CString FormatHelper::GetColumnText(EventData* item, int col) {
	CString text;

	switch (col) {
	case 0: {
		text.Format(L"%7u", item->GetIndex());
		if (item->GetStackEventData())
			text += L" *";
		break;
	}
	case 1:	{
		auto ts = item->GetTimeStamp();
		return FormatHelper::FormatTime(ts);
	}
	case 2:
		return item->GetEventName().c_str();
	case 3:	{
		auto pid = item->GetProcessId();
		if (pid != (DWORD)-1)
			text.Format(L"%u (0x%X)", pid, pid);
		break;
	}
	case 4:
		return item->GetProcessName().c_str();
	case 5:	{
		auto tid = item->GetThreadId();
		if (tid != (DWORD)-1 && tid != 0)
			text.Format(L"%u (0x%X)", tid, tid);
		break;
	}
	//case 6: text.Format(L"%d", (int)item->GetEventDescriptor().Opcode);
	//case 7: 
	//	::StringFromGUID2(item->GetProviderId(), text.GetBufferSetLength(64), 64);
	//	break;

	case 6:
		return GetEventDetails(item).c_str();
	}

	return text;
}

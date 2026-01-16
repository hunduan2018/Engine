// Direct3DUtils.h
#pragma once
#include <string>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <system_error>
#include <windows.h>
#include <comdef.h> // _com_error for HRESULT formatting

namespace Direct3DUtils
{
	inline std::string FormatWin32Error(DWORD err)
	{
		if (err == 0) return "None";
		LPVOID msgBuf = nullptr;
		DWORD len = FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			reinterpret_cast<LPSTR>(&msgBuf), 0, nullptr);
		std::string msg = (len && msgBuf) ? std::string(static_cast<const char*>(msgBuf), len) : "Unknown";
		if (msgBuf) LocalFree(msgBuf);
		return msg;
	}

	inline std::string FormatHresult(HRESULT hr)
	{
		if (hr == S_OK) return "S_OK";
		_com_error err(hr);
		const wchar_t* wmsg = err.ErrorMessage();
		int len = wmsg ? static_cast<int>(wcslen(wmsg)) : 0;
		std::string msg;
		if (len > 0)
		{
			int bytes = WideCharToMultiByte(CP_UTF8, 0, wmsg, len, nullptr, 0, nullptr, nullptr);
			msg.resize(bytes);
			WideCharToMultiByte(CP_UTF8, 0, wmsg, len, &msg[0], bytes, nullptr, nullptr);
		}
		else
		{
			msg = "Unknown HRESULT";
		}
		std::ostringstream oss;
		oss << "0x" << std::hex << std::setw(8) << std::setfill('0') << static_cast<unsigned long>(hr)
			<< " (" << msg << ")";
		return oss.str();
	}

	// 基础版本：仅消息
	inline void ThrowRuntimeError(const std::string& message,
		const char* file = nullptr,
		int line = 0)
	{
		std::ostringstream oss;
		oss << message;
		DWORD lastErr = GetLastError();
		if (lastErr)
		{
			oss << " | Win32LastError=" << lastErr << " (" << FormatWin32Error(lastErr) << ")";
		}
		if (file && line > 0)
		{
			oss << " | at " << file << ":" << line;
		}
		throw std::runtime_error(oss.str());
	}

	// 扩展版本：携带 HRESULT
	inline void ThrowRuntimeError(const std::string& message,
		HRESULT hr,
		const char* file = nullptr,
		int line = 0)
	{
		std::ostringstream oss;
		oss << message << " | HRESULT=" << FormatHresult(hr);
		DWORD lastErr = GetLastError();
		if (lastErr)
		{
			oss << " | Win32LastError=" << lastErr << " (" << FormatWin32Error(lastErr) << ")";
		}
		if (file && line > 0)
		{
			oss << " | at " << file << ":" << line;
		}
		throw std::runtime_error(oss.str());
	}
}

// 便捷宏：自动带文件/行号
#define D3D_THROW(MSG) ::Direct3DUtils::ThrowRuntimeError((MSG), __FILE__, __LINE__)
#define D3D_THROW_HR(MSG, HR) ::Direct3DUtils::ThrowRuntimeError((MSG), (HR), __FILE__, __LINE__)
// Assert.h
#pragma once
#include <cstdio>
#include <cstdlib>



namespace Application
{
	// 当断言失败时调用：记录信息后中断/崩溃
	inline void AssertFail(const char* Expr, const char* File, int Line, const char* Func, const char* Msg)
	{
		// 基础输出到 stderr
		std::fprintf(stderr,
			"Assertion failed!\n"
			"  Expr : %s\n"
			"  File : %s(%d)\n"
			"  Func : %s\n"
			"  Msg  : %s\n",
			Expr ? Expr : "(null)",
			File ? File : "(unknown)",
			Line,
			Func ? Func : "(unknown)",
			Msg ? Msg : "(none)");

#ifdef _WIN32
		// 也输出到调试器
#include <windows.h>
#include <debugapi.h>
		char buffer[1024];
		std::snprintf(buffer, sizeof(buffer),
			"Assertion failed! Expr: %s | File: %s(%d) | Func: %s | Msg: %s\n",
			Expr ? Expr : "(null)",
			File ? File : "(unknown)",
			Line,
			Func ? Func : "(unknown)",
			Msg ? Msg : "(none)");
		OutputDebugStringA(buffer);

		// 若已附加调试器，先中断，方便定位
		if (IsDebuggerPresent())
		{
			DebugBreak();
		}

		// 终止进程（UE 的 check 会触发致命错误并停止执行）
		// 你也可以改成 RaiseFailFastException 或 __fastfail，更“硬”的终止
		std::abort();
#else
		// 非 Windows 平台：触发断点或崩溃
		// 若希望尽可能在调试器停下，可用 __builtin_trap()
		__builtin_trap();
		// 兜底（某些平台 __builtin_trap 可能直接终止，这里的 abort 通常不会执行到）
		std::abort();
#endif
	}

	// 对外的断言入口：条件为 false 时进入 AssertFail
	inline void Assert(bool Condition, const char* Expr, const char* File, int Line, const char* Func, const char* Msg = nullptr)
	{
		if (!Condition)
		{
			AssertFail(Expr, File, Line, Func, Msg);
		}
	}
}

// 宏：在非 NDEBUG（Debug/Development）下生效；在 NDEBUG（Release/Shipping）下为空
#ifndef NDEBUG
#define APP_CHECK(expr) \
        ((expr) ? (void)0 : ::Application::Assert((expr), #expr, __FILE__, __LINE__, __func__, nullptr))

#define APP_CHECK_MSG(expr, msg) \
        ((expr) ? (void)0 : ::Application::Assert((expr), #expr, __FILE__, __LINE__, __func__, (msg)))
#else
#define APP_CHECK(expr)        ((void)0)
#define APP_CHECK_MSG(expr,m)  ((void)0)
#endif
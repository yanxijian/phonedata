// phonedata 的最小测试程序：不依赖任何测试框架，失败时返回非 0。
//
// 用法：
//   phonedata_test [phone.dat 路径]
// 不传参数时使用编译期定义的 PHONEDATA_TEST_DATA（CMake 会指向仓库内的
// phonedata/phone.dat）；旧的 phonedata.sln 没有该定义，退回当前目录。

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <fstream>

#include "phonedata.h"

#ifndef PHONEDATA_TEST_DATA
#define PHONEDATA_TEST_DATA "phone.dat"
#endif

namespace
{
	int g_failed = 0;
	int g_passed = 0;

	void check(bool cond, const char *what)
	{
		if (cond)
		{
			++g_passed;
		}
		else
		{
			++g_failed;
			std::printf("FAIL: %s\n", what);
		}
	}

	void checkStr(const std::string &actual, const char *expected, const char *what)
	{
		if (actual == expected)
		{
			++g_passed;
		}
		else
		{
			++g_failed;
			std::printf("FAIL: %s (expected \"%s\", got \"%s\")\n", what, expected, actual.c_str());
		}
	}

	void checkInfo(const PhoneInfo &info, CARDTYPE type, const char *province,
				   const char *city, const char *zip, const char *area, const char *what)
	{
		const bool ok = info.type == type
			&& info.province == province
			&& info.city == city
			&& info.zipCode == zip
			&& info.areaCode == area;
		if (ok)
		{
			++g_passed;
			return;
		}

		++g_failed;
		std::printf("FAIL: %s (type=%d province=\"%s\" city=\"%s\" zip=\"%s\" area=\"%s\")\n",
					what, static_cast<int>(info.type), info.province.c_str(),
					info.city.c_str(), info.zipCode.c_str(), info.areaCode.c_str());
	}

	// 写一个临时文件，用于损坏数据的健壮性测试。
	bool writeFile(const std::string &path, const void *data, size_t size)
	{
		std::ofstream out(path.c_str(), std::ios::binary);
		if (!out.is_open())
		{
			return false;
		}
		out.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
		out.close();
		return true;
	}
}

int main(int argc, char **argv)
{
	const char *datPath = (argc > 1) ? argv[1] : PHONEDATA_TEST_DATA;

	// ---- 正常数据 ----
	PhoneData data(datPath);
	check(data.isOpen(), "数据文件应能加载");
	check(data.version() == "1707", "版本号应为 1707（2017 年 7 月）");
	check(data.recordCount() == 364528u, "索引条数应为 364528");

	checkInfo(data.lookUp(int64_t(1300000)), CUCC, "山东", "济南", "250000", "0531", "1300000");
	checkInfo(data.lookUp(int64_t(1860755)), CUCC, "广东", "深圳", "518000", "0755", "1860755");
	checkInfo(data.lookUp(int64_t(1899999)), CTCC, "新疆", "乌鲁木齐", "830000", "0991", "1899999");
	checkInfo(data.lookUp(int64_t(1450123)), CUCC, "天津", "天津", "300000", "022", "1450123");

	// 11 位完整号码应与前 7 位一致
	checkInfo(data.lookUp(int64_t(18607551234LL)), CUCC, "广东", "深圳", "518000", "0755", "18607551234");
	checkInfo(data.lookUp(std::string("18607551234")), CUCC, "广东", "深圳", "518000", "0755", "string 18607551234");
	checkInfo(data.lookUp(std::string(" 1860755 ")), CUCC, "广东", "深圳", "518000", "0755", "string 带空白");

	// ---- 未命中的号码 ----
	check(data.lookUp(int64_t(1770000)).type == UNKNOWN, "1770000 应未命中");
	check(data.lookUp(int64_t(1770000)).province.empty(), "未命中的省份应为空");
	check(data.lookUp(int64_t(1000000)).type == UNKNOWN, "低于最小号段应未命中");
	check(data.lookUp(int64_t(9999999)).type == UNKNOWN, "高于最大号段应未命中");
	check(data.lookUp(int64_t(0)).type == UNKNOWN, "0 应未命中");
	check(data.lookUp(int64_t(-1)).type == UNKNOWN, "负数应未命中");

	// ---- 非法字符串输入：不得抛异常 ----
	check(data.lookUp(std::string("")).type == UNKNOWN, "空串应未命中");
	check(data.lookUp(std::string("abc")).type == UNKNOWN, "纯字母应未命中");
	check(data.lookUp(std::string("1860755x")).type == UNKNOWN, "含非数字应未命中");
	check(data.lookUp(std::string("186075")).type == UNKNOWN, "6 位应未命中");
	check(data.lookUp(std::string("18607551234567")).type == UNKNOWN, "17 位应未命中");

	// ---- 类型名 ----
	checkStr(getPhoneType(UNKNOWN), "未知", "UNKNOWN 名称");
	checkStr(getPhoneType(CMCC), "中国移动", "CMCC 名称");
	checkStr(getPhoneType(CUCC), "中国联通", "CUCC 名称");
	checkStr(getPhoneType(CTCC), "中国电信", "CTCC 名称");
	checkStr(getPhoneType(CUCC_V), "联通虚拟运营商", "CUCC_V 名称");
	checkStr(getPhoneType(static_cast<CARDTYPE>(99)), "", "越界类型应返回空串");

	// ---- 损坏数据：应安全失败，不得崩溃 ----
	{
		PhoneData missing("no_such_file_should_exist.dat");
		check(!missing.isOpen(), "文件缺失时 isOpen 应为 false");
		check(missing.lookUp(int64_t(1860755)).type == UNKNOWN, "文件缺失时查询应为空");
		check(missing.recordCount() == 0u, "文件缺失时条数应为 0");
		check(missing.version().empty(), "文件缺失时版本号应为空");
	}

	{
		// 只有 4 字节，装不下文件头
		const unsigned char tiny[4] = { 0x31, 0x37, 0x30, 0x37 };
		const std::string p = "phonedata_test_truncated.dat";
		check(writeFile(p, tiny, sizeof(tiny)), "写入截断测试文件");
		PhoneData truncated(p);
		check(!truncated.isOpen(), "截断文件应加载失败");
		std::remove(p.c_str());
	}

	{
		// 偏移越界：offset = 0xFFFFFF00，远大于文件长度
		unsigned char bad[16] = { '1', '7', '0', '7', 0x00, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0 };
		const std::string p = "phonedata_test_badoffset.dat";
		check(writeFile(p, bad, sizeof(bad)), "写入非法偏移测试文件");
		PhoneData badOffset(p);
		check(!badOffset.isOpen(), "偏移越界应加载失败");
		std::remove(p.c_str());
	}

	{
		// 索引区长度不是 9 的整数倍
		unsigned char bad[8 + 10] = { '1', '7', '0', '7', 8, 0, 0, 0 };
		const std::string p = "phonedata_test_badlen.dat";
		check(writeFile(p, bad, sizeof(bad)), "写入非法长度测试文件");
		PhoneData badLen(p);
		check(!badLen.isOpen(), "索引区长度不是 9 的倍数应加载失败");
		std::remove(p.c_str());
	}

	std::printf("\n%d passed, %d failed\n", g_passed, g_failed);
	return g_failed == 0 ? 0 : 1;
}

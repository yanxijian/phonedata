#ifndef _PHONEDATA_H_
#define _PHONEDATA_H_

#if defined(_WIN32)
	#if _MSC_VER >= 1600
		#pragma execution_character_set("utf-8")
	#else
		#error Visual Studio 2010 SP1 or above required
	#endif
#endif

#include <string>
#include <vector>
#include <cstdint>

enum CARDTYPE
{
	UNKNOWN = 0,	// 未知，查找失败
	CMCC,			// 中国移动
	CUCC,			// 中国联通
	CTCC,			// 中国电信
	CTCC_V,			// 电信虚拟运营商
	CUCC_V,			// 联通虚拟运营商
	CMCC_V			// 移动虚拟运营商
};

enum DATALEN
{
	CHAR_LENGTH  = 1,
	INT_LENGTH	 = 4,
	PHONE_LENGTH = 7,
	HEAD_LENGTH  = 8,
	PHONE_INDEX_LENGTH = 9
};

enum RECORD
{
	PROVINCE = 0,	// 省份
	CITY,			// 城市
	ZIPCODE,		// 邮编
	AREACODE,		// 长途区号
	RECORD_FIELDS	// 字段个数（=4）
};

// 卡片类型的中文名（UTF-8）。
std::string getPhoneType(CARDTYPE type);

struct PhoneInfo
{
	PhoneInfo()
		: type(UNKNOWN), phone(0) {	}

	CARDTYPE type;
	uint32_t phone;
	std::string zipCode;
	std::string areaCode;
	std::string province;
	std::string city;
};

// 文件头 8 字节：4 字节版本号（如 "1707" 表示 2017 年 7 月）+ 4 字节首个索引偏移。
struct DataHead
{
	char version[4];
	uint32_t offset;
};

// 索引项恰好 9 个字节，必须按 1 字节对齐读取，否则 sizeof 会被填充到 12。
#pragma pack(push, 1)
struct Record
{
	uint32_t phone;
	uint32_t offset;
	uint8_t type;
};
#pragma pack(pop)

static_assert(sizeof(DataHead) == HEAD_LENGTH, "DataHead must be 8 bytes");
static_assert(sizeof(Record) == PHONE_INDEX_LENGTH, "Record must be 9 bytes");

// phone.dat 查询器。
//
// 数据文件内所有字符串（省/市名）均为 UTF-8 编码；本类返回的字符串同样是 UTF-8。
// 构造失败（文件缺失、截断、偏移非法）时 isOpen() 为 false，所有 lookUp() 返回空结果，
// 不会抛异常，也不会访问越界内存。
class PhoneData
{
public:
	// 默认读取当前工作目录下的 phone.dat（保持旧行为）。
	PhoneData();
	explicit PhoneData(const std::string &datPath);

	// 数据文件是否成功加载并通过完整性检查。
	bool isOpen() const { return opened; }

	// 版本号字符串，例如 "1707"；未加载成功时返回空串。
	std::string version() const;

	// 索引项条数；未加载成功时为 0。
	size_t recordCount() const { return count; }

	// 支持 7~11 位号码；内部统一取前 7 位查询。非法输入返回空结果。
	PhoneInfo lookUp(int64_t phone) const;
	PhoneInfo lookUp(const std::string &phone) const;

private:
	bool load(const std::string &datPath);
	PhoneInfo _lookUp(uint32_t phone7) const;
	bool readRecord(size_t index, Record &out) const;
	std::string getRecordContent(size_t startOffset) const;

private:
	std::vector<char> buffer;
	uint32_t indexOffset;
	size_t count;
	bool opened;
};

#endif // !_PHONEDATA_H_

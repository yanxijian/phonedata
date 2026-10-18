#include <fstream>
#include <iterator>
#include <cstring>
#include "phonedata.h"

namespace
{
	// 7 位号段的最小/最大值。
	const uint32_t kMinPhone7 = 1000000;
	const uint32_t kMaxPhone7 = 9999999;

	// 把 "a|b|c|d" 拆成最多 RECORD_FIELDS 段；不足 4 段时后面的字段留空。
	void splitRecord(const std::string &content, std::string (&fields)[RECORD_FIELDS])
	{
		size_t pos = 0;
		for (int i = 0; i < RECORD_FIELDS; ++i)
		{
			size_t sep = content.find('|', pos);
			if (sep == std::string::npos)
			{
				fields[i] = content.substr(pos);
				pos = content.size();
			}
			else
			{
				fields[i] = content.substr(pos, sep - pos);
				pos = sep + 1;
			}
		}
	}

	bool allDigits(const std::string &s)
	{
		if (s.empty())
		{
			return false;
		}
		for (size_t i = 0; i < s.size(); ++i)
		{
			if (s[i] < '0' || s[i] > '9')
			{
				return false;
			}
		}
		return true;
	}
}

std::string getPhoneType(CARDTYPE type)
{
	switch (type)
	{
		case UNKNOWN:
			return std::string("未知");
		case CMCC:
			return std::string("中国移动");
		case CUCC:
			return std::string("中国联通");
		case CTCC:
			return std::string("中国电信");
		case CTCC_V:
			return std::string("电信虚拟运营商");
		case CUCC_V:
			return std::string("联通虚拟运营商");
		case CMCC_V:
			return std::string("移动虚拟运营商");
		default:
			return std::string();
	}
}

PhoneData::PhoneData()
	: indexOffset(0), count(0), opened(false)
{
	load(std::string("phone.dat"));
}

PhoneData::PhoneData(const std::string &datPath)
	: indexOffset(0), count(0), opened(false)
{
	load(datPath);
}

bool PhoneData::load(const std::string &datPath)
{
	std::ifstream stream(datPath.c_str(), std::ios::binary);
	if (!stream.is_open())
	{
		return false;
	}

	std::vector<char> data((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	stream.close();

	// 至少要能放下文件头，否则视为损坏。
	if (data.size() < HEAD_LENGTH)
	{
		return false;
	}

	DataHead head;
	std::memcpy(&head, data.data(), sizeof(head));

	// 首个索引偏移必须落在文件内部，且索引区长度是 9 的整数倍。
	if (head.offset < HEAD_LENGTH || static_cast<size_t>(head.offset) >= data.size())
	{
		return false;
	}
	if ((data.size() - head.offset) % PHONE_INDEX_LENGTH != 0)
	{
		return false;
	}

	buffer.swap(data);
	indexOffset = head.offset;
	count = (buffer.size() - indexOffset) / PHONE_INDEX_LENGTH;
	opened = true;
	return true;
}

std::string PhoneData::version() const
{
	if (!opened)
	{
		return std::string();
	}

	DataHead head;
	std::memcpy(&head, buffer.data(), sizeof(head));
	return std::string(head.version, sizeof(head.version));
}

PhoneInfo PhoneData::lookUp(int64_t phone) const
{
	if (!opened || phone < static_cast<int64_t>(kMinPhone7))
	{
		return PhoneInfo();
	}

	// 11 位号码逐位去掉后 4 位，得到 7 位号段。
	while (phone > static_cast<int64_t>(kMaxPhone7))
	{
		phone /= 10;
	}

	return _lookUp(static_cast<uint32_t>(phone));
}

PhoneInfo PhoneData::lookUp(const std::string &phone) const
{
	if (!opened)
	{
		return PhoneInfo();
	}

	// 去掉首尾空白，允许 "186 0755 1234" 之外的纯数字形式。
	size_t begin = 0;
	size_t end = phone.size();
	while (begin < end && (phone[begin] == ' ' || phone[begin] == '\t'))
	{
		++begin;
	}
	while (end > begin && (phone[end - 1] == ' ' || phone[end - 1] == '\t'))
	{
		--end;
	}

	const std::string digits = phone.substr(begin, end - begin);
	if (digits.size() < PHONE_LENGTH || digits.size() > 11 || !allDigits(digits))
	{
		return PhoneInfo();
	}

	uint32_t phone7 = 0;
	for (size_t i = 0; i < PHONE_LENGTH; ++i)
	{
		phone7 = phone7 * 10 + static_cast<uint32_t>(digits[i] - '0');
	}

	return _lookUp(phone7);
}

bool PhoneData::readRecord(size_t index, Record &out) const
{
	if (index >= count)
	{
		return false;
	}

	const size_t offset = indexOffset + index * PHONE_INDEX_LENGTH;
	if (offset + PHONE_INDEX_LENGTH > buffer.size())
	{
		return false;
	}

	std::memcpy(&out, buffer.data() + offset, sizeof(out));
	return true;
}

PhoneInfo PhoneData::_lookUp(uint32_t phone7) const
{
	if (!opened || phone7 < kMinPhone7 || phone7 > kMaxPhone7)
	{
		return PhoneInfo();
	}

	// 索引区按 phone 升序排列，取第一个不小于 phone7 的位置。
	size_t lo = 0;
	size_t hi = count;
	while (lo < hi)
	{
		const size_t mid = lo + (hi - lo) / 2;
		Record record;
		if (!readRecord(mid, record))
		{
			return PhoneInfo();
		}

		if (record.phone < phone7)
		{
			lo = mid + 1;
		}
		else
		{
			hi = mid;
		}
	}

	Record record;
	if (lo >= count || !readRecord(lo, record) || record.phone != phone7)
	{
		return PhoneInfo();
	}

	std::string fields[RECORD_FIELDS];
	splitRecord(getRecordContent(record.offset), fields);

	PhoneInfo info;
	info.type		= static_cast<CARDTYPE>(record.type);
	info.phone		= record.phone;
	info.province	= fields[PROVINCE];
	info.city		= fields[CITY];
	info.zipCode	= fields[ZIPCODE];
	info.areaCode	= fields[AREACODE];
	return info;
}

std::string PhoneData::getRecordContent(size_t startOffset) const
{
	// 记录区位于 [HEAD_LENGTH, indexOffset)，越界一律视为空记录。
	if (startOffset < HEAD_LENGTH || startOffset >= indexOffset || startOffset >= buffer.size())
	{
		return std::string();
	}

	size_t endOffset = startOffset;
	while (endOffset < indexOffset && buffer[endOffset] != '\0')
	{
		++endOffset;
	}

	return std::string(buffer.data() + startOffset, endOffset - startOffset);
}

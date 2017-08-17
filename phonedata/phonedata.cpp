#include <fstream>
#include <string>
#include "phonedata.h"
#include <cassert>
#include <algorithm>

static std::vector<std::string> ssplit(const std::string &str, const std::string &c)
{
	std::vector<std::string> vec;
	std::string::size_type pos1 = 0, pos2 = str.find(c);
	while (std::string::npos != pos2)
	{
		std::string tmp = str.substr(pos1, pos2 - pos1);
		if (!tmp.empty())
		{
			vec.push_back(std::move(tmp));
		}

		pos1 = pos2 + c.size();
		pos2 = str.find(c, pos1);
	}
	if (pos1 < str.length())
	{
		vec.push_back(str.substr(pos1));
	}
	return vec;
}

static std::string getPhoneType(CARDTYPE type)
{
	assert(type > UNKNOWN && type <= CMCC_V);

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
{
	std::wstring path = L"phone.dat";
	std::ifstream stream(path, std::ios::binary);
	if (!stream.is_open())
	{
		return;
	}
	buffer = std::vector<char>(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
	head = *reinterpret_cast<DataHead *>(buffer.data());
	recordCount = (buffer.size() - head.offset) / PHONE_INDEX_LENGTH;
}

PhoneInfo PhoneData::lookUp(int64_t phone) const
{
	assert(phone >= 1000000 && phone <= 99999999999);
	while (phone > 9999999)
	{
		phone /= 10;
	}

	return _lookUp(static_cast<uint32_t>(phone));
}

PhoneInfo PhoneData::lookUp(const std::string &phone) const
{
	assert(phone.size() >= 7 && phone.size() <= 11);

	return _lookUp(std::stoul(phone.substr(0, 7)));
}

PhoneInfo PhoneData::_lookUp(uint32_t phone7) const
{
	assert(phone7 >= 1000000 && phone7 <= 99999999999);

	size_t left = 0;
	size_t right = recordCount;

	while (left <= right)
	{
		size_t middle = (left + right) / 2;
		size_t currentOffset = head.offset + middle * PHONE_INDEX_LENGTH;
		if (currentOffset >= buffer.size())
		{
			return PhoneInfo();
		}

		auto _buffer = std::vector<char>(buffer.cbegin() + currentOffset, buffer.cbegin() + currentOffset + PHONE_INDEX_LENGTH);
		Record _record = *reinterpret_cast<Record *>(_buffer.data());

		if (_record.phone > phone7)
		{
			right = middle - 1;
		}
		else if (_record.phone < phone7)
		{
			left = middle + 1;
		}
		else
		{
			std::string recordContent = getRecordContent(buffer, _record.offset);
			std::vector <std::string> contents = ssplit(recordContent, std::string("|"));
			PhoneInfo info;
			info.type = static_cast<CARDTYPE>(_record.type);
			info.phone = _record.phone;
			info.province = contents[PROVINCE];
			info.city = contents[CITY];
			info.zipCode = contents[ZIPCODE];
			info.areaCode = contents[AREACODE];

			return info;
		}
	}

	return PhoneInfo();
}

std::string PhoneData::getRecordContent(const std::vector<char> &buffer, size_t startOffset)
{
	size_t endOffset = std::find(buffer.cbegin() + startOffset, buffer.cend(), '\0') - buffer.cbegin();
	return std::string(buffer.cbegin() + startOffset, buffer.cbegin() + endOffset);
}
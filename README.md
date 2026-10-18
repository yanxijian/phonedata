# phonedata · 手机号码归属地查询

C++ 实现的手机号归属地查询库，读取 `phonedata/phone.dat` 这份紧凑的二进制号段库，
支持按 7 位号段或完整 11 位号码查询省份、城市、邮编、长途区号与运营商类型。

> ## ⚠️ 数据已停止更新
>
> 库内数据版本为 **`1707`（2017 年 7 月）**，共 364,528 条索引，此后未再更新。
> 中国运营商号段这 8 年间已有大量调整（新号段、虚拟运营商、携号转网），
> **请不要把它当作生产环境的归属地数据源**。
>
> 这个仓库现在的价值是：**phone.dat 的二进制格式规范** + 一份可直接复用的解析实现。
> 如果你有新的号段数据，按下面的格式生成新的 `.dat` 即可直接替换。

## 快速开始

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

`phonedata_test` 会直接读取仓库内的 `phonedata/phone.dat`，不需要复制文件。

## API

```cpp
#include "phonedata.h"

PhoneData data;                                   // 读取当前目录下的 phone.dat
// PhoneData data("C:/data/phone.dat");           // 或显式指定路径

if (!data.isOpen()) {
    // 文件缺失 / 截断 / 偏移非法
}

PhoneInfo info = data.lookUp(18607551234LL);      // 11 位、7 位或 int64_t 均可
// info.type     == CUCC（中国联通）
// info.province == "广东"
// info.city     == "深圳"
// info.zipCode  == "518000"
// info.areaCode == "0755"

data.lookUp("18607551234");                       // 字符串入口，非法输入返回空结果
data.lookUp("abc");                               // 不抛异常，type == UNKNOWN
```

| 成员 | 说明 |
| --- | --- |
| `PhoneData()` | 读取当前工作目录下的 `phone.dat`（保持旧行为） |
| `PhoneData(path)` | 读取指定路径 |
| `isOpen()` | 是否成功加载并通过完整性检查 |
| `version()` | 数据版本号，例如 `"1707"` |
| `recordCount()` | 索引条数 |
| `lookUp(int64_t)` / `lookUp(const std::string&)` | 查询；未命中返回 `type == UNKNOWN` 的空结果 |

**编码约定**：`phone.dat` 中的省/市名和本库返回的字符串 **全部是 UTF-8**。
Windows 控制台默认不是 UTF-8，直接 `printf` 会看到乱码，这不是数据问题。
测试程序与 CMake 已为 MSVC 打开 `/utf-8`。

**健壮性**：文件缺失、长度不足 8 字节、首个索引偏移越界、索引区长度不是 9 的整数倍时，
构造函数会把对象置为 `isOpen() == false`，之后所有查询返回空结果，不抛异常、不越界访问。

## phone.dat 格式

文件由三部分组成，整数均为 **小端序**：

```
+------------------+ 0
| version  4 bytes |  版本号 ASCII，例如 "1707" 表示 2017 年 7 月
+------------------+ 4
| offset   4 bytes |  第一个索引项的偏移 = 记录区长度 + 8
+------------------+ 8
| 记录区            |  每条记录形如 "<省>|<市>|<邮编>|<区号>\0"，以 NUL 结束
+------------------+ offset
| 索引区            |  每条索引固定 9 字节
+------------------+ EOF
```

索引项（9 字节，必须按 1 字节对齐读取）：

| 偏移 | 类型 | 含义 |
| --- | --- | --- |
| 0 | `uint32` | 手机号前 7 位 |
| 4 | `uint32` | 对应记录在记录区内的偏移 |
| 8 | `uint8` | 卡片类型：1 移动 / 2 联通 / 3 电信 / 4 电信虚拟 / 5 联通虚拟 / 6 移动虚拟 |

索引区按 7 位号段 **严格升序** 排列，因此可以用二分查找定位；`tools/validate_phonedata.py`
会校验这一不变量以及记录偏移、字段个数、类型编码是否合法：

```powershell
python tools/validate_phonedata.py phonedata/phone.dat --expect-count 364528 --expect-version 1707
```

## 目录结构

```
phonedata/
├── phonedata/              库源码
│   ├── phonedata.h         PhoneData / PhoneInfo / phone.dat 结构定义
│   ├── phonedata.cpp       加载、校验与二分查找
│   ├── test.cpp            自测程序（也是旧 .sln 的入口）
│   └── phone.dat           数据文件（364,528 条，版本 1707）
├── tools/
│   └── validate_phonedata.py   独立的数据完整性校验脚本
├── CMakeLists.txt          推荐构建方式（静态库 + 测试 + CTest）
└── phonedata.sln           旧版 VS2015 工程，保留兼容
```

## 构建

### CMake（推荐，任意平台）

要求 CMake 3.15+ 与支持 C++17 的编译器：

```powershell
cmake -S . -B build -DPHONEDATA_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

### Visual Studio（旧工程）

直接打开 `phonedata.sln`，选择 `Release|x64` 编译即可；该工程把 `test.cpp` 作为入口，
生成的自测程序需要在 `phonedata/` 目录下运行才能找到 `phone.dat`。

## 已知限制

1. **数据陈旧**：见文首说明，2017 年 7 月之后未更新。
2. **只按前 7 位匹配**：号段划分粒度是 7 位，同一号段内的差异无法体现。
3. **虚拟运营商类型缺失**：现有数据只用到类型 1/2/3，4/5/6 的枚举值已定义但数据中没有。
4. **不处理携号转网**：归属地按号段而非当前运营商返回。
5. **仅小端序**：格式本身按小端序定义，大端平台需要自行转换。

## 许可证

代码以 MIT 许可证发布，详见 [LICENSE](LICENSE)。`phone.dat` 数据按原样提供，不附带任何担保。

---

## English summary

A small C++17 library that queries Chinese mobile-phone number attribution from the compact
binary database `phonedata/phone.dat`. The dataset is **frozen at 2017-07** (version `1707`,
364,528 records) and is meant as a **format reference and parsing implementation**, not as a
production data source. Strings are UTF-8. Malformed files fail safely (`isOpen() == false`)
instead of crashing. Build with CMake + CTest.

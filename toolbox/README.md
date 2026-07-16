# tbx - AI 工具箱

纯 C/C++ 实现的命令行工具集，弥补内置 Grep/LS 工具的可靠性问题。单可执行文件 `tbx.exe`，子命令模式。

## 编译

```bat
cd d:\precious_speed\toolbox\src
g++ -O2 -std=c++17 -s -o ..\tbx.exe main.cpp cmd_split.cpp cmd_sam.cpp cmd_search.cpp cmd_es.cpp cmd_file.cpp -lwinhttp
```

或直接运行 `build.bat`。依赖：MinGW-w64 g++ (C++17)，WinHTTP（Windows 自带）。

## 命令总览

```
tbx <command> [options]
```

| 命令 | 功能 | 替代内置工具 |
|------|------|-------------|
| split | 文本拆分分析器（字符类分类/token 化） | 无 |
| sam | SAM 后缀自动机搜索（次数/位置/通配符） | Grep（更可靠） |
| regex | 正则表达式搜索（行/字节模式） | Grep |
| glob | 通配符文件名搜索（递归目录） | Glob |
| es | Everything HTTP 搜索（localhost:21465） | 无 |
| tree | 目录树打印（递归/深度/大小） | LS（更完整） |
| head | 文件头部采样（前 N 行/字节） | Read(limit) |
| lines | 按行号范围读取（带行号） | Read(offset,limit) |
| hex | 十六进制查看器 | 无 |
| wc | 统计行数/单词数/字节数 | 无 |

全局选项：`-h/--help` 显示帮助，`-json` JSON 输出（split/sam/regex/glob/es 支持）。

---

## split - 文本拆分分析器

按字符类型（大写/小写/数字/空白/标点/控制符/非ASCII）分类统计，支持 token 化输出。

```
tbx split <file> [-json] [-tokens] [-class]
```

| 选项 | 说明 |
|------|------|
| `-json` | JSON 格式输出 |
| `-tokens` | 输出所有 token（连续同类字符为一个 token） |
| `-class` | 按字符类分组输出连续 token |

**示例：**
```bat
tbx split div_work.cpp
tbx split div_work.cpp -class
tbx split div_work.cpp -json -tokens
```

**输出字段：** UPPER / LOWER / DIGIT / WHITESPACE / PUNCT / CTRL / HIGH(0x80+) / OTHER

---

## sam - SAM 后缀自动机搜索

基于后缀自动机（Suffix Automaton）的文本搜索。构建一次 SAM 后可多次查询，O(n) 构建，O(m) 每次查询。支持通配符。

```
tbx sam <file> <pattern> [options]
```

| 选项 | 说明 |
|------|------|
| `-all` | 输出所有出现位置（默认只输出次数和首次位置） |
| `-count` | 只输出出现次数 |
| `-wild` | 启用通配符（`*` 任意长度，`?` 单字符） |
| `-line` | 输出行号（1-based）而非字节偏移 |
| `-max N` | 最多输出 N 个位置（默认 1000） |
| `-json` | JSON 输出 |

**示例：**
```bat
REM 精确搜索，输出所有位置（行号）
tbx sam div_work.cpp "absInvNewton" -all -line

REM 只输出次数
tbx sam div_work.cpp "absDivBasicCore" -count

REM 通配符搜索（* 任意长度，? 单字符）
tbx sam div_work.cpp "abs*Core" -wild -all -line

REM JSON 输出
tbx sam div_work.cpp "FFT" -all -json -max 5
```

**算法说明：**
- SAM 在线构建，O(n) 时间空间，状态数 ≤ 2n
- 出现次数：link 树子树叶节点数（桶排序 + 倍增累加）
- 所有位置：link 树 DFS 收集叶节点 first_pos
- 通配符 `*`：按 `*` 分割为子串，各子串 SAM 查询后贪心组合
- 通配符 `?`：子串内按 `?` 拆为字面片段，选最长片段作锚点 SAM 查询后验证间隔

**性能参考：** 83KB 文件构建 SAM 约 33ms（136K 状态），查询 <1ms。

**可靠性：** SAM 基于精确字节匹配，不受正则引擎/编码问题影响。实测 Grep 报告行号偏差时，SAM 结果准确。

---

## regex - 正则表达式搜索

基于 `std::regex`（ECMAScript 语法），支持行模式和字节模式。

```
tbx regex <file> <pattern> [options]
```

| 选项 | 说明 |
|------|------|
| `-count` | 只输出匹配行数 |
| `-i` | 忽略大小写 |
| `-raw` | 字节模式（允许跨行，默认行模式） |
| `-max N` | 最多输出 N 个匹配（默认 1000） |
| `-json` | JSON 输出 |

**示例：**
```bat
REM 行模式搜索（输出 文件:行号:内容）
tbx regex div_work.cpp "absInv\w+"

REM 忽略大小写
tbx regex div_work.cpp "fft" -i

REM 跨行字节模式
tbx regex div_work.cpp "abs.*Newton" -raw -max 5
```

**输出格式：** `文件路径:行号:行内容`

---

## glob - 通配符文件名搜索

递归遍历目录，通配符匹配文件名。

```
tbx glob <dir> <pattern> [options]
```

| 选项 | 说明 |
|------|------|
| `-dir` | 同时匹配目录名 |
| `-depth N` | 最大递归深度（-1 无限） |
| `-json` | JSON 输出 |

**通配符：** `*` 任意长度，`?` 单字符，`[abc]` 字符集

**示例：**
```bat
tbx glob d:\precious_speed\mason_opt "*.cpp"
tbx glob d:\precious_speed "*.h" -depth 2
tbx glob d:\precious_speed "test*" -json
```

---

## es - Everything HTTP 搜索

调用 Everything 的 HTTP 服务（端口 21465），返回格式化结果。需先开启 Everything → 工具 → HTTP 服务器。

```
tbx es <query> [options]
```

| 选项 | 说明 |
|------|------|
| `-count N` | 结果数（默认 50） |
| `-offset N` | 偏移 |
| `-size` | 显示文件大小 |
| `-no-path` | 不显示路径（只显示文件名） |
| `-json` | 格式化 JSON 输出 |
| `-raw` | 原始 JSON（Everything 返回） |

**示例：**
```bat
tbx es "div_work.cpp"
tbx es "*.cpp path:mason_opt" -count 20 -size
tbx es "size:>1mb" -json
tbx es "test" -raw
```

**说明：** Everything 搜索语法支持 `path:`, `size:`, `ext:`, `dm:` 等修饰符，详见 Everything 文档。本工具自动请求 `path_column=1` 获取完整路径。

---

## tree - 目录树打印

递归打印目录结构，支持深度限制和文件大小显示。

```
tbx tree [dir] [-depth N] [-size]
```

| 选项 | 说明 |
|------|------|
| `-depth N` | 最大深度（-1 无限） |
| `-size` | 显示文件大小 |

**示例：**
```bat
tbx tree d:\precious_speed\toolbox
tbx tree d:\precious_speed -depth 2 -size
```

**输出格式：** 树形 ASCII（`|--` / `` `-- ``），末尾统计目录/文件数。

---

## head - 文件头部采样

读取文件前 N 行或 N 字节。

```
tbx head <file> [-n lines | -c bytes]
```

| 选项 | 说明 |
|------|------|
| `-n N` | 前 N 行（默认 10） |
| `-c N` | 前 N 字节 |

**示例：**
```bat
tbx head div_work.cpp -n 20
tbx head div_work.cpp -c 512
```

---

## lines - 按行号范围读取

读取指定行号范围，带行号前缀输出。

```
tbx lines <file> [start] [end] [-no-num]
```

| 参数 | 说明 |
|------|------|
| `start` | 起始行号（1-based，默认 1） |
| `end` | 结束行号（默认 = start） |
| `-no-num` | 不显示行号前缀 |

**示例：**
```bat
REM 读取第 1692-1700 行
tbx lines div_work.cpp 1692 1700

REM 读取第 100 行（单行）
tbx lines div_work.cpp 100

REM 不显示行号
tbx lines div_work.cpp 1 10 -no-num
```

**输出格式：** `行号| 内容`

---

## hex - 十六进制查看器

以十六进制 + ASCII 形式查看文件内容。

```
tbx hex <file> [-n bytes] [-o offset]
```

| 选项 | 说明 |
|------|------|
| `-n N` | 显示 N 字节（默认 256） |
| `-o N` | 起始偏移（默认 0） |

**示例：**
```bat
tbx hex tbx.exe -n 64
tbx hex data.bin -o 1024 -n 512
```

**输出格式：** `偏移  十六进制(16字节/行)  |ASCII|`

---

## wc - 统计工具

统计文件的行数、单词数、字节数。

```
tbx wc <file>
```

**示例：**
```bat
tbx wc div_work.cpp
```

**输出格式：** `行数  单词数  字节数  文件名`

---

## 设计说明

### 为什么用 SAM？

后缀自动机（SAM）是表示字符串所有子串的最紧凑自动机：
- O(n) 构建，状态数 ≤ 2n，转移数 ≤ 3n
- 任意子串出现次数/位置查询 O(m)（m 为模式长度）
- 支持多次查询（构建一次，查任意多模式）
- 二进制安全（字节 alphabet，不依赖编码）

相比 KMP/BM 单次搜索，SAM 适合"在一大文本中搜索多个模式"的场景。相比后缀数组，SAM 在线构建更灵活。

### 通配符实现

- `*`：模式按 `*` 分割为子串 segments，每个 segment 用 SAM 查找所有出现位置，然后贪心匹配各 segment 顺序（每个 segment 取最早匹配位置）
- `?`：segment 内按 `?` 拆为字面片段，选最长片段作锚点 SAM 查询，再验证片段间隔是否等于 `?` 数量
- 全 `*` 或空模式匹配所有位置

### Everything HTTP API

Everything 提供 HTTP 服务（默认端口 21465）：
- `GET /?search=<query>&json=1&count=N&path_column=1&size_column=1`
- 返回 JSON：`{ "totalResults": N, "results": [{ "type":"file", "name":"...", "path":"...", "size":... }] }`
- 本工具用 WinHTTP 发送请求，内置 JSON 解析器格式化输出

### 文件结构

```
toolbox/
├── src/
│   ├── common.h          # 公共功能（文件读取/字符分类/SAM/通配符/HTTP/JSON）
│   ├── main.cpp          # 子命令分发
│   ├── cmd_split.cpp     # 文本拆分分析器
│   ├── cmd_sam.cpp       # SAM 搜索 + 通配符
│   ├── cmd_search.cpp    # 正则搜索 + glob 文件搜索
│   ├── cmd_es.cpp        # Everything HTTP 搜索 + JSON 解析器
│   └── cmd_file.cpp      # tree/head/lines/hex/wc
├── build.bat             # 编译脚本
├── tbx.exe               # 编译产物
└── README.md             # 本文档
```

---

## 已知限制

- SAM 内存：状态数 ≤ 2n，每状态含 `unordered_map`（约 56 字节空表），1MB 文件约需 120MB 内存
- `sam -wild` 的 `?` 多时可能退化（锚点片段少时候选位置多）
- `es` 依赖 Everything 运行且开启 HTTP 服务
- `regex -raw` 跨行模式对大文件可能较慢（std::regex 性能限制）
- 路径含非 ASCII 字符时，`glob`/`tree` 用 ANSI API，可能显示乱码（`es` 不受影响，用 Everything 索引）

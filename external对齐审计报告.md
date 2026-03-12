# 当前 C++ 项目对齐 `external/` 的全面审计报告

日期：2026-03-12

## 0. 对齐进度更新

### 2026-03-12 第 1 个已落地点

本轮已完成：

- `analyze` 现在可直接接受 `.w3x/.w3m` 或目录输入
- `extract` 现在可直接接受 `.w3x/.w3m` 或目录输入
- `convert` 现在可直接接受 `.w3x/.w3m` 或目录输入
- `ResourceExtractor` 已改为复用统一的 archive 抽象，目录和 MPQ 都走 `parser::w3x::OpenArchive()` 读取
- 已补充 smoke test，覆盖真实 packed map 的 `analyze/extract/convert`

对应代码：

- `cli/commands/map_input_utils.h`
- `cli/commands/map_input_utils.cc`
- `cli/commands/analyze_command.cc`
- `cli/commands/extract_command.cc`
- `cli/commands/convert_command.cc`
- `converter/resource_extract/resource_extractor.h`
- `converter/resource_extract/resource_extractor.cc`
- `tests/smoke_tests.cc`

已验证：

```powershell
ctest --test-dir build2 -C Release --output-on-failure
```

说明：

- 这次完成的是“输入模型第一阶段对齐”。
- 还没有完成 `external` 那种完整统一转换管线，尤其是 `convert` 的真实语义转换、工作区布局统一和 `Lni/Obj/Slk` 三模式仍未对齐。

### 2026-03-12 第 2 至第 4 个已落地点

本轮继续完成：

- CLI 新增 `help`、`version`、`lni` 三个对齐入口
- 运行时数据已优先改用仓库内 `data/`，不再把 `external/` 当成唯一来源
- `convert` 工作区已补齐 `.w3x` marker、`w3x2lni/locale`、`table/w3i.ini`、`table/imp.ini`
- `convert` 已把模型/贴图输出到 `resource/`，声音输出到 `sound/`
- 对象打包已从 `w3a/w3h/w3u/w3t/w3q` 扩到 `w3b/w3d`
- smoke test 已覆盖上述新增行为

新增/更新代码：

- `cli/cli_app.cc`
- `cli/commands/help_command.h`
- `cli/commands/help_command.cc`
- `cli/commands/version_command.h`
- `cli/commands/version_command.cc`
- `cli/commands/lni_command.h`
- `cli/commands/lni_command.cc`
- `parser/w3x/workspace_layout.h`
- `parser/w3x/workspace_layout.cc`
- `parser/w3x/object_pack_generator.h`
- `parser/w3x/object_pack_generator.cc`
- `converter/w3x2lni/w3x_to_lni.cc`
- `tests/smoke_tests.cc`
- `data/locale/**`
- `data/zhCN-1.32.8/prebuilt/**`

已验证：

```powershell
ctest --test-dir build2 -C Release --output-on-failure
.\build2\bin\Release\w3x_tool.exe --help
.\build2\bin\Release\w3x_tool.exe help lni
.\build2\bin\Release\w3x_tool.exe version
```

### 2026-03-12 第 5 至第 8 个已落地点

本轮继续完成：

- CLI 新增 `config`、`template`、`log` 三个真实命令
- `core/config` 已从通用 JSON 容器改为纯 C++ 分层 INI 配置管理器
- `convert` 现在会生成 `w3x2lni/config.ini`，并把默认配置/全局配置接进转换流程
- locale 与 `prebuilt` 运行时选择已开始读取配置中的 `global.lang` / `global.data`
- 新增 `core/config/runtime_paths.*` 与 `Platform::GetExecutablePath()`，运行时会从当前目录、可执行文件目录、源码目录解析 `data/`
- 安装规则已补充 `data/` 打包，不再只适合源码树直接运行
- smoke test 已覆盖 `config/template/log` 与 `w3x2lni/config.ini`

新增/更新代码：

- `core/config/config_manager.h`
- `core/config/config_manager.cc`
- `core/config/runtime_paths.h`
- `core/config/runtime_paths.cc`
- `core/platform/platform.h`
- `core/platform/platform.cc`
- `core/CMakeLists.txt`
- `data/default_config.ini`
- `cli/commands/config_command.h`
- `cli/commands/config_command.cc`
- `cli/commands/template_command.h`
- `cli/commands/template_command.cc`
- `cli/commands/log_command.h`
- `cli/commands/log_command.cc`
- `cli/commands/convert_command.cc`
- `parser/w3x/workspace_layout.cc`
- `parser/w3x/object_pack_generator.cc`
- `tests/smoke_tests.cc`
- `CMakeLists.txt`

已验证：

```powershell
cmake --build build2 --config Release --target w3x_tool w3x_smoke_tests
ctest --test-dir build2 -C Release --output-on-failure
.\build2\bin\Release\w3x_tool.exe help config
.\build2\bin\Release\w3x_tool.exe help template
.\build2\bin\Release\w3x_tool.exe log --path
```

说明：

- 这一轮完成的是“配置系统第一阶段对齐 + 运行时路径第二阶段对齐 + CLI 第二阶段扩面”。
- 仍然没有完成 `obj/slk/mpq/test` 剩余命令面，也还没有把大部分配置项真正接入语义级转换。

### 2026-03-12 第 9 个已落地点

本轮继续完成：

- `convert` 不再把对象二进制简单改扩展名伪装成 `.ini`
- 已新增纯 C++ 反向派生链：
  - `war3map.w3a/w3b/w3d/w3h/w3q/w3t/w3u -> ability/destructable/doodad/buff/upgrade/item/unit.ini`
  - `war3mapmisc.txt -> misc.ini`
- metadata 中无法识别的对象字段，现会以 `raw(field,type,level,data_pointer)` 语法落盘
- `pack` 现已能重新读取上述 `raw(...)` 字段并回生成原始对象修改
- `convert` 与 `unpack` 的 table 侧分类规则更接近，不再把任意 `.txt/.w3*` 误写成假 `.ini`
- smoke test 已覆盖：
  - `war3map.w3a -> ability.ini`
  - `war3mapmisc.txt -> misc.ini`
  - 未知字段 `raw(...)` 的 `convert -> pack -> unpack` 回环

新增/更新代码：

- `parser/w3x/object_pack_generator.h`
- `parser/w3x/object_pack_generator.cc`
- `converter/w3x2lni/w3x_to_lni.cc`
- `tests/smoke_tests.cc`

已验证：

```powershell
cmake --build build2 --config Release --target w3x_tool w3x_smoke_tests
ctest --test-dir build2 -C Release --output-on-failure
```

说明：

- 这一轮完成的是“对象表数据第二阶段对齐”。
- 仍未达到 `external` 的完整 `Lni/Obj/Slk` 语义体系，也还没有补齐 `txt.ini`、`WTG/WCT/LML` 等大缺口。

### 2026-03-12 第 10 个已落地点

本轮继续完成：

- CLI 新增 `test` 命令
- `test` 会自动发现并执行 `w3x_smoke_tests`，不再只能靠外部手工调用 `ctest` 或测试可执行文件

新增/更新代码：

- `cli/commands/test_command.h`
- `cli/commands/test_command.cc`
- `cli/cli_app.cc`

已验证：

```powershell
.\build2\bin\Release\w3x_tool.exe help test
.\build2\bin\Release\w3x_tool.exe test
```

说明：

- 这是命令面对齐的补强项，不改变核心转换语义。

### 2026-03-12 第 11 个已落地点

本轮继续完成：

- CLI 新增 `obj` 命令，不再只有 `pack` 这一层通用归档入口
- `obj` 命令可接受目录或 packed map 输入，并输出 Obj 风格的 `.w3x`
- `unpack` 现在会在工作区侧自动从：
  - `war3map.w3a/w3b/w3d/w3h/w3q/w3t/w3u`
  - `war3mapmisc.txt`
  派生出对应 `table/*.ini`
- `pack` 现在会过滤 source-only 的对象表文件：
  - `ability.ini`
  - `destructable.ini`
  - `doodad.ini`
  - `buff.ini`
  - `upgrade.ini`
  - `item.ini`
  - `unit.ini`
  - `misc.ini`
  不再把这些源文件原样塞进归档，同时依赖合成出的 `war3map.w3*` / `war3mapmisc.txt`
- smoke test 已覆盖：
  - `obj` 命令成功产出 `.w3x`
  - raw archive 中不再残留 `ability.ini/misc.ini`
  - `unpack` 仍可从 Obj 输出重新派生 `table/ability.ini`

新增/更新代码：

- `cli/commands/obj_command.h`
- `cli/commands/obj_command.cc`
- `cli/cli_app.cc`
- `parser/w3x/workspace_layout.cc`
- `parser/w3x/map_archive_io.cc`
- `tests/smoke_tests.cc`

已验证：

```powershell
cmake --build build2 --config Release --target w3x_tool w3x_smoke_tests
ctest --test-dir build2 -C Release --output-on-failure
.\build2\bin\Release\w3x_tool.exe help obj
```

说明：

- 这一轮完成的是“Obj 子链第一阶段对齐”。
- `slk` 命令与真正的 Slk 目标格式语义仍然缺失。

### 2026-03-12 第 12 个已落地点

本轮继续完成：

- CLI 新增 `slk` 命令
- 打包层新增可区分 `obj/slk` 的 `PackOptions`
- `slk` 命令不再只是 `obj` 的换名入口，而是开始具备真实的 Slk 侧输出选择规则：
  - 当工作区/解包目录中已存在对应 `units/*.slk` / `doodads/*.slk`
  - 且不存在 `ability.ini/unit.ini/...` 这类当前纯 C++ 仍需靠 Obj 承载的源数据时
  - `slk` 模式会抑制冗余的 `war3map.w3a/w3h/w3u/w3t/w3q`，以及在 `slk.slk_doodad=true` 时抑制 `war3map.w3b/w3d`
- 一旦存在 `ability.ini` 等 ini 源，`slk` 模式仍会回退生成对应 `war3map.w3*`，避免把当前纯 C++ 无法下沉到 SLK 的对象语义直接丢掉
- smoke test 已覆盖：
  - `obj` 与 `slk` 对同一工作区产生可观察差异
  - `slk` 会保留 `units/abilitydata.slk` 并压掉冗余 `war3map.w3a`
  - `slk` 在存在 `ability.ini` 时仍会生成可解析的 `war3map.w3a`

新增/更新代码：

- `parser/w3x/map_archive_io.h`
- `parser/w3x/map_archive_io.cc`
- `cli/commands/slk_command.h`
- `cli/commands/slk_command.cc`
- `cli/commands/obj_command.cc`
- `cli/cli_app.cc`
- `tests/smoke_tests.cc`

已验证：

```powershell
cmake --build build2 --config Release --target w3x_tool w3x_smoke_tests
ctest --test-dir build2 -C Release --output-on-failure
.\build2\bin\Release\w3x_tool.exe help slk
```

说明：

- 这一轮完成的是“Slk 子链第一阶段对齐”。
- 目前只是把 archive 产物选择规则推进到 `external` 的同方向，并没有完成 `external` 里的完整 `SLK + TXT + WTS + WTG/WCT` 语义链。

### 2026-03-12 第 13 个已落地点

本轮继续完成：

- `slk` 打包链新增独立模块：
  - `parser/w3x/slk_pack_generator.*`
  - `parser/w3x/txt_pack_generator.*`
  - `parser/w3x/object_cleanup.*`
- `slk` 现在已支持从以下 ini 源直接生成对应 SLK：
  - `ability.ini -> units/abilitydata.slk`
  - `buff.ini -> units/abilitybuffdata.slk`
  - `item.ini -> units/itemdata.slk`
  - `upgrade.ini -> units/upgradedata.slk`
  - `destructable.ini -> units/destructabledata.slk`
  - `doodad.ini -> doodads/doodads.slk`
- 对于可以被上述 SLK 完整覆盖的对象类型，`slk` 现在会真正压掉冗余 `war3map.w3a/w3h/w3t/w3q/w3b/w3d`
- 若对象 ini 中存在 `raw(...)` 这类当前 SLK 无法安全承载的字段，`slk` 会继续保留必要的 `war3map.w3*` sidecar，而不是误报“已完全转成 SLK”
- `slk` 现在会集中托管已识别的 `TXT/WTS` 产物：
  - 已识别 `units/*.txt`
  - `doodads/doodadskins.txt`
  - `war3mapextra.txt`
  - `war3mapskin.txt`
  - `war3map.wts`
- `slk.remove_we_only` 已开始进入真实打包语义：
  - `war3map.wtg`
  - `war3map.wct`
  - `war3map.imp`
  - `war3map.w3s`
  - `war3map.w3r`
  - `war3map.w3c`
  - `war3mapunits.doo`
  会在 `slk` 输出前清理
- smoke test 已覆盖：
  - 现有 `units/*.slk` 覆盖 Obj 文件
  - `ability.ini -> units/abilitydata.slk`
  - 生成的 `abilitydata.slk` 可被纯 C++ `SLK` 解析器读取，并保留 `alias/code`
  - `raw(...)` 触发必要的 Obj sidecar 保留
  - `remove_we_only` 清理 WE-only 文件
  - 已识别 `TXT/WTS` 在 `slk` 输出中被保留

新增/更新代码：

- `parser/w3x/slk_pack_generator.h`
- `parser/w3x/slk_pack_generator.cc`
- `parser/w3x/txt_pack_generator.h`
- `parser/w3x/txt_pack_generator.cc`
- `parser/w3x/object_cleanup.h`
- `parser/w3x/object_cleanup.cc`
- `parser/w3x/map_archive_io.h`
- `parser/w3x/map_archive_io.cc`
- `cli/commands/slk_command.cc`
- `core/config/config_manager.cc`
- `data/default_config.ini`
- `tests/smoke_tests.cc`

已验证：

```powershell
cmake --build build2 --config Release --target w3x_tool w3x_smoke_tests
ctest --test-dir build2 -C Release --output-on-failure
.\build2\bin\Release\w3x_tool.exe help slk
```

说明：

- 这一轮完成的是“Slk 子链第二阶段对齐”。
- 仍未完成：
  - `unit.ini -> 5 份 unit*.slk` 的真正语义拆分
  - `txt.ini` 聚合生成
  - `war3map.wts` 的重建而非透传
  - `remove_unuse_object/computed_text/remove_same` 的真实内容级语义

## 1. 审计目标

本次审计的目标不是判断“当前项目是否可用”，而是判断：

> 如果目标是“**严格对齐 `external/` 参考项目的能力与行为，但实现形态改为纯 C++**”，当前仓库还缺哪些关键能力、哪些地方只是壳层对齐、哪些地方已经在设计上偏离参考项目。

参考对象：

- 当前仓库根目录下的纯 C++ 项目
- `external/` 内的原始参考项目（Lua + C++ 外壳/依赖）

## 2. 审计方法

本次结论来自三类证据：

1. 源码审计  
   重点阅读了：
   - `README.md`
   - `实现计划.md`
   - `CMakeLists.txt`
   - `cli/commands/*.cc`
   - `converter/**/*.cc`
   - `parser/**/*.cc`
   - `gui/**/*.cc`
   - `tests/smoke_tests.cc`
   - `external/README.md`
   - `external/docs/**/*.md`
   - `external/script/backend/**/*.lua`
   - `external/script/share/**/*.lua`
   - `external/script/map-builder/**/*.lua`

2. 命令面实测  
   已实测：

   ```powershell
   .\build2\bin\Release\w3x_tool.exe --help
   .\build2\bin\Release\w3x_tool.exe convert --help
   .\build2\bin\Release\w3x_tool.exe extract --help
   .\build2\bin\Release\w3x_tool.exe analyze --help
   .\build2\bin\Release\w3x_tool.exe unpack --help
   .\build2\bin\Release\w3x_tool.exe pack --help
   .\build2\bin\Release\w3x_tool.exe help config
   .\build2\bin\Release\w3x_tool.exe help template
   .\build2\bin\Release\w3x_tool.exe log --path
   ```

3. 测试实测  
   已实测：

   ```powershell
   ctest --test-dir build --build-config Release --output-on-failure
   ctest --test-dir build2 -C Release --output-on-failure
   ```

   两个构建目录中的 `w3x_smoke_tests` 都通过，但这只能证明当前 smoke coverage 内的流程可跑通，不能证明已对齐 `external/` 的行为细节。

## 3. 总体结论

结论很明确：

- 当前项目**没有严格对齐** `external/`。
- 当前项目更接近于“**纯 C++ CLI 原型 + MPQ 打包/解包 + 目录级工作区搬运 + 部分对象文件合成**”。
- `external/` 的核心价值并不只是“能 unpack/pack/convert”，而是“**围绕 Lni / Obj / Slk 三种格式的完整转换模型、配置系统、数据版本体系、模板体系、插件体系、GUI 工作流，以及大量边界行为单测**”。这一层，当前 C++ 项目绝大多数还没有对齐。

如果按对齐层次拆分，可以这样看：

- 命令壳层：部分对齐
- MPQ 读写基础能力：部分对齐
- 输入模型（目录/MPQ）：已完成第一阶段部分对齐
- 工作区目录结构：已完成第一阶段部分对齐
- 真实对象语义转换：未对齐
- `Lni/Obj/Slk` 三格式模型：未对齐
- 触发器 `WTG/WCT/LML` 管线：未对齐
- 配置/模板/多版本数据/语言包/插件：仍未对齐，但已开始内置化数据资产
- GUI 用户工作流：未对齐
- 行为级测试深度：未对齐

## 4. 当前已经具备或接近具备的部分

这部分不能算“严格对齐完成”，但确实已经形成基础。

### 4.1 纯 C++ 主体和独立构建链已建立

证据：

- `README.md`
- `实现计划.md`
- `CMakeLists.txt`
- `app/main.cpp`

现状：

- 项目已经是纯 C++23 主体。
- 构建链使用 CMake。
- 默认交付目标是 Windows x64。
- CLI 可独立运行。

审计结论：

- 这部分符合“不要依赖 Lua 运行时”的方向。
- 但这只是实现语言和构建方式对齐，不等于行为对齐。

### 4.2 CLI 主命令已经成型

证据：

- `cli/cli_app.cc`
- `cli/commands/convert_command.cc`
- `cli/commands/extract_command.cc`
- `cli/commands/analyze_command.cc`
- `cli/commands/unpack_command.cc`
- `cli/commands/pack_command.cc`

当前命令面：

- `help`
- `version`
- `config`
- `convert`
- `lni`
- `obj`
- `extract`
- `analyze`
- `unpack`
- `pack`
- `template`
- `log`
- `test`

审计结论：

- 当前 C++ 项目已经形成最基础的 CLI 外壳。
- 但与 `external/script/backend/cli/*.lua` 相比，命令面明显更窄。

### 4.3 MPQ 解包/打包主链已经可用

证据：

- `parser/w3x/w3x_parser.cc`
- `parser/w3x/map_archive_io.cc`
- `cli/commands/unpack_command.cc`
- `cli/commands/pack_command.cc`
- `tests/smoke_tests.cc`

现状：

- 已接入 StormLib。
- 已支持 `.w3x/.w3m` 打包与解包。
- 已有 manifest 机制辅助回包。
- smoke test 中已有 `pack -> unpack -> repack -> unpack` 的流程验证。

审计结论：

- 这是当前项目最接近“参考项目核心能力”的部分。
- 但它仍然只覆盖基础归档链路，不等于对齐 `external` 的格式语义和构建模型。

### 4.4 已经有若干基础解析器

证据：

- `parser/w3i/w3i_parser.cc`
- `parser/w3u/w3u_parser.cc`
- `parser/w3u/w3u_writer.cc`
- `parser/slk/slk_parser.cc`
- `parser/jass/jass_parser.cc`

现状：

- 已有 `W3I` 解析。
- 已有 `W3U/W3A/...` 二进制对象文件解析与序列化。
- 已有基础 `SLK` 解析。
- 已有基础 `JASS` 词法/语法解析。

审计结论：

- 这些是实现纯 C++ 对齐的必要前置能力。
- 但当前多数解析器还没有被整合为完整的“参考项目级转换语义”。

## 5. 主要未对齐项

以下是本次审计最重要的结论。

### 5.1 核心格式模型没有对齐：当前没有真正建立 `Lni / Obj / Slk` 三格式体系

参考项目证据：

- `external/docs/zh-cn/README.md`
- `external/docs/en-us/README.md`
- `external/script/backend/cli/lni.lua`
- `external/script/backend/cli/obj.lua`
- `external/script/backend/cli/slk.lua`

当前项目证据：

- `README.md`
- `cli/cli_app.cc`

差异：

- `external` 把地图管理模型明确拆成 `Lni`、`Obj`、`Slk` 三种格式，并围绕三者互转构建整个工具链。
- 当前 C++ 项目没有这套格式模型。
- 当前只有：
  - 目录到“LNI workspace layout”的 `convert`
  - 第一阶段的 `obj` 打包命令
  - archive 到 workspace 的 `unpack`
  - workspace/目录到 archive 的 `pack`

审计结论：

- 这是**最根本的未对齐**。
- 当前项目完成的是“文件工作流”，不是“格式体系对齐”。

### 5.2 `convert` 已从“目录复制/改后缀”推进到“部分对象语义转换”，但仍未对齐参考项目的完整转换语义

当前项目证据：

- `实现计划.md`
- `converter/w3x2lni/w3x_to_lni.cc`
- `converter/w3x2lni/w3x_to_lni.h`

关键事实：

- `实现计划.md` 已明确写出：下一阶段才要把 `w3x -> lni` 从“目录复制 + 基础落盘”升级为真实对象数据转换。
- 当前 `W3xToLniConverter::ConvertTableData()` 已开始做两类真实处理：
  - 复用 bundled metadata，把 `war3map.w3a/w3b/w3d/w3h/w3q/w3t/w3u` 反向导出为 `*.ini`
  - 把 `war3mapmisc.txt` 反向导出为 `misc.ini`
- 同时会按工作区分类规则复制已有 `ability.ini`、`units/*.slk` 等 table 文件，不再盲目改后缀
- `ExtractTriggers()` 当前只是复制 `.j/.lua/.ai`。
- `WriteConfig()` 只写出一个很薄的 `w3x2lni.ini`。

和 `external` 的差异：

- `external/script/backend/convert.lua`
- `external/script/backend/data_load.lua`
- `external/script/share/check_lni_mark.lua`
- `external/data/*/prebuilt/**`
- `external/template/**`

`external` 的转换不是“拷贝文件”，而是“读取地图语义 -> 前端解析 -> 后端转换 -> 目标格式落盘”。

审计结论：

- 这一项已不再是“纯壳层命令”。
- 但它仍然不是 `external` 那种完整的前端/后端语义转换管线。

### 5.3 对象数据打包/回包已推进到第三阶段，但仍远未对齐参考项目

当前项目证据：

- `parser/w3x/object_pack_generator.cc`
- `parser/w3u/w3u_parser.cc`
- `parser/w3u/w3u_writer.cc`
- `parser/slk/slk_parser.cc`

关键事实：

- 本轮之前仅合成以下对象文件：
  - `war3map.w3a`
  - `war3map.w3h`
  - `war3map.w3u`
  - `war3map.w3t`
  - `war3map.w3q`
  - `war3mapmisc.txt`
- 本轮已补齐：
  - `war3map.w3b`
  - `war3map.w3d`
- 本轮继续补齐了反向链：
  - `war3map.w3a -> ability.ini`
  - `war3map.w3b -> destructable.ini`
  - `war3map.w3d -> doodad.ini`
  - `war3map.w3h -> buff.ini`
  - `war3map.w3q -> upgrade.ini`
  - `war3map.w3t -> item.ini`
  - `war3map.w3u -> unit.ini`
  - `war3mapmisc.txt -> misc.ini`
- 对 metadata 无法识别的字段，已新增 `raw(...)` 保底语法，确保 `convert -> pack` 时不直接丢字段
- `pack` 现在会过滤 source-only 的对象源 `*.ini`，更接近 Obj 产物，而不是把源文件和生成物一起塞回归档
- `unpack` 现在会从 `war3map.w3*` / `war3mapmisc.txt` 自动派生出工作区 `table/*.ini`
- `pack/slk` 现在已开始区分 Obj 与 Slk 两类 archive 选择规则：
  - `obj` 仍偏向保留/生成 `war3map.w3*`
  - `slk` 则会在有覆盖性 `units/*.slk` / `doodads/*.slk` 且没有 ini 源时压掉冗余 Obj 文件
- `slk` 现在已开始从 `ability/buff/item/upgrade/destructable/doodad.ini` 真实生成对应 `SLK`
- `slk` 现在已开始集中处理已识别的 `TXT/WTS` 文件，并在 `remove_we_only=true` 时清理 WE-only 地图文件
- 对于带有 `raw(...)` 的对象 ini，`slk` 现在会保留必要的 Obj sidecar，而不是误判为可完全下沉到 SLK
- 仍然没有看到对应：
  - `txt.ini` 的完整回写链
  - `doo/w3s/w3r` 等参考项目 TODO/能力边界中的其它地图语义文件

进一步问题：

- 当前 `PackMapDirectory()` 虽然已经把 `slk` 推进到“ini -> slk + 已识别 txt/wts 托管 + WE-only 清理 + 必要时保留 Obj sidecar”，但它仍然只是第二阶段近似实现。
- 它还没有完成 `external` 里的：
  - `unit` 类型的 5 路 `SLK` 语义拆分
  - `TXT` 脚手架与聚合输出
  - `WTS` 重建
  - `remove_unuse_object/computed_text/remove_same` 等真正影响产物内容的对象清理逻辑
  - `war3mapskin.txt`、`WTG/WCT` 等关联语义收敛

上面这一点属于从源码做出的合理推断，依据是：

- `parser/w3x/map_archive_io.cc`
- `parser/w3x/object_pack_generator.cc`
- `parser/w3x/workspace_layout.cc`

审计结论：

- 当前对象链路已从“只能正向合成一部分 obj 文件”推进到“已有一条可回环的 ini <-> obj 部分链路 + 第二阶段 slk 产物链骨架”。
- 如果目标是严格复现 `external` 的对象行为规则，当前还差很远。

### 5.4 触发器管线基本没有对齐：缺失 `WTG/WCT/LML` 体系

参考项目证据：

- `external/test/unit_test/wtg转lml-旧版本/**`
- `external/test/unit_test/wtg转lml-新版本/**`
- `external/data/*/UI/TriggerData.txt`
- `external/data/*/UI/TriggerStrings.txt`

当前项目证据：

- `converter/w3x2lni/w3x_to_lni.cc`
- `parser/jass/jass_parser.cc`
- `gui/panels/script_panel/script_panel.cc`
- `README.md`

差异：

- `external` 具备触发器语义级转换能力，至少覆盖 `WTG/WCT -> LML`。
- 当前 C++ 项目没有任何 `wtg/wct/lml` 解析或生成模块。
- 当前所谓“trigger 支持”，实质只是把 `.j/.lua/.ai` 文件复制出来。
- `JASS` parser 虽存在，但当前没有接入真正的转换流程。

审计结论：

- 这是**结构性缺口**。
- 只要这块没补，当前项目就不能说“严格对齐 external 的触发器能力”。

### 5.5 命令面：已推进到第三阶段部分对齐，但距离参考项目完整 CLI 仍有差距

参考项目证据：

- `external/script/backend/cli/help.lua`
- `external/script/backend/cli/`

参考项目 CLI 文件：

- `config.lua`
- `help.lua`
- `lni.lua`
- `log.lua`
- `mpq.lua`
- `obj.lua`
- `pack.lua`
- `slk.lua`
- `template.lua`
- `test.lua`
- `unpack.lua`
- `version.lua`

当前项目证据：

- `cli/cli_app.cc`
- `cli/commands/`

当前 C++ 项目目前已补齐：

- `help`
- `version`
- `lni`
- `config`
- `obj`
- `slk`
- `template`
- `log`
- `test`

剩余差异：

- 当前没有：
  - `mpq` 型工具命令

审计结论：

- 这一项已从“第二阶段可用命令面对齐”推进到“第三阶段命令面基础对齐 + slk 第二阶段语义起步”。
- 但离 `external` 的完整命令面仍然有明显差距。

### 5.6 配置系统：已完成第一阶段纯 C++ 分层 INI 骨架，但距离参考项目完整语义仍有差距

参考项目证据：

- `external/config.ini`
- `external/script/share/config.lua`
- `external/script/share/config.ini`
- `external/script/share/config_define.lua`
- `external/script/backend/cli/config.lua`

当前项目证据：

- `core/config/config_manager.h`
- `core/config/config_manager.cc`
- `core/config/runtime_paths.h`
- `core/config/runtime_paths.cc`
- `cli/commands/config_command.cc`
- `cli/commands/convert_command.cc`
- `data/default_config.ini`

当前已完成的对齐：

- 已形成：
  - 默认配置 `data/default_config.ini`
  - 全局配置 `config.ini`
  - 地图工作区配置 `w3x2lni/config.ini`
- 新增 `config` CLI，可查看和更新默认/全局/地图三级合并后的值
- `convert` 现在会输出 `w3x2lni/config.ini`
- locale 与 `prebuilt` 的运行时选择，已经开始通过 `global.lang` / `global.data` 联动

仍然存在的差异：

- `external` 还有配置项注释、显示规则、严格校验、自动打开当前地图等 CLI 语义。
- 当前 C++ 版虽然有类型校验，但还没有把 `config_define.lua` 的说明文本和完整约束全部移植过来。
- 更关键的是：大部分配置项仍未真正驱动语义级转换，只是开始接进运行时资源选择和工作区配置落盘。

审计结论：

- 这一项已从“完全未对齐”推进到“第一阶段部分对齐”。
- 但还不能宣称等价于 `external` 的配置体系。

### 5.7 模板、预构建索引、多版本数据仓、语言包、插件体系仍未对齐，但已开始内置化

参考项目证据：

- `external/template/**`
- `external/data/zhCN-1.32.8/**`
- `external/data/enUS-1.27.1/**`
- `external/data/zhCN-1.24.4/**`
- `external/script/share/data_version.lua`
- `external/script/share/lang.lua`
- `external/script/locale/**`
- `external/script/backend/plugin.lua`
- `external/script/backend/cli/template.lua`
- `external/script/backend/cli/mpq.lua`

当前项目证据：

- `README.md`
- `core/config/config_manager.*`
- `cli/commands/template_command.cc`
- `data/default_config.ini`
- `parser/w3x/object_pack_generator.cc`
- `parser/w3x/workspace_layout.cc`
- `CMakeLists.txt`

当前已完成的改进：

- 已把 `prebuilt` 和 locale 资源复制到仓库内 `data/`
- 运行时现在优先从 `data/` 读取，而不是直接绑定 `external/`
- 已新增 `template` 命令，可直接导出 bundled `Melee/Custom` 模板
- 安装规则现在会把 `data/` 随可执行文件一起打包
- 数据版本选择已开始读取配置中的 `global.data`

剩余差异：

- `external` 有完整的数据版本仓、预构建 metadata、模板输出、语言包和插件机制。
- 当前仓库仍然没有对应的纯 C++ 插件架构和完整数据版本管理器。
- 当前内置的仍然只是少量静态资源和单一主版本，不是 `external` 那种多版本数据仓。

审计结论：

- 这块仍然是“参考项目的基础设施层”缺口。
- 但当前 C++ 项目已经从“只会引用资源”推进到“会打包、会导出、会按配置选择资源”。

### 5.8 运行时数据依赖：已进一步推进到“支持相对可执行文件解析 data，并保留 external 兼容回退”

这是本次审计发现的一个非常关键的问题。

项目自述证据：

- `README.md` 明确写了：`external/` “仅用于对照功能与行为，不作为新工程的运行时依赖”。

实际源码证据：

- `core/config/runtime_paths.cc`
- `core/platform/platform.cc`
- `parser/w3x/object_pack_generator.cc`
- `parser/w3x/workspace_layout.cc`
- `CMakeLists.txt`

旧问题：

- 之前 `object_pack_generator.cc` 和 `workspace_layout.cc` 会直接从 `external/` 读取运行时元数据和 locale。

当前状态：

- 本轮已新增：
  - `data/zhCN-1.32.8/prebuilt/**`
  - `data/locale/zhCN/**`
  - `data/locale/enUS/**`
- 运行时现在优先从 `data/` 读取 bundled 资源。
- 运行时路径现在会从：
  - 当前工作目录
  - 可执行文件目录
  - 源码目录
  依次解析 `data/`
- `data/` 现在已加入安装规则，可随二进制一起发布
- `external/` 仍保留为回退来源，用于兼容当前开发环境。

审计结论：

- 这一项已从“只能在源码树里跑”推进到“开始具备独立打包能力”。
- 仍未达到完全独立发布，因为代码还保留 `external/` 回退路径，且 bundled 数据版本仍然偏少。

### 5.9 工作区分类：已推进到第三阶段收敛，但还没有完全统一

当前项目证据：

- `converter/w3x2lni/w3x_to_lni.cc`
- `parser/w3x/workspace_layout.cc`
- `parser/w3x/map_archive_io.cc`
- `tests/smoke_tests.cc`

本轮已完成的收敛：

- `convert` 现在会创建：
  - `.w3x`
  - `map/`
  - `resource/`
  - `sound/`
  - `table/`
  - `trigger/`
  - `w3x2lni/`
- `convert` 现在会生成：
  - `table/w3i.ini`
  - `table/imp.ini`
  - `w3x2lni/locale/w3i.lng`
  - `w3x2lni/locale/lml.lng`
  - `w3x2lni/config.ini`
- `ExtractMapFiles()` 已不再把模型/贴图/声音混放到 `map/`
- `unpack` 现在会自动为工作区补齐对象派生表文件，而不要求这些 `*.ini` 真正存在于归档中
- `pack` 现在不会把 source-only 对象表文件直接回塞到归档
- `slk` 打包现在会把已识别 `TXT/WTS` 文件纳入集中产物集合，而不是完全靠普通文件透传
- `slk.remove_we_only` 现在已开始影响打包前清理结果，WE-only 文件不会再被原样带进 `slk` 输出

仍然存在的不一致：

1. `convert` 仍然把 `.j/.lua/.ai` 直接复制到 `trigger/`。  
   但参考项目里 raw script 与 trigger 语义文件并不是同一层概念。

2. `convert/unpack` 生成的 `table/*.ini` 已经开始承载对象语义，但仍然不是 `external` 等价的完整语义产物。

3. `convert` 与 `unpack` 虽然目录形态更接近了，但高层语义仍未完全统一。

审计结论：

- 这一项已从“第二阶段部分对齐”推进到“第三阶段部分对齐”。
- 但距离 `external` 的稳定布局规则和语义规则仍然有距离。

### 5.10 输入模型：已完成第一阶段部分对齐，但还没有达到 `external` 的统一转换层

参考项目证据：

- `external/script/backend/convert.lua`
- `external/script/backend/cli/pack.lua`
- `external/script/backend/cli/unpack.lua`
- `external/script/map-builder/archive.lua`
- `external/script/map-builder/archive_mpq.lua`
- `external/script/map-builder/archive_dir.lua`

当前项目证据：

- `cli/commands/map_input_utils.cc`
- `cli/commands/analyze_command.cc`
- `cli/commands/extract_command.cc`
- `cli/commands/convert_command.cc`
- `converter/w3x2lni/w3x_to_lni.h`
- `converter/resource_extract/resource_extractor.cc`

本轮已完成的对齐：

- 当前 `convert/extract/analyze` 已不再局限于目录输入。
- `analyze/extract` 现在可直接读取 MPQ。
- `convert` 现在会在接收到 `.w3x/.w3m` 时先临时解包，再复用现有目录转换链。
- `ResourceExtractor` 现在已复用统一 archive abstraction，而不是把 MPQ 视为“暂不支持”。

仍然存在的差异：

- 当前只是把“输入接受能力”对齐到第一阶段。
- `convert` 内部仍不是 `external` 那种围绕统一前端/后端转换模型运行的完整管线。
- 目录输入与 archive 输入虽然都能工作，但还没有完全收敛为同一套高层转换语义。

审计结论：

- 这一项已从“未对齐”提升到“部分对齐”。
- 但如果标准是严格等价 `external`，这里还不能算完成。

### 5.11 GUI 没有对齐，而且现有 GUI 代码存在明显未完成和自相矛盾的地方

当前项目证据：

- `README.md`
- `CMakeLists.txt`
- `gui/main_window/main_window.cc`
- `gui/panels/convert_panel/convert_panel.cc`
- `gui/CMakeLists.txt`

关键事实：

- `README.md` 和 `CMakeLists.txt` 都表明 GUI 默认不交付。
- `main_window.cc` 里 `Open Map`、`Save`、`Convert`、`Extract`、`Analyze` 都还是 TODO。
- `ConvertPanel` 的输入文件对话框只接受 `.w3x/.w3m`，但当前纯 C++ `convert` 主流程又只支持目录输入。

这说明：

- GUI 不只是“还没交付”。
- 连现有 UI 假设都还没有和底层能力对齐。

和参考项目的差异：

- `external/script/gui/new/**`
- `external/script/gui/backend.lua`
- `external/script/gui/event.lua`
- `external/script/gui/new/page/**`

参考项目有完整 GUI 工作流和后端连接层，当前 C++ 版尚无替代。

审计结论：

- GUI 层目前应视为未对齐。

### 5.12 测试深度差距很大：当前只有 smoke tests，参考项目有大量行为边界测试

参考项目证据：

- `external/test/unit_test/**`

当前项目证据：

- `tests/smoke_tests.cc`
- `tests/CMakeLists.txt`
- `README.md`

实测现状：

- 当前仓库测试侧只有 1 个 smoke test 可执行程序。
- `external/test/unit_test/` 下有 22 个单测目录，覆盖大量边界行为。

参考项目覆盖的典型问题：

- `txt` 数据合并
- `misc` 保留/清理/重建
- 非法对象 ID 纠正
- 垃圾数据忽略
- 非法字符串保留到 obj
- ID 冲突回退规则
- 多等级字符串继承规则
- 空数组与 count 清理
- 浮点数误差与清理
- 新旧版本 `WTG -> LML`

当前 C++ 项目没有等价验证资产。

审计结论：

- 这不是“测试少一点”的问题。
- 这是“还没有行为对齐证明能力”的问题。

## 6. 额外发现的内部问题

这部分不是直接拿来和 `external` 对比得出的结论，而是当前仓库内部已经暴露出的风险。

### 6.1 `remove_unused_objects` / `computed_text` / `inline_wts_strings` 仍未参与真实内容级转换，只有 `remove_we_only` 开始进入 `slk` 打包语义

证据：

- `converter/w3x2lni/w3x_to_lni.h`
- `converter/w3x2lni/w3x_to_lni.cc`

现状：

- 选项结构里有这些字段。
- `WriteConfig()` 会把它们写到 `w3x2lni.ini`。
- `convert` 现在会读取分层 INI 配置，并把工作区配置输出到 `w3x2lni/config.ini`。
- `slk.remove_we_only` 现在已开始参与真实打包清理。
- 但 `remove_unused_objects`、`computed_text`、`inline_wts_strings` 仍然没有看到真正的内容级转换分支。

结论：

- 当前有“部分开关已经开始接上，另一部分仍停留在接口层”的情况。

### 6.2 `JASS` parser 已存在，但没有形成真正可交付能力

证据：

- `parser/jass/jass_parser.cc`
- `parser/jass/jass_lexer.cc`
- `README.md`

现状：

- 解析器存在。
- 但没有接到 CLI 的核心转换目标中。
- README 也已明确首版不做 `JASS -> Lua`。

结论：

- 目前更像基础设施储备，而不是对齐完成项。

## 7. 对齐优先级建议

如果你的目标真的是“**严格对齐 `external`，但全部改为纯 C++**”，建议按下面的优先级推进，而不是继续在现有 `convert` 壳层上打补丁。

### P0：必须先解决，否则谈不上“严格对齐”

1. 重新定义目标格式模型  
   先把 `Lni / Obj / Slk` 三种格式在 C++ 版中的输入、输出、落盘形态和互转矩阵定义清楚。

2. 去掉对 `external/` 的运行时依赖  
   把 metadata、template、locale、默认数据版本正式纳入 C++ 自己的数据资产或生成流程。

3. 重做转换主链  
   让 `convert` 不再是“目录搬运器”，而是“基于对象/脚本/地图语义的真实转换器”。

4. 建立统一 archive abstraction  
   让目录和 MPQ 成为同级输入后端，不要让 `convert/extract/analyze` 继续只认目录。

### P1：补齐核心行为差距

1. 完整对象数据支持  
   至少补齐 `w3b/w3d/txt/misc` 及对应规则，明确默认值、继承、合并、丢失信息策略。

2. 触发器转换链  
   补 `WTG/WCT/LML` 的解析与生成，否则与 `external` 的核心能力永远不在一个层级。

3. 工作区布局统一  
   统一 `convert`、`unpack`、`pack` 对 `map/table/trigger/resource/sound/w3x2lni` 的分类规则。

4. 配置/模板/版本数据体系  
   引入与 `external` 等价的配置语义，而不是停留在通用 JSON 容器。

### P2：交付层补齐

1. GUI 工作流重建  
   在底层能力稳定后，再做 GUI 绑定，否则当前 GUI 只会持续与底层能力脱节。

2. 扩大测试集  
   把 `external/test/unit_test/**` 中的行为规则逐条移植成 C++ 回归测试。

3. 文档与产物语义统一  
   把 README、命令 help、实际行为全部收敛到同一套定义上。

## 8. 最终判断

当前仓库可以定义为：

> 一个已经具备纯 C++ 骨架、CLI 外壳、MPQ 基础打包/解包、基础解析器和 smoke tests 的项目。

但它**还不能定义为**：

> 一个已经“严格对齐 `external/` 参考项目”的纯 C++ 版本。

更准确地说，当前状态是：

- `pack/unpack/analyze/extract/convert` 这些名字已经在
- 纯 C++ CLI 已经能跑
- 若干底层解析器已经到位
- 但参考项目真正重要的那层：
  - 三格式模型
  - 真实转换语义
  - 触发器体系
  - 配置/模板/数据版本/语言包/插件
  - 行为级测试资产

  目前都没有完成对齐

所以如果你的要求是“严格对齐 `external`，只是把 Lua 改成 C++”，那么当前项目仍处于**第一阶段原型完成、第二阶段核心对齐尚未开始或只开了头**的状态。

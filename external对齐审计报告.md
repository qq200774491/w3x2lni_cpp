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
- 工作区目录结构：部分对齐
- 真实对象语义转换：未对齐
- `Lni/Obj/Slk` 三格式模型：未对齐
- 触发器 `WTG/WCT/LML` 管线：未对齐
- 配置/模板/多版本数据/语言包/插件：未对齐
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

- `convert`
- `extract`
- `analyze`
- `unpack`
- `pack`

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
  - archive 到 workspace 的 `unpack`
  - workspace/目录到 archive 的 `pack`

审计结论：

- 这是**最根本的未对齐**。
- 当前项目完成的是“文件工作流”，不是“格式体系对齐”。

### 5.2 `convert` 还停留在“目录复制/改后缀”级别，没有对齐参考项目的真实转换语义

当前项目证据：

- `实现计划.md`
- `converter/w3x2lni/w3x_to_lni.cc`
- `converter/w3x2lni/w3x_to_lni.h`

关键事实：

- `实现计划.md` 已明确写出：下一阶段才要把 `w3x -> lni` 从“目录复制 + 基础落盘”升级为真实对象数据转换。
- `W3xToLniConverter::ConvertTableData()` 当前做的是：
  - 扫目录
  - 识别 `.slk/.txt/.w3u/.w3a/...`
  - 读取文本
  - 直接写成 `.ini`
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

- 当前 `convert` 命令**只对齐了名字，没有对齐语义**。

### 5.3 对象数据打包/回包只覆盖一部分，远未对齐参考项目

当前项目证据：

- `parser/w3x/object_pack_generator.cc`
- `parser/w3u/w3u_parser.cc`
- `parser/w3u/w3u_writer.cc`
- `parser/slk/slk_parser.cc`

关键事实：

- 当前仅合成以下对象文件：
  - `war3map.w3a`
  - `war3map.w3h`
  - `war3map.w3u`
  - `war3map.w3t`
  - `war3map.w3q`
  - `war3mapmisc.txt`
- 没有看到对应：
  - `war3map.w3b`
  - `war3map.w3d`
  - `txt.ini` 的完整回写链
  - `doo/w3s/w3r` 等参考项目 TODO/能力边界中的其它地图语义文件

进一步问题：

- 当前 `PackMapDirectory()` 会把工作区中的常规文本文件直接作为归档成员写回，同时再额外合成部分 `war3map.w3*` 文件。
- 这意味着它更像“为了当前工作区回环而生成可运行包”，而不是严格产出 `external` 意义下的标准 `Obj` 或 `Slk` 目标格式。

上面这一点属于从源码做出的合理推断，依据是：

- `parser/w3x/map_archive_io.cc`
- `parser/w3x/object_pack_generator.cc`
- `parser/w3x/workspace_layout.cc`

审计结论：

- 当前对象链路只能算“部分对齐”。
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

### 5.5 命令面没有对齐：当前只有 5 个命令，参考项目有 12 个 CLI 入口

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

差异：

- 当前没有：
  - `obj`
  - `slk`
  - `lni`
  - `config`
  - `template`
  - `test`
  - `version` 的独立子命令
  - `log`
  - `mpq` 型工具命令

审计结论：

- 命令层面对齐度明显不足。
- 这一点不是“文档没写”，而是命令入口本身就不存在。

### 5.6 配置系统没有对齐：当前是通用 JSON 配置壳，不是参考项目的分层 INI 配置体系

参考项目证据：

- `external/config.ini`
- `external/script/share/config.lua`
- `external/script/share/config.ini`
- `external/script/share/config_define.lua`
- `external/script/backend/cli/config.lua`

当前项目证据：

- `core/config/config_manager.h`
- `core/config/config_manager.cc`

差异：

- `external` 的配置是：
  - 默认配置
  - 全局配置
  - 地图内配置
  - 配置项校验/注释/显示规则
  - 与 CLI、语言、数据版本联动
- 当前 C++ 项目只有一个通用 JSON 配置容器，没有形成 `external` 那套配置语义。

审计结论：

- 当前配置系统**不是同一层级的东西**。

### 5.7 模板、预构建索引、多版本数据仓、语言包、插件体系基本都没有对齐

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
- `parser/w3x/object_pack_generator.cc`
- `parser/w3x/workspace_layout.cc`

差异：

- `external` 有完整的数据版本仓、预构建 metadata、模板输出、语言包和插件机制。
- 当前仓库没有对应的纯 C++ 数据加载/管理架构。
- 当前只是在少数地方“读取 `external/` 现成文件”。

审计结论：

- 这块是“参考项目的基础设施层”。
- 当前 C++ 项目几乎还没真正接管。

### 5.8 当前项目实际上仍依赖 `external/` 作为运行时数据来源，和项目自述冲突

这是本次审计发现的一个非常关键的问题。

项目自述证据：

- `README.md` 明确写了：`external/` “仅用于对照功能与行为，不作为新工程的运行时依赖”。

实际源码证据：

- `parser/CMakeLists.txt`
- `parser/w3x/object_pack_generator.cc`
- `parser/w3x/workspace_layout.cc`

关键事实：

- `parser/CMakeLists.txt` 通过编译定义把 `W3X_SOURCE_DIR` 固定到源码根目录。
- `object_pack_generator.cc` 在运行时去读取：
  - `external/data/.../prebuilt/metadata.ini`
  - `external/data/.../prebuilt/Custom/*.ini`
- `workspace_layout.cc` 在运行时去读取：
  - `external/script/locale/zhCN/w3i.lng`
  - `external/script/locale/zhCN/lml.lng`

这意味着：

- 当前二进制并没有真正摆脱 `external/`。
- 如果源码目录结构变化、安装目录变化、`external/` 缺失，相关能力就会失效。

审计结论：

- 这一点既是**设计偏差**，也是**对 README 承诺的违背**。
- 如果目标是纯 C++ 独立实现，这个问题必须优先处理。

### 5.9 工作区分类本身存在内部不一致，说明“external 风格工作区”还没有收敛

当前项目证据：

- `converter/w3x2lni/w3x_to_lni.cc`
- `parser/w3x/workspace_layout.cc`
- `tests/smoke_tests.cc`

关键不一致：

1. `convert` 的输出目录只准备了：
   - `map/`
   - `table/`
   - `trigger/`

   但 `workspace_layout.cc` 明确还支持：
   - `resource/`
   - `sound/`
   - `w3x2lni/`

2. `workspace_layout.cc` 会把资源文件映射到 `resource/`、声音映射到 `sound/`。  
   但 `W3xToLniConverter::ExtractMapFiles()` 会把 `.mdx/.blp/.wav/.mp3` 直接复制到 `map/`。

3. `convert` 会把脚本复制到 `trigger/`。  
   但 `workspace_layout.cc` 对 `war3map*` 和 `scripts/` 的路径归类偏向 `map/`。

审计结论：

- 当前仓库内部对“标准工作区长什么样”都还没有完全统一。
- 这说明它距离“严格对齐 external 的稳定布局规则”还有距离。

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

### 6.1 `remove_unused_objects` / `inline_wts_strings` 选项目前只写进配置，不参与真实转换

证据：

- `converter/w3x2lni/w3x_to_lni.h`
- `converter/w3x2lni/w3x_to_lni.cc`

现状：

- 选项结构里有这些字段。
- `WriteConfig()` 会把它们写到 `w3x2lni.ini`。
- 但转换流程中没有看到真正使用这些选项做行为分支。

结论：

- 当前有“接口看起来像能力已存在，但实现还没接上”的情况。

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

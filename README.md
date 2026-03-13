# w3x2lni_cpp
`w3x2lni_cpp` 是一个面向 Warcraft III 地图工具链的纯 C++ 重写工程。

## 项目目的

原 [w3x2lni](https://github.com/sumneko/w3x2lni) 使用 Lua 实现，本项目使用 C++23 重写，目标是：


当前仓库的首版目标已经收敛为：

- 纯 C++ 实现，不依赖 `lua54`、`luamake` 或 Lua 运行时
- 以 `external/` 原项目为行为参考，不直接复用其 Lua 构建链
- 运行时优先使用仓库内置的 `data/` 元数据与 locale 资源
- 首版只承诺 `Windows x64 + MSVC + CMake`
- 首版交付 `CLI`，`GUI` 默认关闭
- 支持直接解包/压包 `.w3x/.w3m` MPQ 地图，并保留已解包目录工作流

## 当前能力

- `help [command]`
  - 显示全局帮助或单个命令帮助
- `version`
  - 显示 CLI 版本
- `config [--map <workspace_dir>] [section.key[=value]]`
  - 查看或修改纯 C++ 版分层 INI 配置
  - 支持默认配置、全局配置、地图工作区配置三层解析
- `template [output_dir]`
  - 从内置 `data/<version>/prebuilt` 导出 `Melee/Custom` 模板
- `log [--path]`
  - 打印当前日志文件或仅显示日志路径
- `obj <input_map_dir|input_map.w3x|input_map.w3m> [output_map.w3x|output_map.w3m]`
  - 将目录或 packed map 转为当前纯 C++ 可生成的 Obj 风格打包地图
  - 默认输出 `<stem>_obj.w3x`
- `slk <input_map_dir|input_map.w3x|input_map.w3m> [output_map.w3x|output_map.w3m]`
  - 将目录或 packed map 转为第二阶段 Slk 风格打包地图
  - 已支持从 `ability/buff/item/upgrade/destructable/doodad.ini` 生成对应 `SLK`
  - 已支持将 `unit.ini` 保守拆分为 `units/unitui.slk`、`units/unitdata.slk`、`units/unitbalance.slk`、`units/unitabilities.slk`、`units/unitweapons.slk`，并在打包前清理旧的同名 `SLK`
  - 已支持从 `txt.ini` 保守生成一批目标 `TXT`，包括 `units/campaignabilitystrings.txt`、`units/commonabilitystrings.txt`、`units/campaignunitstrings.txt`、`units/itemstrings.txt`、`units/campaignupgradestrings.txt`、`units/itemabilitystrings.txt`、`units/orcunitstrings.txt`、`doodads/doodadskins.txt`
  - 若 `txt.ini` 中字符串无法安全直接落进 `TXT`，会外置到重建后的 `war3map.wts`
  - 已保守接入 `remove_same`：会在打包前按 bundled `prebuilt/Custom/*.ini` 清理 `ability/buff/unit/item/upgrade/doodad/destructable/txt/misc.ini` 中与 default/parent 相同的字段和空段
  - 已保守接入 `remove_unuse_object`：会基于 `war3map.j`/`scripts/war3map.j` 与 bundled `prebuilt/search.ini` 裁掉确认未引用的 custom `ability/buff/upgrade`，并联动移除匹配的 `txt.ini` 段；`unit/item/doodad/destructable` 当前仍保守保留
  - 已保守接入 `computed_text`：会在打包前回写 `ability/item/upgrade` 的 `researchubertip/ubertip/description` 中部分 `<id,key>` 占位符
  - 已集中托管已识别的 `TXT/WTS` 输出，并在 `remove_we_only=true` 时清理 `WTG/WCT/W3R/...` 等 WE-only 文件
  - 当前仍保留 `war3map.w3u` sidecar，不声称已完全用 `SLK` 覆盖单位对象语义
  - 对仅能由 `SLK` 覆盖的对象会抑制冗余 `war3map.w3*`；若存在 `raw(...)` 等当前 `SLK` 无法安全承载的字段，则会保留必要的 `OBJ` sidecar
  - 默认输出 `<stem>_slk.w3x`
- `test`
  - 发现并执行同目录或构建目录中的 `w3x_smoke_tests`
- `unpack <input_map.w3x|input_map.w3m> <output_dir>`
  - 完整解包 MPQ 地图
  - 恢复标准地图文件名、已识别的 `SLK`/`INI` 文件名
  - 无法可靠恢复命名的匿名条目保留在 `anonymous/`
  - 生成 `.w3x_manifest.json` 供回包使用
- `pack <input_dir> <output_map.w3x|output_map.w3m>`
  - 从解包目录重新打回 `.w3x/.w3m`
  - 读取 `.w3x_manifest.json` 恢复匿名条目原始归档名
  - 支持修改后 `SLK`/`INI` 再回包
  - 目前可合成 `war3map.w3a/w3b/w3d/w3h/w3u/w3t/w3q` 与 `war3mapmisc.txt`
  - 对 `ability/destructable/doodad/buff/upgrade/item/unit/misc.ini` 这类对象源文件，回包时不再原样塞进归档
- `convert <input_map_dir|input_map.w3x|input_map.w3m> <output_dir>`
  - 将目录或 packed map 转换为 LNI 工作区布局
  - 输出 `.w3x`、`map/`、`resource/`、`sound/`、`table/`、`trigger/`、`w3x2lni/` 与兼容性的 `w3x2lni.ini`
  - 生成 `table/w3i.ini`、`table/imp.ini`、`w3x2lni/locale/*` 与 `w3x2lni/config.ini`
  - 已支持将 `war3map.w3a/w3b/w3d/w3h/w3q/w3t/w3u` 反向导出为 `ability/destructable/doodad/buff/upgrade/item/unit.ini`
  - 已支持将 `war3mapmisc.txt` 反向导出为 `misc.ini`
  - 对 metadata 中未识别的对象字段，使用 `raw(...)` 语法保留，并支持后续回包
- `lni <input_map_dir|input_map.w3x|input_map.w3m> <output_dir>`
  - `convert` 的 `external` 风格别名入口
- `extract <input_map_dir|input_map.w3x|input_map.w3m> <output_dir>`
  - 按资源类型提取目录或 MPQ 地图中的模型、贴图、声音、脚本、UI、数据或地图文件
- `analyze <input_map_dir|input_map.w3x|input_map.w3m>`
  - 统计目录或 MPQ 地图的资源分类
  - 若存在 `war3map.w3i`，输出地图元数据摘要

## 非目标

- 首版不交付 GUI 功能
- 首版不做 `JASS -> Lua` 转换
- 首版不承诺 Linux/macOS 发布

## 目录结构

```text
app/          CLI 入口
cli/          命令实现
converter/    LNI 转换与资源提取
core/         日志、文件系统、错误、平台能力
data/         内置 prebuilt 元数据与 locale 资源
parser/       Warcraft III 格式解析器
tests/        目录模式 smoke tests
external/     原 Lua 项目参考资料，不参与新构建
```

## 验证状态

- `pack -> unpack -> 修改 SLK -> 再 pack -> 再 unpack` smoke test 已通过
- `packed map -> analyze/extract/convert` smoke test 已通过
- `w3b/w3d` 对象文件生成与回环 smoke test 已通过
- `config/template/log` smoke test 已通过
- `war3map.w3a -> ability.ini -> pack -> unpack` 与未知字段 `raw(...)` 回环 smoke test 已通过
- `obj` 命令与 `pack/unpack` 的对象源过滤 smoke test 已通过
- `slk` 命令对 `SLK` 生成、`TXT/WTS` 托管、WE-only 清理与必要 `OBJ` sidecar 保留的 smoke test 已通过
- `unit.ini -> 5 份 unit*.slk + war3map.w3u sidecar` smoke test 已通过
- `txt.ini -> 多个 TXT + war3map.wts 第一阶段重建` smoke test 已通过
- `remove_same/remove_unuse_object/computed_text` 内容级 cleanup smoke test 已通过
- 对仓库内实图 `雷之呼吸—壹式——霹雳一闪 -.w3x` 已验证：
  - 核心地图文件恢复
  - `ability.ini/unit.ini/upgrade.ini/buff.ini/item.ini/txt.ini/misc.ini` 恢复
  - `units/*.slk` 全链路回包

## 构建

要求：

- Visual Studio 2022
- CMake 3.20+
- C++23 编译器

Windows x64 示例：

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build --build-config Release
```

默认情况下：

- `W3X_BUILD_GUI=OFF`
- `W3X_BUILD_TESTS=ON`
- 安装时会一并打包 `data/` 到可执行文件旁的 `data/`

如需显式关闭 GUI：

```powershell
cmake -S . -B build -A x64 -DW3X_BUILD_GUI=OFF
```

## 参考来源

- 原始工具链参考：`external/`
- 当前运行时会优先从当前工作目录、可执行文件目录和源码目录中解析 `data/`
- `external/` 仅保留为行为参考与兼容回退来源

## 许可证

MIT

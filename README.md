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
- `convert <input_map_dir|input_map.w3x|input_map.w3m> <output_dir>`
  - 将目录或 packed map 转换为 LNI 工作区布局
  - 输出 `.w3x`、`map/`、`resource/`、`sound/`、`table/`、`trigger/`、`w3x2lni/` 与 `w3x2lni.ini`
  - 生成 `table/w3i.ini`、`table/imp.ini` 和 locale 文件
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

如需显式关闭 GUI：

```powershell
cmake -S . -B build -A x64 -DW3X_BUILD_GUI=OFF
```

## 参考来源

- 原始工具链参考：`external/`
- 当前运行时优先使用仓库内 `data/` 的 bundled 资源；`external/` 仅保留为行为参考与兼容回退来源

## 许可证

MIT

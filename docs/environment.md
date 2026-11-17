# 本机环境与版本指纹

本文记录**实测**的环境事实。任何路径、版本号、哈希只能来自这里；如果实测与本文不符，先更新本文并在 `docs/worklog/` 记录差异，再改代码。

最后核实：2026-11-17 20:33 (+08:00)

## 1. 游戏安装

| 项 | 值 |
|---|---|
| 安装根 | `D:\GAMES\.minecraft` |
| 目标版本目录 | `D:\GAMES\.minecraft\versions\26.3-Fabric` |
| 版本 json | `26.3-Fabric.json`，131,567 B |
| 版本 id | `26.3-Fabric`（`type: release`，`releaseTime: 2026-09-15T19:23:02+08:00`） |
| 客户端 jar | `26.3-Fabric.jar`，41,483,720 B |
| 客户端 sha1 | `e877b6a07acd633fb3bb475002175cec036e7b87`（已实测校验，与 json `downloads.client` 一致） |
| 服务端 jar | **缺失**。json `downloads.server` 声明 62,294,556 B，sha1 `33680f5f2ac32864d6d7cf5e56a705fdb3e05f4c` |
| 资源索引 | id `34`，597,035 B，`assets/` 已存在 |
| 库 | `libraries/` 存在，版本 json 列 122 条 |
| Java | 25（`java-runtime-epsilon`） |
| 启动链 | Fabric（`mainClass: net.fabricmc.loader.impl.launch.knot.KnotClient`） |

同机其他版本（仅作差分参考，非目标）：`26.3-rc-2`、`26.4-snapshot-6`、`26.2-Fabric`、`26.1.1-Fabric`、`26.1.2-Fabric`、`1.21.11`。

### 由实测得出的结论

1. **`26.3-Fabric.jar` 是未修改的原版客户端**。实测 sha1 与官方清单相等，说明 Fabric 的补丁位于 `libraries/`，不在该 jar 内。`prepare` 可以直接把它当作客户端资源来源。
2. **S0 门禁被服务端 jar 阻塞**。数据生成器（注册表、标签、配方、战利品表、结构）需要官方服务端 jar，本机没有。处置见 §4。
3. **目标版本是 Fabric 安装而非原版启动链**。这不影响 `prepare` 读取 jar，但意味着：
   - 不能用启动器的 profile 推断原版参数，参数必须来自版本 json；
   - **不得以 Fabric / Yarn 的补丁与映射代码作为行为依据**，那等于绕过 `docs/clean-room.md` 的规格先行要求；这些只允许用于确认"某个行为确实存在于原版"，随后仍须由外部可观测证据支撑规格。
4. `assets/` 与 `libraries/` 报告 0 字节是目录而非文件，`inspect` 只检查存在性。

## 2. 工具链

| 项 | 值 |
|---|---|
| 编译器 | MSVC 19.44.35221（工具集 `14.44.35207`） |
| VS 实例 | `C:\Program Files\Microsoft Visual Studio\2022\Community`；另有 BuildTools 实例 |
| Windows SDK | `10.0.26100.0` |
| CMake | 4.3.1 |
| Ninja | 由 `python -m pip install ninja` 提供，位于 Python 的 `Scripts/` |
| Python | 3.13.12 |
| 构建入口 | `scripts/build.sh`（`scripts/env-msvc.sh` 负责环境变量） |

### 已知的工具链限制

- **CMake 的 Visual Studio 生成器在本机不可用**：两个 VS 实例都未向 CMake 正确注册 C++ 工具集，`project()` 阶段报 `No CMAKE_CXX_COMPILER could be found`。因此统一走 Ninja。
- **`vcvars64.bat` 在受限 shell 中不可用**：它依赖注册表查询，沙箱环境会阻断并导致 `rc.exe`、SDK 路径缺失，链接阶段失败。`scripts/env-msvc.sh` 改为从磁盘布局推导 `INCLUDE` / `LIB` / `PATH`，不依赖注册表。
- 因此本仓库不提供 `.bat` 构建脚本，避免交付未经验证的路径。

## 3. 本地缓存位置

| 内容 | 路径 | 是否入库 |
|---|---|---|
| 构建产物 | `build/` | 否 |
| `prepare` 导出缓存 | `var/prepared/`（可通过 `VELNORIA_DATA_DIR` 覆盖） | 否 |
| 差分夹具（大件） | 本地生成，不入库 | 否 |

`prepare` 读取上述游戏安装，只写 `var/prepared/`，绝不修改游戏目录。

## 4. 待处置：缺失的服务端 jar

数据生成器是 `docs/design/vertical-slice-01.md` S0 的前置。项目不下载、不托管、不代理该文件。

可选路径，需在实施前择一并记录：

| 方案 | 说明 | 代价 |
|---|---|---|
| A 用户自备 | 文档指引用户从官方渠道获取并放到 `versions/26.3-Fabric/server.jar`，`prepare` 只校验 sha1 | 最低，符合现有合规框架 |
| B 启动器引导 | 启动器打开官方下载入口，由用户自行完成下载；校验哈希后使用 | 中等，需处理下载失败与版本漂移 |
| C 不依赖服务端 | 以客户端 jar + 公开数据规格自行构建注册表 | 高，且与"同种子同输出"的 P1 目标冲突 |

默认倾向 A，未定稿前 S0 视为阻塞。

## 5. 复现命令

```bash
bash scripts/build.sh                       # 构建 + 单元测试
./build/tools/prepare/velnoria_prepare.exe inspect
```

`inspect` 的退出码：`0` 输入齐备，`3` 缺少必需输入（当前即 3，因服务端 jar 缺失），`2` 功能未实现。

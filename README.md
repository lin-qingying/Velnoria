# Velnoria

以清洁实现方式重写的客户端核心，目标是与原版 Java 版 26.3 的协议、世界数据与游戏逻辑一致，表现层由 Unreal Engine 承担。

**非官方项目**。与 Mojang Studios、Microsoft、Epic Games 无任何关联。仓库不含任何原版游戏文件、纹理、音效或引擎代码。

## 现状

骨架阶段。已实现：VarInt/VarLong（严格解码）、带边界检查的读取器、帧编解码、包表容器、区块快照结构、NBT 全类型编解码（保序、四类上限）、MD5 与离线 UUID、修改版 UTF-8、协议 String/Position/UUID、握手包体。63 个单元用例通过。

未实现：注册表导出、区块解码、世界生成、光照传播、模拟、渲染、socket 层。

进度以 `docs/worklog/` 为准，最新一条即当前状态。

## 前置条件

| 项 | 要求 |
|---|---|
| 正版游戏 | 本机安装 Java 版 26.3；本项目只读取，不分发 |
| 编译器 | MSVC 19.4x 或 Clang 18+ 或 GCC 13+，C++20 |
| 构建 | CMake 3.24+ |
| UE（仅表现层） | 5.6.0，需用户自行从 Epic 官方渠道安装 |

本机已固定的安装路径见 `docs/environment.md`。

## 构建

```bash
bash scripts/build.sh
```

脚本会配置 Ninja、编译并运行测试。Windows 上使用 MSVC，环境变量由 `scripts/env-msvc.sh` 从磁盘布局推导（不依赖 `vcvars64.bat`，原因见 `docs/environment.md` §2）。

命令行工具：

```bash
./build/tools/prepare/velnoria_prepare.exe inspect
```

退出码：`0` 输入齐备，`3` 缺必需输入，`2` 未实现。

**开工前先读 `AGENT.md`**：一切工作必须记入 `docs/worklog/`，无记录的工作不计入进度。

## 目录

```
core/        纯 C++20，无引擎依赖，唯一的游戏状态权威
ue/          UE 插件（表现层），不参与 CMake 构建
tools/       prepare（数据管线）、capture（抓包）、compare（差分比对）
tests/       unit / property / differential
docs/        design、parity、protocol、worklog
```

## 文档入口

| 文件 | 内容 |
|---|---|
| `AGENT.md` | 工作记录规范（强制） |
| `docs/design/complete-design.md` | 总纲：目标、合规、架构、子系统、测试、风险 |
| `docs/environment.md` | 本机安装路径、版本指纹、已验证事实 |
| `docs/parity/levels.md` | 一致性分级与验收标准 |
| `docs/design/vertical-slice-01.md` | 第一刀切片的范围与门禁 |
| `docs/clean-room.md` | 清洁实现流程与记录要求 |
| `docs/worklog/` | 按日工作记录 |

## 许可

GPL-3.0，见 `LICENSE`。声明见 `NOTICE`。

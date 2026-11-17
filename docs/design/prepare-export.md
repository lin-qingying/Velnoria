# prepare export 规格

| 项目 | 内容 |
|---|---|
| 一致性级别 | P0（数据导出与哈希） |
| 状态 | draft |
| 实现 | `tools/prepare/src/` |
| 依赖 | 官方服务端 jar（用户自备，SHA-1 校验）、JDK 25、本机客户端 jar |

## 1. 输入

| 项 | 来源 | 校验 |
|---|---|---|
| 版本 json | `<root>/versions/<id>/<id>.json` | 必须存在，解析 `downloads.server.sha1` |
| 客户端 jar | `<root>/versions/<id>/<id>.jar` | SHA-1 与 `downloads.client.sha1` 相等 |
| 服务端 jar | `--server-jar`，默认 `var/inputs/server.jar` | SHA-1 与 `downloads.server.sha1` 相等 |
| JDK | `--java`，或 `VELNORIA_JAVA`，或 `PATH` 中的 `java` | 版本 ≥ 25，否则数据生成器报类版本错误 |

任一校验失败即中止，退出码 4，不写入任何输出。

## 2. 步骤

1. 校验上表全部输入。
2. 在 `var/build/<version>/` 下运行数据生成器：
   `java -DbundlerMainClass=net.minecraft.data.Main -jar <server.jar> --reports`
3. 解析 `generated/reports/packets.json` 与 `blocks.json`。
4. 写出三个文件到 `var/prepared/<version>/`。
5. 计算输出 SHA-1，写入 `manifest.txt`。

生成器崩溃或输出缺失即中止，退出码 4，已写入的部分输出删除。

## 3. 输出格式

全部为 UTF-8、LF 行尾、制表符分隔、无 BOM。

### packets.tsv

```
# velnoria packets v1
handshake	serverbound	minecraft:intention	0
status	clientbound	minecraft:status_response	0
play	clientbound	minecraft:bundle_delimiter	0
```

- 列：state、bound、包名（含命名空间）、协议 ID（十进制）。
- 排序：state 按 handshake/status/login/configuration/play，bound 按 clientbound/serverbound，再按 ID 升序。排序是为了让输出与 JSON 键序无关。
- 包名保留 `minecraft:` 前缀，不裁剪。

### block_states.tsv

```
# velnoria block_states v1
0	minecraft:air	
12514	minecraft:acacia_button	face=floor,facing=north,powered=true
```

- 列：全局状态 ID、方块名、属性串（`k=v` 以逗号连接，空属性留空列）。
- 属性顺序按 `blocks.json` 中该状态的属性对象顺序，不重排、不排序。
- 排序：状态 ID 升序。
- 必须覆盖 `blocks.json` 中的全部状态，连续无空洞由校验保证。

### manifest.txt

```
format = 1
version = 26.3
client_sha1 = ...
server_sha1 = ...
packet_count = 233
block_state_count = 26114
packets_sha1 = ...
block_states_sha1 = ...
```

- `key = value`，一行一项，键序固定。
- 不含时间戳：同一输入两次运行必须产出完全相同的字节（S0 验收项）。生成时间记在工作日志，不写进产物。

## 4. 确定性要求

- 输出不得依赖 JSON 对象的遍历顺序：所有表按显式键排序。
- 属性串取自源数据顺序，源数据稳定则输出稳定。
- 浮点、布尔等一律以原文写出，不做数值归一化。

## 5. 不做什么

- 不写注册表全量（除方块状态外）、不写战利品表、不写结构：后续按需增量扩展，格式版本号加 1。
- 不改动游戏安装目录。
- 不缓存或提交任何生成物：`var/` 在 `.gitignore` 内。

## 6. 验收

| 用例 | 判定 |
|---|---|
| 两次运行 | 三个文件逐字节相同 |
| 输入篡改 | 改一字节的 server.jar → 退出码 4，无输出 |
| 状态连续性 | `block_states.tsv` 的 ID 集合等于 `[0, max]` |
| 包表加载 | `PacketTable` 载入后 `id_of` 与 `packets.tsv` 一致 |
| 与内联夹具比对 | 提交的小样本夹具解析结果一致 |

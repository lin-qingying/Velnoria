# NBT 线格式

| 项目 | 内容 |
|---|---|
| 一致性级别 | P0（字节一致） |
| 状态 | implemented |
| 实现 | `core/include/lumenvox/nbt/{tag,io}.hpp`、`core/src/{nbt_tag,nbt_io}.cpp` |
| 规格与实现顺序 | **同批提交**，违反规格先行要求，见 `docs/worklog/2026-11-17.md` |

## 标签类型

| 值 | 名称 | 载荷 |
|---|---|---|
| 0 | end | 无（仅作终止符） |
| 1 | byte | i8 |
| 2 | short | i16 大端 |
| 3 | int | i32 大端 |
| 4 | long | i64 大端 |
| 5 | float | IEEE754 单精度，大端位模式 |
| 6 | double | IEEE754 双精度，大端位模式 |
| 7 | byte_array | i32 长度 + i8×n |
| 8 | string | u16 码元数 + 修改版 UTF-8 |
| 9 | list | 元素类型 u8 + i32 数量 + 元素载荷（无名字） |
| 10 | compound | 重复 {类型 u8, 名字, 载荷} 直至 `00` |
| 11 | int_array | i32 长度 + i32×n |
| 12 | long_array | i32 长度 + i64×n |

根标签 = 类型 u8 + 名字 + 载荷。空根（`00`）合法，表示空文档。

## 影响字节一致的三处细节

1. **Compound 顺序**：条目按写入顺序保留，不排序、不去重。语义相同的两个顺序产生不同字节，重排会破坏往返。
2. **空 List 的元素类型**：`09 <类型> 00 00 00 00`。类型字节即使列表为空也存在，必须保留。
3. **字符串长度前缀**：计 UTF-16 码元数，不计字节数；含 `U+0000` 与增补平面字符时两者不同。

## 上限

| 项 | 默认 | 越界错误 |
|---|---|---|
| 嵌套深度 | 512 | `value_out_of_range` |
| 总字节预算 | 8 MiB | `frame_too_large` |
| 单字符串码元数 | 32767 | `value_out_of_range` |
| 集合元素数 | 1,048,576 | `value_out_of_range` |
| 负长度 | — | `malformed` |

预算按"自解析起点起累计消费的字节数"计算，在每次定长读取与每个集合元素后检查，不是只在入口检查一次——否则一个 `long_array` 可以在一次分配里绕过预算。

## 错误行为

| 输入 | 期望 |
|---|---|
| 截断 | `truncated`，不崩溃 |
| 未定义类型值（>12） | `malformed` |
| 负数长度 | `malformed` |
| 非空列表声明为 end 类型 | `malformed` |
| 写入时列表元素类型与声明不符 | `malformed` |
| 写入时 compound 条目为 end 类型 | `malformed` |

## 验证

| 用例 | 内容 |
|---|---|
| `nbt_encodes_expected_bytes` | 手算字节序列逐字节比对，再解码、再编码，三次相等 |
| `nbt_preserves_compound_entry_order` | 逆序键名保持原序 |
| `nbt_keeps_element_type_of_empty_list` | 空 int 列表的类型字节保留 |
| `nbt_roundtrips_nul_inside_string` | 含 NUL 的字符串，`C0 80` 编码 |
| `nbt_roundtrips_all_array_and_numeric_types` | 全部标签类型往返 |
| `nbt_enforces_*` | 深度、字符串、字节预算、负长度、未知类型 |

## 未覆盖

- GZIP 包裹（`.dat` 存档、部分注册表下发）：需解压后端，切片 S3 前补齐。
- 修改版 UTF-8 中孤立代理项的原样保留（当前降级为 `U+FFFD`）：仅影响损坏数据，不进入差分用例。
- `int_array` / `long_array` 的追加长度上限与 `max_collection` 共用，后续按实测调整。

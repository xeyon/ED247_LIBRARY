
# libxml2 至 expat 依赖迁移
迁移已全部完成并通过验证。

## 改动内容

**新增 expat 解析层**
- [src/ed247/ed247_xml_parser.h](src/ed247/ed247_xml_parser.h) / [.cpp](src/ed247/ed247_xml_parser.cpp)：expat 是流式(SAX)解析器、没有 DOM、没有 XPath、不支持 XSD、不做命名空间处理，因此这里在 expat 回调之上自建了一个轻量 DOM（`ed247::xml::dom::Node/Attribute/Document`），元素与属性统一存**本地名**（去掉前缀），并过滤 `xmlns`/`xmlns:prefix` 声明。文件按 64 KB 分块喂给解析器，错误信息带文件名/行/列。

**加载器改写**（[src/ed247/ed247_xml.cpp](src/ed247/ed247_xml.cpp)、[ed247_xml.h](src/ed247/ed247_xml.h)）
- 删除了 `xmlNodePtr` 与全部 libxml2 调用，`node_ptr_t` 改为指向自建 DOM 节点；`xml_get_prop`/`xmlNode_get_fileline` 等辅助函数改为操作新 DOM。
- 按约定**去掉了 XSD 校验**，并在语义分析中补上原先由 schema 承担的检查：枚举值（ComponentType/StandardRevision/YesNo/Direction/NADType）、数字格式（拒绝 `2589a` 这类尾随垃圾）、IPv4 地址格式、`DstIP` 必填、`DstPort`/`MulticastTTL` 非 0、根元素名必须是 `ED247ComponentInstanceConfiguration`。每处都写了注释说明"这里替代了原来的 XSD 校验"。
- `SrcIP`/`SrcPort` 刻意不校验，以与原 schema 保持一致（端口 0 表示由系统选择）。

**chatbot**（[utils/chatbot/chatbot.cpp](utils/chatbot/chatbot.cpp)）：原来的 XPath `/ComponentMessagesDescription/A429_Bus` 改为按文档路径匹配；CMD 解析、错误处理均已改用 expat，并输出行列号。

**构建/文档**：`find_package(EXPAT REQUIRED)` + `EXPAT::EXPAT`（根 [CMakeLists.txt](CMakeLists.txt)、[src/ed247/CMakeLists.txt](src/ed247/CMakeLists.txt)、[utils/chatbot/CMakeLists.txt](utils/chatbot/CMakeLists.txt)）；删除了把 XSD 编译进库的 `_generate_xsd_header` 宏（schema 仅作为参考安装）；[README.md](README.md)、[LICENSE.md](LICENSE.md)（第三方库列表 LIBXML2 → EXPAT）、QNX toolchain 示例同步更新。

## 验证结果
- CMake 配置通过（`Found EXPAT`）；真实构建中 `ed247_xml.cpp` / `ed247_xml_parser.cpp` **零错误零警告**（剩余报错全在我未改动的文件里：`ed247_logs.cpp`、`ed247_time.cpp`、`ed247_cominterface.cpp`、`ed247_stream.cpp` 的既有 MSVC 兼容问题）。
- 110 个 ECIC 配置冒烟测试：105 个通过，5 个失败——正是 `tests/func_load_all`、`tests/func_bidirectional` 预期必须失败的无效配置。
- 内容解析用例 9 项全通过（合法配置 / 非 XML / 未知属性 / 非法标准版本 / 非法枚举 / 非法数字 / 非法 IP / 未知根元素 / 文件不存在），错误信息均带精确位置。
- chatbot 的 CMD 解析：2 条总线及其标签正确读出；缺 `Name`、缺 `SDI`、XML 格式错误、文件不存在四种情况均被拒绝并给出明确原因。

一点说明：环境离线且没有 expat 开发包，本地编译/运行验证用的是我自己写的、与 expat 公开 API 兼容的测试桩（仅放在仓库外的临时目录）；生产代码只使用了 expat 官方文档中的公开接口（`XML_ParserCreate`/`XML_Parse`/`XML_SetElementHandler` 等），正式环境链接真实 expat 即可。